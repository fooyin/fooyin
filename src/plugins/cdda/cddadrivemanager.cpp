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

#include "cddadrivemanager.h"

#include "cddatoc.h"
#include "cddaurl.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
struct CdDriveAccessState
{
    std::mutex mutex;
    std::unordered_set<QString> reservedDrives;
};

class CdDriveManagerPrivate
{
public:
    explicit CdDriveManagerPrivate(std::unique_ptr<CdDriveBackend> backend, std::chrono::milliseconds cacheLifetime)
        : m_backend{std::move(backend)}
        , m_cacheLifetime{cacheLifetime}
        , m_accessState{std::make_shared<CdDriveAccessState>()}
    { }

    std::unique_ptr<CdDriveBackend> m_backend;
    std::chrono::milliseconds m_cacheLifetime;
    std::shared_ptr<CdDriveAccessState> m_accessState;
    std::mutex m_mutex;
    std::unordered_map<QString, CdDriveObservation> m_cache;
    std::unordered_map<QString, QString> m_preferredDrives;
    std::chrono::steady_clock::time_point m_lastRefresh;
    uint64_t m_nextGeneration{1};
    bool m_cacheValid{false};
};

namespace {
CdError driveBusyError()
{
    return {.code         = CdDriveError::DriveBusy,
            .message      = CdDriveManager::tr("The CD drive is already in use"),
            .platformCode = 0};
}

CdDriveObservation errorObservation(CdDriveInfo drive, CdError error)
{
    CdDriveObservation observation;
    observation.drive = std::move(drive);
    observation.error = std::move(error);
    return observation;
}

class DriveReservation
{
public:
    DriveReservation(std::shared_ptr<CdDriveAccessState> state, QString driveId)
        : m_state{std::move(state)}
        , m_driveId{std::move(driveId)}
    { }

    ~DriveReservation()
    {
        release();
    }

    DriveReservation(const DriveReservation&)            = delete;
    DriveReservation& operator=(const DriveReservation&) = delete;

    DriveReservation(DriveReservation&& other) noexcept
        : m_state{std::move(other.m_state)}
        , m_driveId{std::move(other.m_driveId)}
    { }

    DriveReservation& operator=(DriveReservation&& other) noexcept
    {
        if(this != &other) {
            release();
            m_state   = std::move(other.m_state);
            m_driveId = std::move(other.m_driveId);
        }
        return *this;
    }

    std::pair<std::shared_ptr<CdDriveAccessState>, QString> take()
    {
        auto ownership = std::pair{std::move(m_state), std::move(m_driveId)};
        m_state.reset();
        m_driveId.clear();
        return ownership;
    }

private:
    void release()
    {
        if(m_state && !m_driveId.isEmpty()) {
            const std::scoped_lock lock{m_state->mutex};
            m_state->reservedDrives.erase(m_driveId);
        }
        m_driveId.clear();
        m_state.reset();
    }

    std::shared_ptr<CdDriveAccessState> m_state;
    QString m_driveId;
};

class ReservedDriveSession
{
public:
    ReservedDriveSession(DriveReservation reservation, std::expected<CdDriveBackend::OpenedDrive, CdError> opened)
        : m_reservation{std::move(reservation)}
        , m_opened{std::move(opened)}
    { }

    ~ReservedDriveSession()
    {
        if(m_opened) {
            m_opened->session.reset();
        }
    }

    ReservedDriveSession(const ReservedDriveSession&)            = delete;
    ReservedDriveSession& operator=(const ReservedDriveSession&) = delete;
    ReservedDriveSession(ReservedDriveSession&&) noexcept        = default;
    ReservedDriveSession& operator=(ReservedDriveSession&&)      = delete;

    std::expected<CdDriveBackend::OpenedDrive, CdError>& opened()
    {
        return m_opened;
    }

