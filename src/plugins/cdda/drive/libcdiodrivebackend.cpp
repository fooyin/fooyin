/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libcdiodrivebackend.h"

#include "cddadrivesettings.h"
#include "cddatoc.h"

#include <cdio/cdtext.h>
#include <cdio/device.h>
#include <cdio/paranoia/paranoia.h>
#include <cdio/read.h>

#include <atomic>
#include <optional>

using namespace Qt::StringLiterals;

constexpr auto MaxSectorsPerRead = 16;
constexpr auto StandardRetries   = 5;
constexpr auto ParanoidRetries   = 20;

namespace Fooyin::Cdda {
namespace {
enum class CddaError : int
{
    PermissionDeniedIoctl = -102,
    PermissionDeniedData  = -103,
    TrackNotAudio         = -402,
    NoAudioTracks         = -403,
    NoMedia               = -404,
};

QString cdTextValue(const cdtext_t* cdText, cdtext_field_t field, int track)
{
    const char* value = cdtext_get_const(cdText, field, static_cast<track_t>(track));
    return value ? QString::fromUtf8(value).trimmed() : QString{};
}

CdTextFields cdTextFields(const cdtext_t* cdText, int track)
{
    return {.title     = cdTextValue(cdText, CDTEXT_FIELD_TITLE, track),
            .performer = cdTextValue(cdText, CDTEXT_FIELD_PERFORMER, track),
            .genre     = cdTextValue(cdText, CDTEXT_FIELD_GENRE, track),
            .composer  = cdTextValue(cdText, CDTEXT_FIELD_COMPOSER, track),
            .message   = cdTextValue(cdText, CDTEXT_FIELD_MESSAGE, track),
            .isrc      = cdTextValue(cdText, CDTEXT_FIELD_ISRC, track)};
}

CdDriveInfo driveInfo(CdIo_t* cdio, const QString& device)
{
    QString vendor;
    QString model;
    QString revision;
    bool supportsSpeedLimit{false};

    cdio_hwinfo_t hardware{};
    if(cdio_get_hwinfo(cdio, &hardware)) {
        vendor   = QString::fromLatin1(hardware.psz_vendor).trimmed();
        model    = QString::fromLatin1(hardware.psz_model).trimmed();
        revision = QString::fromLatin1(hardware.psz_revision).trimmed();
    }

    cdio_drive_read_cap_t readCapabilities{};
    cdio_drive_write_cap_t writeCapabilities{};
    cdio_drive_misc_cap_t miscCapabilities{};
    cdio_get_drive_cap(cdio, &readCapabilities, &writeCapabilities, &miscCapabilities);
    supportsSpeedLimit = miscCapabilities & CDIO_DRIVE_CAP_MISC_SELECT_SPEED;

    const QString label = QStringList{vendor, model}.join(u' ').simplified();
    return {.id                 = device,
            .displayName        = label.isEmpty() ? device : label,
            .settingsKey        = cdDriveSettingsKey(u"libcdio"_s, vendor, model, device),
            .supportsSpeedLimit = supportsSpeedLimit,
            .vendor             = vendor,
            .model              = model,
            .revision           = revision};
}

CdError driveError(CdDriveError category, int platformCode, QString message, const QString& detail)
{
    if(!detail.isEmpty()) {
        message += u'\n';
        message += LibcdioDriveSession::tr("Details: %1").arg(detail.trimmed());
    }

    if(category == CdDriveError::AccessDenied) {
        message += u'\n';
        message += LibcdioDriveSession::tr("Check the disk drive device permissions.");
    }

    return {
        .code         = category,
        .message      = message,
        .platformCode = platformCode,
    };
}

CdError cdioError(driver_return_code_t code, QString message, QString detail = {})
{
    CdDriveError category{CdDriveError::ReadFailed};

    switch(code) {
        case DRIVER_OP_NOT_PERMITTED:
            category = CdDriveError::AccessDenied;
            break;

        case DRIVER_OP_UNSUPPORTED:
        case DRIVER_OP_NO_DRIVER:
            category = CdDriveError::Unsupported;
            break;

        default:
            break;
    }

    if(detail.isEmpty()) {
        detail = QString::fromLocal8Bit(cdio_driver_errmsg(code));
    }

    return driveError(category, static_cast<int>(code), std::move(message), detail);
}

CdError cddaError(int code, QString message, const QString& detail = {})
{
    CdDriveError category{CdDriveError::ReadFailed};

    switch(static_cast<CddaError>(code)) {
        case CddaError::PermissionDeniedIoctl:
        case CddaError::PermissionDeniedData:
            category = CdDriveError::AccessDenied;
            break;

        case CddaError::TrackNotAudio:
        case CddaError::NoAudioTracks:
            category = CdDriveError::NotAudioDisc;
            break;

        case CddaError::NoMedia:
            category = CdDriveError::NoMedia;
            break;

        default:
            break;
    }

    return driveError(category, code, std::move(message), detail);
}

QString takeDriveMessages(cdrom_drive_t* drive)
{
    if(!drive) {
        return {};
    }
    char* errors = cdio_cddap_errors(drive);
    if(!errors) {
        return {};
    }
    const QString result = QString::fromLocal8Bit(errors).trimmed();
    cdio_cddap_free_messages(errors);
    return result;
}

struct ParanoiaDiagnostics
{
    int corrections{0};
    int readErrors{0};
    int skipped{0};
};

class ScopedParanoiaDiagnostics
{
public:
    explicit ScopedParanoiaDiagnostics(ParanoiaDiagnostics& diagnostics)
        : m_previous{std::exchange(m_activeScope, this)}
        , m_diagnostics{diagnostics}
    { }