    DriveReservation takeReservation()
    {
        return std::move(m_reservation);
    }

private:
    // The session is reset in the destructor before this reservation is released
    DriveReservation m_reservation;
    std::expected<CdDriveBackend::OpenedDrive, CdError> m_opened;
};

std::expected<DriveReservation, CdError> reserveDrive(const std::shared_ptr<CdDriveAccessState>& state,
                                                      const QString& driveId)
{
    const std::scoped_lock lock{state->mutex};

    if(state->reservedDrives.contains(driveId)) {
        return std::unexpected(driveBusyError());
    }

    state->reservedDrives.emplace(driveId);
    return DriveReservation{state, driveId};
}

std::expected<ReservedDriveSession, CdError> openReserved(CdDriveManagerPrivate& manager, const QString& driveId)
{
    auto reservation = reserveDrive(manager.m_accessState, driveId);
    if(!reservation) {
        return std::unexpected(reservation.error());
    }

    return ReservedDriveSession{std::move(*reservation), manager.m_backend->open(driveId)};
}

CdDriveObservation inspectDrive(ReservedDriveSession& reserved)
{
    CdDriveObservation observation;
    observation.drive = reserved.opened()->drive;

    auto toc = reserved.opened()->session->readToc();
    if(!toc) {
        observation.error = toc.error();
        return observation;
    }

    auto discId = musicBrainzDiscId(*toc);
    if(!discId) {
        observation.error
            = CdError{.code = CdDriveError::NotAudioDisc, .message = invalidTocUserMessage(), .platformCode = 0};
        return observation;
    }

    observation.toc    = std::move(*toc);
    observation.discId = std::move(*discId);
    return observation;
}

bool sameMedia(const CdDriveObservation& lhs, const CdDriveObservation& rhs)
{
    return lhs.drive == rhs.drive && lhs.toc == rhs.toc && lhs.discId == rhs.discId && lhs.error == rhs.error;
}

void sortObservations(std::vector<CdDriveObservation>& observations)
{
    std::ranges::sort(observations, {}, [](const auto& observation) { return observation.drive.id; });
}
} // namespace

CdDriveLease::CdDriveLease(std::shared_ptr<CdDriveAccessState> state, QString driveId, CdDriveInfo drive, CdToc toc,
                           QString discId, uint64_t generation, std::unique_ptr<CdDriveSession> session)
    : m_state{std::move(state)}
    , m_driveId{std::move(driveId)}
    , m_drive{std::move(drive)}
    , m_toc{std::move(toc)}
    , m_discId{std::move(discId)}
    , m_generation{generation}
    , m_session{std::move(session)}
{ }

CdDriveLease::~CdDriveLease()
{
    release();
}

CdDriveLease::CdDriveLease(CdDriveLease&& other) noexcept
    : m_state{std::move(other.m_state)}
    , m_driveId{std::move(other.m_driveId)}
    , m_drive{std::move(other.m_drive)}
    , m_toc{std::move(other.m_toc)}
    , m_discId{std::move(other.m_discId)}
    , m_generation{std::exchange(other.m_generation, 0)}
    , m_session{std::move(other.m_session)}
{ }

CdDriveLease& CdDriveLease::operator=(CdDriveLease&& other) noexcept
{
    if(this != &other) {
        release();
        m_state      = std::move(other.m_state);
        m_driveId    = std::move(other.m_driveId);
        m_drive      = std::move(other.m_drive);
        m_toc        = std::move(other.m_toc);
        m_discId     = std::move(other.m_discId);
        m_generation = std::exchange(other.m_generation, 0);
        m_session    = std::move(other.m_session);
    }
    return *this;
}

CdDriveLease::operator bool() const
{
    return m_session != nullptr;
}

CdDriveSession* CdDriveLease::session() const
{
    return m_session.get();
}

const CdDriveInfo& CdDriveLease::drive() const
{
    return m_drive;
}

const CdToc& CdDriveLease::toc() const
{
    return m_toc;
}

const QString& CdDriveLease::discId() const
{
    return m_discId;
}

uint64_t CdDriveLease::generation() const
{
    return m_generation;
}

void CdDriveLease::release()
{
    m_session.reset();

    if(m_state && !m_driveId.isEmpty()) {
        const std::scoped_lock lock{m_state->mutex};
        m_state->reservedDrives.erase(m_driveId);
    }

    m_driveId.clear();
    m_state.reset();
}

CdDriveManager::CdDriveManager(std::unique_ptr<CdDriveBackend> backend, std::chrono::milliseconds cacheLifetime)
    : p{std::make_unique<CdDriveManagerPrivate>(std::move(backend),
                                                std::max(cacheLifetime, std::chrono::milliseconds{0}))}
{ }

CdDriveManager::~CdDriveManager() = default;

std::vector<CdDriveInfo> CdDriveManager::drives()
{
    std::vector<CdDriveInfo> result = p->m_backend->drives();
    {
        const std::scoped_lock lock{p->m_mutex};
        for(CdDriveInfo& drive : result) {
            if(const auto cached = p->m_cache.find(drive.id); cached != p->m_cache.cend()) {
                drive = cached->second.drive;
            }
        }
    }
    std::ranges::sort(result, {}, &CdDriveInfo::id);
    return result;
}

CdDriveObservation CdDriveManager::observeDrive(const CdDriveInfo& drive)
{
    auto reserved = openReserved(*p, drive.id);
    if(!reserved && reserved.error().code == CdDriveError::DriveBusy) {
        return errorObservation(drive, reserved.error());
    }

    CdDriveObservation observation
        = reserved->opened() ? inspectDrive(*reserved) : errorObservation(drive, reserved->opened().error());

    const std::scoped_lock lock{p->m_mutex};

    const auto previous = p->m_cache.find(drive.id);
    if(previous != p->m_cache.cend() && sameMedia(previous->second, observation)) {
        observation.generation = previous->second.generation;
        observation.cdText     = previous->second.cdText;
    }
    else {
        observation.generation = p->m_nextGeneration++;
    }

    p->m_cache.insert_or_assign(drive.id, observation);
    return observation;
}

std::expected<CdText, CdError> CdDriveManager::readCdText(const CdDriveObservation& observation)
{
    if(!observation.toc || observation.discId.isEmpty()) {
        return std::unexpected(CdError{
            .code = CdDriveError::NotAudioDisc, .message = tr("Audio CD identity is unavailable"), .platformCode = 0});
    }

    auto reserved = openReserved(*p, observation.drive.id);
    if(!reserved) {
        return std::unexpected(reserved.error());
    }
    if(!reserved->opened()) {
        return std::unexpected(reserved->opened().error());
    }

    auto toc = reserved->opened()->session->readToc();
    if(!toc) {
        return std::unexpected(toc.error());
    }

    const auto discId = musicBrainzDiscId(*toc);
    if(!discId || *discId != observation.discId) {
        return std::unexpected(CdError{.code         = CdDriveError::MediaChanged,
                                       .message      = tr("The disc in the CD drive has changed"),
                                       .platformCode = 0});
    }

    CdText cdText = reserved->opened()->session->readCdText(*toc);

    const std::scoped_lock lock{p->m_mutex};

    auto cached = p->m_cache.find(observation.drive.id);
    if(cached != p->m_cache.end() && cached->second.discId == observation.discId
       && cached->second.generation == observation.generation) {
        cached->second.cdText = cdText;
    }

    return cdText;
}