    ~ScopedParanoiaDiagnostics()
    {
        m_activeScope = m_previous;
    }

    ScopedParanoiaDiagnostics(const ScopedParanoiaDiagnostics&)            = delete;
    ScopedParanoiaDiagnostics& operator=(const ScopedParanoiaDiagnostics&) = delete;

    static void callback(long /*position*/, paranoia_cb_mode_t mode)
    {
        if(!m_activeScope) {
            return;
        }

        switch(mode) {
            case PARANOIA_CB_FIXUP_EDGE:
            case PARANOIA_CB_FIXUP_ATOM:
            case PARANOIA_CB_FIXUP_DROPPED:
            case PARANOIA_CB_FIXUP_DUPED:
                ++m_activeScope->m_diagnostics.corrections;
                break;
            case PARANOIA_CB_READERR:
            case PARANOIA_CB_CACHEERR:
                ++m_activeScope->m_diagnostics.readErrors;
                break;
            case PARANOIA_CB_SKIP:
                ++m_activeScope->m_diagnostics.skipped;
                break;
            default:
                break;
        }
    }

private:
    inline static thread_local ScopedParanoiaDiagnostics* m_activeScope{nullptr};

    ScopedParanoiaDiagnostics* m_previous;
    ParanoiaDiagnostics& m_diagnostics;
};
} // namespace

std::vector<CdDriveInfo> LibcdioDriveBackend::drives()
{
    std::vector<CdDriveInfo> result;
    char** devices = cdio_get_devices(DRIVER_DEVICE);
    if(!devices) {
        return result;
    }

    for(char* const* current = devices; *current; ++current) {
        const QString device = QString::fromLocal8Bit(*current);
        result.push_back({.id                 = device,
                          .displayName        = device,
                          .settingsKey        = cdDriveSettingsKey(u"libcdio"_s, {}, {}, device),
                          .supportsSpeedLimit = false,
                          .vendor             = {},
                          .model              = {},
                          .revision           = {}});
    }
    cdio_free_device_list(devices);
    return result;
}

std::expected<CdDriveBackend::OpenedDrive, CdError> LibcdioDriveBackend::open(const QString& driveId)
{
    if(driveId.isEmpty()) {
        return std::unexpected(CdError{.code         = CdDriveError::Unsupported,
                                       .message      = tr("The selected optical drive is unavailable"),
                                       .platformCode = 0});
    }

    const QByteArray device = driveId.toLocal8Bit();
    CdIo_t* cdio            = cdio_open(device.constData(), DRIVER_UNKNOWN);
    if(!cdio) {
        return std::unexpected(CdError{.code         = CdDriveError::Unsupported,
                                       .message      = tr("The selected optical drive is unavailable: %1").arg(driveId),
                                       .platformCode = 0});
    }
    return OpenedDrive{.drive   = driveInfo(cdio, driveId),
                       .session = std::make_unique<LibcdioDriveSession>(cdio, driveId)};
}

LibcdioDriveSession::LibcdioDriveSession(CdIo_t* cdio, QString device)
    : m_cdio{cdio}
    , m_drive{nullptr}
    , m_paranoia{nullptr}
    , m_nextParanoiaSector{-1}
    , m_device{std::move(device)}
    , m_cancelled{false}
{ }

LibcdioDriveSession::~LibcdioDriveSession()
{
    if(m_paranoia) {
        cdio_paranoia_free(m_paranoia);
    }
    if(m_drive) {
        cdio_cddap_close(m_drive);
    }
    else if(m_cdio) {
        cdio_destroy(m_cdio);
    }
}

std::expected<CdToc, CdError> LibcdioDriveSession::readToc()
{
    if(m_cancelled.load()) {
        return std::unexpected(cancelledError());
    }

    const CdIo_t* cdio = cdioHandle();
    if(!cdio) {
        return std::unexpected(cdioError(DRIVER_OP_ERROR, tr("Failed to access audio CD drive %1").arg(m_device)));
    }

    CdToc toc;
    toc.firstTrackNumber = cdio_get_first_track_num(cdio);
    toc.lastTrackNumber  = cdio_get_last_track_num(cdio);

    const lsn_t leadout = cdio_get_track_lsn(cdio, CDIO_CDROM_LEADOUT_TRACK);
    if(toc.firstTrackNumber == CDIO_INVALID_TRACK || toc.lastTrackNumber == CDIO_INVALID_TRACK
       || leadout == CDIO_INVALID_LSN) {
        return std::unexpected(cdioError(DRIVER_OP_ERROR, tr("Failed to read the CD TOC from %1").arg(m_device),
                                         takeDriveMessages(m_drive)));
    }

    toc.leadoutSector = leadout;

    for(int number{toc.firstTrackNumber}; number <= toc.lastTrackNumber; ++number) {
        const auto track        = static_cast<track_t>(number);
        const lsn_t firstSector = cdio_get_track_lsn(cdio, track);
        const lsn_t nextSector
            = number < toc.lastTrackNumber ? cdio_get_track_lsn(cdio, static_cast<track_t>(number + 1)) : leadout;
        if(firstSector == CDIO_INVALID_LSN || nextSector == CDIO_INVALID_LSN) {
            return std::unexpected(
                cdioError(DRIVER_OP_ERROR, tr("Failed to read a CD track boundary from %1").arg(m_device)));
        }
        toc.tracks.push_back({.number             = number,
                              .firstSector        = firstSector,
                              .endSectorExclusive = nextSector,
                              .isAudio            = cdio_get_track_format(cdio, track) == TRACK_FORMAT_AUDIO});
    }

    if(const auto valid = validateToc(toc); !valid) {
        return std::unexpected(
            CdError{.code = CdDriveError::NotAudioDisc, .message = invalidTocUserMessage(), .platformCode = 0});
    }

    return toc;
}

CdText LibcdioDriveSession::readCdText(const CdToc& toc)
{
    CdIo_t* cdio = cdioHandle();
    if(!cdio) {
        return {};
    }

    const cdtext_t* cdText = cdio_get_cdtext(cdio);
    if(!cdText) {
        return {};
    }

    CdText result;
    result.disc = cdTextFields(cdText, 0);

    for(const CdTocTrack& track : toc.tracks) {
        CdTextFields fields = cdTextFields(cdText, track.number);
        if(!fields.empty()) {
            result.tracks.emplace(track.number, std::move(fields));
        }
    }

    return result;
}

std::expected<CdSectorRead, CdError> LibcdioDriveSession::readAudioSectors(int firstSector, int count)
{
    if(m_cancelled.load()) {
        return std::unexpected(cancelledError());
    }
    if(const auto valid = validateSectorReadRequest(firstSector, count, MaxSectorsPerRead); !valid) {
        return std::unexpected(valid.error());
    }
    if(count == 0) {
        return CdSectorRead{};
    }

    const CdIo_t* cdio = cdioHandle();
    if(!cdio) {
        return std::unexpected(cdioError(DRIVER_OP_ERROR, tr("Failed to access audio CD drive %1").arg(m_device)));
    }

    CdSectorRead result{.pcm = QByteArray(count * BytesPerSector, Qt::Uninitialized), .sectorsRead = count};

    const driver_return_code_t status
        = cdio_read_audio_sectors(cdio, result.pcm.data(), firstSector, static_cast<uint32_t>(count));
    if(status != DRIVER_OP_SUCCESS) {
        return std::unexpected(cdioError(status, tr("Failed to read CD audio sectors from %1").arg(m_device)));
    }

    return result;
}

std::expected<CdSectorRead, CdError> LibcdioDriveSession::readAudioSectorsSecure(int firstSector, int count,
                                                                                 CdRippingSecurity security)
{
    if(security == CdRippingSecurity::Disabled) {
        return readAudioSectors(firstSector, count);
    }
    if(m_cancelled.load()) {
        return std::unexpected(cancelledError());
    }
    if(const auto valid = validateSectorReadRequest(firstSector, count, MaxSectorsPerRead); !valid) {
        return std::unexpected(valid.error());
    }
    if(count == 0) {
        return CdSectorRead{};
    }
    if(const auto initialized = ensureParanoiaDrive(); !initialized) {
        return std::unexpected(initialized.error());
    }

    if(!m_paranoia) {
        m_paranoia = cdio_paranoia_init(m_drive);
        if(!m_paranoia) {
            return std::unexpected(cdioError(DRIVER_OP_ERROR,
                                             tr("Failed to initialise secure CD extraction for %1").arg(m_device),
                                             takeDriveMessages(m_drive)));
        }
    }

    const int mode           = security == CdRippingSecurity::Standard ? PARANOIA_MODE_VERIFY | PARANOIA_MODE_OVERLAP
                                                                       : PARANOIA_MODE_FULL & ~PARANOIA_MODE_NEVERSKIP;
    const int retries        = security == CdRippingSecurity::Standard ? StandardRetries : ParanoidRetries;
    const bool policyChanged = !m_paranoiaSecurity || *m_paranoiaSecurity != security;
    if(policyChanged) {
        cdio_paranoia_modeset(m_paranoia, mode);
    }
    if(policyChanged || m_nextParanoiaSector != firstSector) {
        if(cdio_paranoia_seek(m_paranoia, firstSector, SEEK_SET) < 0) {
            return std::unexpected(cdioError(DRIVER_OP_ERROR,
                                             tr("Failed to seek the secure CD reader for %1").arg(m_device),
                                             takeDriveMessages(m_drive)));
        }
    }

    m_paranoiaSecurity   = security;
    m_nextParanoiaSector = firstSector;

    ParanoiaDiagnostics diagnostics;
    CdSectorRead result;
    result.pcm.reserve(count * BytesPerSector);
    {
        const ScopedParanoiaDiagnostics scopedDiagnostics{diagnostics};
        for(int index{0}; index < count; ++index) {
            if(m_cancelled.load()) {
                return std::unexpected(cancelledError());
            }
            const int16_t* pcm = cdio_paranoia_read_limited(m_paranoia, ScopedParanoiaDiagnostics::callback, retries);
            if(!pcm) {
                return std::unexpected(cdioError(DRIVER_OP_ERROR,
                                                 tr("Failed to read securely from audio CD drive %1").arg(m_device),
                                                 takeDriveMessages(m_drive)));
            }
            result.pcm.append(reinterpret_cast<const char*>(pcm), BytesPerSector);
            ++result.sectorsRead;
            ++m_nextParanoiaSector;
        }
    }

    if(diagnostics.corrections > 0) {
        m_warnings.push_back(
            tr("CD extraction corrected %Ln read inconsistency event(s)", nullptr, diagnostics.corrections));
    }
    if(diagnostics.readErrors > 0) {
        m_warnings.push_back(tr("CD extraction encountered %Ln read error event(s)", nullptr, diagnostics.readErrors));
    }
    if(diagnostics.skipped > 0) {
        m_warnings.push_back(tr("CD extraction exhausted retries and concealed %Ln unreadable sector event(s)", nullptr,
                                diagnostics.skipped));
    }

    return result;
}

std::expected<void, CdError> LibcdioDriveSession::setReadSpeed(int speed)
{
    if(speed <= 0) {
        return std::unexpected(CdError{.code         = CdDriveError::Unsupported,
                                       .message      = tr("The requested CD read-speed limit is invalid"),
                                       .platformCode = EINVAL});
    }

    const CdIo_t* cdio = cdioHandle();
    if(!cdio) {
        return std::unexpected(
            cdioError(DRIVER_OP_ERROR, tr("Failed to limit the CD read speed for %1").arg(m_device)));
    }

    const driver_return_code_t status = cdio_set_speed(cdio, speed);
    if(status != DRIVER_OP_SUCCESS) {
        return std::unexpected(cdioError(status, tr("Failed to limit the CD read speed for %1").arg(m_device)));
    }

    return {};
}

QStringList LibcdioDriveSession::takeWarnings()
{
    return std::exchange(m_warnings, {});
}

void LibcdioDriveSession::cancel()
{
    m_cancelled.store(true);
}

CdIo_t* LibcdioDriveSession::cdioHandle() const
{
    return m_drive ? m_drive->p_cdio : m_cdio;
}

std::expected<void, CdError> LibcdioDriveSession::ensureParanoiaDrive()
{
    if(m_drive) {
        return {};
    }
    if(!m_cdio) {
        return std::unexpected(cdioError(DRIVER_OP_ERROR, tr("Failed to access audio CD drive %1").arg(m_device)));
    }

    char* messages{nullptr};
    cdrom_drive_t* drive = cdio_cddap_identify_cdio(m_cdio, CDDA_MESSAGE_LOGIT, &messages);
    const QString detail = messages ? QString::fromLocal8Bit(messages).trimmed() : QString{};
    if(messages) {
        cdio_cddap_free_messages(messages);
    }
    if(!drive) {
        return std::unexpected(
            cdioError(DRIVER_OP_ERROR, tr("Failed to initialise secure CD extraction for %1").arg(m_device), detail));
    }

    // The cdda drive owns the CdIo handle after successfully identifying
    m_cdio = nullptr;

    const int opened = cdio_cddap_open(drive);
    if(opened != 0) {
        const QString openDetail = takeDriveMessages(drive);
        cdio_cddap_close(drive);
        return std::unexpected(
            cddaError(opened, tr("Failed to initialise secure CD extraction for %1").arg(m_device), openDetail));
    }

    m_drive = drive;
    return {};
}

CdError LibcdioDriveSession::cancelledError()
{
    return {.code = CdDriveError::Cancelled, .message = tr("CD operation was cancelled"), .platformCode = ECANCELED};
}
} // namespace Fooyin::Cdda