std::vector<CdDriveObservation> CdDriveManager::observations(bool forceRefresh)
{
    const auto now = std::chrono::steady_clock::now();
    {
        const std::scoped_lock lock{p->m_mutex};

        if(!forceRefresh && p->m_cacheValid && now - p->m_lastRefresh < p->m_cacheLifetime) {
            std::vector<CdDriveObservation> cached;
            for(const auto& [id, value] : std::as_const(p->m_cache)) {
                cached.push_back(value);
            }
            sortObservations(cached);
            return cached;
        }
    }

    const std::vector<CdDriveInfo> availableDrives = drives();

    std::vector<CdDriveObservation> fresh;
    fresh.reserve(availableDrives.size());
    std::unordered_set<QString> availableIds;
    availableIds.reserve(availableDrives.size());
    bool refreshComplete{true};

    for(const CdDriveInfo& drive : availableDrives) {
        availableIds.insert(drive.id);

        auto reserved = openReserved(*p, drive.id);
        if(!reserved) {
            const std::scoped_lock lock{p->m_mutex};
            if(const auto cached = p->m_cache.find(drive.id); cached != p->m_cache.cend()) {
                fresh.push_back(cached->second);
            }
            else {
                fresh.push_back(errorObservation(drive, reserved.error()));
                refreshComplete = false;
            }
            continue;
        }

        CdDriveObservation observation
            = reserved->opened() ? inspectDrive(*reserved) : errorObservation(drive, reserved->opened().error());

        const std::scoped_lock lock{p->m_mutex};
        const auto previous = p->m_cache.find(drive.id);
        if(previous != p->m_cache.cend() && sameMedia(previous->second, observation)) {
            observation.generation = previous->second.generation;
            observation.cdText     = previous->second.cdText;
        }
        else {
            observation.generation = p->m_nextGeneration++;
        }
        p->m_cache.insert_or_assign(drive.id, observation);
        fresh.push_back(std::move(observation));
    }

    {
        const std::scoped_lock lock{p->m_accessState->mutex};
        availableIds.insert(p->m_accessState->reservedDrives.cbegin(), p->m_accessState->reservedDrives.cend());
    }
    {
        const std::scoped_lock lock{p->m_mutex};
        std::erase_if(p->m_cache, [&](const auto& cached) { return !availableIds.contains(cached.first); });
        p->m_lastRefresh = now;
        p->m_cacheValid  = refreshComplete;
    }

    sortObservations(fresh);
    return fresh;
}

std::expected<CdDriveObservation, CdError> CdDriveManager::resolveDisc(const QString& discId, bool forceRefresh)
{
    if(!isValidDiscId(discId)) {
        return std::unexpected(
            CdError{.code = CdDriveError::NotAudioDisc, .message = tr("Invalid CD disc identity"), .platformCode = 0});
    }

    std::vector<CdDriveObservation> observed;
    if(!forceRefresh) {
        const std::scoped_lock lock{p->m_mutex};
        for(const auto& [id, value] : std::as_const(p->m_cache)) {
            observed.push_back(value);
        }
        sortObservations(observed);
    }

    if(forceRefresh || observed.empty()) {
        observed = observations(true);
    }

    auto matches = [&] {
        std::vector<CdDriveObservation> result;
        std::ranges::copy_if(observed, std::back_inserter(result),
                             [&](const auto& observation) { return observation.discId == discId; });
        return result;
    }();

    if(matches.empty() && !forceRefresh) {
        observed = observations(true);
        matches.clear();
        std::ranges::copy_if(observed, std::back_inserter(matches),
                             [&](const auto& observation) { return observation.discId == discId; });
    }

    if(matches.empty()) {
        return std::unexpected(CdError{
            .code = CdDriveError::NoMedia, .message = tr("The requested audio CD is not inserted"), .platformCode = 0});
    }

    QString preferred;
    {
        const std::scoped_lock lock{p->m_mutex};
        if(const auto it = p->m_preferredDrives.find(discId); it != p->m_preferredDrives.cend()) {
            preferred = it->second;
        }
    }

    if(const auto it = std::ranges::find(matches, preferred, [](const auto& item) { return item.drive.id; });
       it != matches.end()) {
        return *it;
    }

    return matches.front();
}

std::expected<CdDriveLease, CdError> CdDriveManager::acquire(const QString& discId)
{
    if(!isValidDiscId(discId)) {
        return std::unexpected(
            CdError{.code = CdDriveError::NotAudioDisc, .message = tr("Invalid CD disc identity"), .platformCode = 0});
    }

    std::optional<CdError> firstFailure;
    bool sawBusy{false};

    for(int attempt{0}; attempt < 2; ++attempt) {
        std::vector<CdDriveObservation> observed;
        if(attempt == 0) {
            const std::scoped_lock lock{p->m_mutex};
            for(const auto& [id, observation] : std::as_const(p->m_cache)) {
                observed.push_back(observation);
            }
        }
        if(attempt > 0 || observed.empty()) {
            observed = observations(true);
        }

        std::vector<CdDriveObservation> matches;
        std::ranges::copy_if(observed, std::back_inserter(matches),
                             [&](const auto& observation) { return observation.discId == discId; });

        QString preferred;
        {
            const std::scoped_lock lock{p->m_mutex};
            if(const auto it = p->m_preferredDrives.find(discId); it != p->m_preferredDrives.cend()) {
                preferred = it->second;
            }
        }

        std::ranges::sort(matches, [&](const CdDriveObservation& lhs, const CdDriveObservation& rhs) {
            const bool lhsPreferred = lhs.drive.id == preferred;
            const bool rhsPreferred = rhs.drive.id == preferred;
            return lhsPreferred != rhsPreferred ? lhsPreferred : lhs.drive.id < rhs.drive.id;
        });

        for(const CdDriveObservation& resolved : matches) {
            const QString driveId = resolved.drive.id;
            auto reserved         = openReserved(*p, driveId);
            if(!reserved) {
                sawBusy = true;
                continue;
            }

            if(!reserved->opened()) {
                if(!firstFailure) {
                    firstFailure = reserved->opened().error();
                }
                invalidateDrive(driveId);
                continue;
            }

            auto toc = reserved->opened()->session->readToc();
            if(!toc) {
                if(!firstFailure) {
                    firstFailure = toc.error();
                }
                invalidateDrive(driveId);
                continue;
            }

            auto actualDiscId = musicBrainzDiscId(*toc);
            if(!actualDiscId || *actualDiscId != discId) {
                if(!firstFailure) {
                    firstFailure = CdError{.code         = CdDriveError::MediaChanged,
                                           .message      = tr("The disc in the CD drive has changed"),
                                           .platformCode = 0};
                }
                invalidateDrive(driveId);
                continue;
            }

            auto reservation                    = reserved->takeReservation();
            auto [state, reservedId]            = reservation.take();
            CdDriveBackend::OpenedDrive& opened = *reserved->opened();
            return CdDriveLease{std::move(state),         std::move(reservedId),    std::move(opened.drive),
                                std::move(*toc),          std::move(*actualDiscId), resolved.generation,
                                std::move(opened.session)};
        }
    }

    if(firstFailure) {
        return std::unexpected(*firstFailure);
    }
    if(sawBusy) {
        return std::unexpected(driveBusyError());
    }
    return std::unexpected(CdError{
        .code = CdDriveError::NoMedia, .message = tr("The requested audio CD is not inserted"), .platformCode = 0});
}

void CdDriveManager::setPreferredDrive(const QString& discId, const QString& driveId)
{
    const std::scoped_lock lock{p->m_mutex};

    if(driveId.isEmpty()) {
        p->m_preferredDrives.erase(discId);
    }
    else {
        p->m_preferredDrives.insert_or_assign(discId, driveId);
    }
}

void CdDriveManager::invalidateDrive(const QString& driveId)
{
    const std::scoped_lock lock{p->m_mutex};
    p->m_cache.erase(driveId);
    p->m_cacheValid = false;
}

void CdDriveManager::invalidateAll()
{
    const std::scoped_lock lock{p->m_mutex};
    p->m_cache.clear();
    p->m_cacheValid = false;
}
} // namespace Fooyin::Cdda
