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
 *
 */

#include "radiostationdialog.h"

#include "radiobrowsercontroller.h"
#include "radiobrowserutils.h"
#include "radioiconprovider.h"

#include <gui/widgets/clickablelabel.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTimerEvent>
#include <QUrl>

using namespace Qt::StringLiterals;

constexpr auto MinIconSize = 128;
constexpr auto MaxIconSize = 512;

namespace Fooyin::RadioBrowser {
namespace {
QIcon stationIcon(const RadioStation& station)
{
    return Utils::placeholderIcon(station);
}
} // namespace

RadioStationDialog::RadioStationDialog(RadioBrowserController* controller, QWidget* parent)
    : QDialog{parent}
    , m_controller{controller}
    , m_icon{new QLabel(this)}
    , m_status{new QLabel(this)}
    , m_name{new QLineEdit(this)}
    , m_streamUrl{new QLineEdit(this)}
    , m_detailsToggle{new ClickableLabel(this)}
    , m_detailsWidget{new QWidget(this)}
    , m_homepage{new QLineEdit(this)}
    , m_favicon{new QLineEdit(this)}
    , m_tags{new QLineEdit(this)}
    , m_country{new QLineEdit(this)}
    , m_language{new QLineEdit(this)}
    , m_codec{new QLineEdit(this)}
    , m_bitrate{new QSpinBox(this)}
    , m_buttons{new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this)}
    , m_detailSources{}
    , m_mode{Mode::Add}
    , m_validated{false}
    , m_detailsVisible{false}
    , m_updatingDetails{false}
{
    m_detailSources.fill(DetailSource::Empty);
    initialise();
}

RadioStationDialog::RadioStationDialog(RadioBrowserController* controller, const RadioStation& station, QWidget* parent)
    : RadioStationDialog{controller, station, Mode::Edit, parent}
{ }

RadioStationDialog* RadioStationDialog::createDetailsDialog(RadioBrowserController* controller,
                                                            const RadioStation& station, QWidget* parent)
{
    return new RadioStationDialog{controller, station, Mode::Details, parent};
}

RadioStationDialog::RadioStationDialog(RadioBrowserController* controller, const RadioStation& station, const Mode mode,
                                       QWidget* parent)
    : QDialog{parent}
    , m_controller{controller}
    , m_icon{new QLabel(this)}
    , m_status{new QLabel(this)}
    , m_name{new QLineEdit(this)}
    , m_streamUrl{new QLineEdit(this)}
    , m_detailsToggle{new ClickableLabel(this)}
    , m_detailsWidget{new QWidget(this)}
    , m_homepage{new QLineEdit(this)}
    , m_favicon{new QLineEdit(this)}
    , m_tags{new QLineEdit(this)}
    , m_country{new QLineEdit(this)}
    , m_language{new QLineEdit(this)}
    , m_codec{new QLineEdit(this)}
    , m_bitrate{new QSpinBox(this)}
    , m_buttons{new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this)}
    , m_initialStation{station}
    , m_validatedStation{station}
    , m_detailSources{}
    , m_mode{mode}
    , m_validated{mode == Mode::Details || (!station.name.isEmpty() && !station.effectiveStreamUrl().isEmpty())}
    , m_detailsVisible{false}
    , m_updatingDetails{false}
{
    m_detailSources.fill(DetailSource::Empty);
    initialise();

    m_name->setText(station.name);
    m_streamUrl->setText(station.effectiveStreamUrl());

    populateDetails(station, true);
    updateIcon();
    if(mode == Mode::Details) {
        setReadOnly(true);
    }
    else {
        m_status->setText(m_validated ? tr("Station validated.") : tr("Enter a station name and stream URL."));
    }
    updateAcceptState();
}

void RadioStationDialog::initialise()
{
    switch(m_mode) {
        case Mode::Add:
            setWindowTitle(tr("Add Custom Station"));
            break;
        case Mode::Edit:
            setWindowTitle(tr("Edit Custom Station"));
            break;
        case Mode::Details:
            setWindowTitle(tr("Station Details"));
            m_buttons->setStandardButtons(QDialogButtonBox::Close);
            break;
    }

    m_icon->setMinimumSize(MinIconSize, MinIconSize);
    m_icon->setMaximumSize(MaxIconSize, MaxIconSize);
    m_icon->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_icon->setAlignment(Qt::AlignCenter);
    updateIcon();

    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);

    m_name->setClearButtonEnabled(true);
    m_streamUrl->setClearButtonEnabled(true);
    m_streamUrl->setPlaceholderText(u"https://example.com/stream"_s);
    m_homepage->setClearButtonEnabled(true);
    m_homepage->setPlaceholderText(u"https://example.com"_s);
    m_favicon->setClearButtonEnabled(true);
    m_favicon->setPlaceholderText(u"https://example.com/favicon.png"_s);
    m_tags->setClearButtonEnabled(true);
    m_tags->setPlaceholderText(tr("Separate tags with commas"));
    m_country->setClearButtonEnabled(true);
    m_language->setClearButtonEnabled(true);
    m_codec->setClearButtonEnabled(true);
    m_bitrate->setRange(0, 10000);
    m_bitrate->setSuffix(u" kbps"_s);
    m_bitrate->setSpecialValueText(tr("Unknown"));

    m_detailsToggle->setCursor(Qt::PointingHandCursor);
    setDetailsVisible(m_mode == Mode::Details);

    auto* detailsLayout = new QGridLayout(m_detailsWidget);
    detailsLayout->setContentsMargins({});

    int detailsRow{0};

    if(m_mode == Mode::Details) {
        detailsLayout->addWidget(new QLabel(tr("Name") + u":"_s), detailsRow, 0);
        detailsLayout->addWidget(m_name, detailsRow++, 1);
        detailsLayout->addWidget(new QLabel(tr("Stream URL") + u":"_s), detailsRow, 0);
        detailsLayout->addWidget(m_streamUrl, detailsRow++, 1);
    }

    detailsLayout->addWidget(new QLabel(tr("Homepage") + u":"_s), detailsRow, 0);
    detailsLayout->addWidget(m_homepage, detailsRow++, 1);
    detailsLayout->addWidget(new QLabel(tr("Favicon") + u":"_s), detailsRow, 0);
    detailsLayout->addWidget(m_favicon, detailsRow++, 1);
    detailsLayout->addWidget(new QLabel(tr("Tags") + u":"_s), detailsRow, 0);
    detailsLayout->addWidget(m_tags, detailsRow++, 1);
    detailsLayout->addWidget(new QLabel(tr("Country") + u":"_s), detailsRow, 0);
    detailsLayout->addWidget(m_country, detailsRow++, 1);
    detailsLayout->addWidget(new QLabel(tr("Language") + u":"_s), detailsRow, 0);
    detailsLayout->addWidget(m_language, detailsRow++, 1);
    detailsLayout->addWidget(new QLabel(tr("Codec") + u":"_s), detailsRow, 0);
    detailsLayout->addWidget(m_codec, detailsRow++, 1);
    detailsLayout->addWidget(new QLabel(tr("Bitrate") + u":"_s), detailsRow, 0);
    detailsLayout->addWidget(m_bitrate, detailsRow++, 1);
    detailsLayout->setColumnStretch(1, 1);

    auto* layout = new QGridLayout(this);

    int row{0};
    layout->addWidget(m_icon, row++, 0, 1, 3, Qt::AlignCenter);

    if(m_mode != Mode::Details) {
        layout->addWidget(new QLabel(tr("Name") + u":"_s), row, 0);
        layout->addWidget(m_name, row++, 1, 1, 2);
        layout->addWidget(new QLabel(tr("Stream URL") + u":"_s), row, 0);
        layout->addWidget(m_streamUrl, row++, 1, 1, 2);
    }
    if(m_mode != Mode::Details) {
        layout->addWidget(m_detailsToggle, row++, 0, 1, 3, Qt::AlignLeft);
    }

    layout->addWidget(m_detailsWidget, row++, 0, 1, 3);
    layout->setRowStretch(row++, 1);

    if(m_mode != Mode::Details) {
        layout->addWidget(m_status, row, 0, 1, 2);
        layout->addWidget(m_buttons, row++, 2, Qt::AlignRight);
    }
    else {
        layout->addWidget(m_buttons, row++, 0, 1, 3, Qt::AlignRight);
    }

    layout->setColumnStretch(1, 1);

    QObject::connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if(m_mode != Mode::Details) {
        QObject::connect(m_name, &QLineEdit::textChanged, this, &RadioStationDialog::scheduleValidation);
        QObject::connect(m_streamUrl, &QLineEdit::textChanged, this, &RadioStationDialog::handleStreamUrlChanged);
    }
    QObject::connect(m_detailsToggle, &ClickableLabel::clicked, this,
                     [this]() { setDetailsVisible(!m_detailsVisible); });

    QObject::connect(m_homepage, &QLineEdit::textChanged, this, [this]() { markDetailEdited(DetailField::Homepage); });
    QObject::connect(m_favicon, &QLineEdit::textChanged, this, [this]() { markDetailEdited(DetailField::Favicon); });
    QObject::connect(m_tags, &QLineEdit::textChanged, this, [this]() { markDetailEdited(DetailField::Tags); });
    QObject::connect(m_country, &QLineEdit::textChanged, this, [this]() { markDetailEdited(DetailField::Country); });
    QObject::connect(m_language, &QLineEdit::textChanged, this, [this]() { markDetailEdited(DetailField::Language); });
    QObject::connect(m_codec, &QLineEdit::textChanged, this, [this]() { markDetailEdited(DetailField::Codec); });
    QObject::connect(m_bitrate, &QSpinBox::valueChanged, this, [this]() { markDetailEdited(DetailField::Bitrate); });

    QObject::connect(m_controller, &RadioBrowserController::stationValidated, this,
                     [this](const QObject* context, const RadioStation& station) {
                         if(context == this) {
                             setValidationSucceeded(station);
                         }
                     });
    QObject::connect(m_controller, &RadioBrowserController::stationValidationFailed, this,
                     [this](const QObject* context, const QString& error) {
                         if(context == this) {
                             setValidationFailed(error);
                         }
                     });
    QObject::connect(m_controller, &RadioBrowserController::stationUrlLookupFinished, this,
                     [this](const QObject* context, const RadioStationList& stations) {
                         if(context == this) {
                             setLookupFinished(stations);
                         }
                     });
    QObject::connect(m_controller, &RadioBrowserController::stationUrlLookupFailed, this,
                     [this](const QObject* context) {
                         if(context == this) {
                             setLookupFailed();
                         }
                     });
    if(m_controller && m_controller->iconProvider()) {
        QObject::connect(m_controller->iconProvider(), &RadioIconProvider::iconLoaded, this,
                         [this](const QString& favicon) {
                             const RadioStation station = m_validated ? m_validatedStation : inputStation();
                             if(station.favicon.trimmed() == favicon) {
                                 updateIcon();
                             }
                         });
    }

    updateAcceptState();
}

void RadioStationDialog::setReadOnly(const bool readOnly)
{
    m_name->setClearButtonEnabled(!readOnly);
    m_streamUrl->setClearButtonEnabled(!readOnly);
    m_homepage->setClearButtonEnabled(!readOnly);
    m_favicon->setClearButtonEnabled(!readOnly);
    m_tags->setClearButtonEnabled(!readOnly);
    m_country->setClearButtonEnabled(!readOnly);
    m_language->setClearButtonEnabled(!readOnly);
    m_codec->setClearButtonEnabled(!readOnly);

    m_name->setReadOnly(readOnly);
    m_streamUrl->setReadOnly(readOnly);
    m_homepage->setReadOnly(readOnly);
    m_favicon->setReadOnly(readOnly);
    m_tags->setReadOnly(readOnly);
    m_country->setReadOnly(readOnly);
    m_language->setReadOnly(readOnly);
    m_codec->setReadOnly(readOnly);
    m_bitrate->setReadOnly(readOnly);
    m_bitrate->setButtonSymbols(readOnly ? QAbstractSpinBox::NoButtons : QAbstractSpinBox::UpDownArrows);
    m_detailsToggle->setVisible(!readOnly);
    m_status->setVisible(!readOnly);
}

RadioStation RadioStationDialog::station() const
{
    return m_validated ? stationWithDetails(m_validatedStation) : inputStation();
}

QSize RadioStationDialog::sizeHint() const
{
    return QDialog::sizeHint().expandedTo({560, 220});
}

QSize RadioStationDialog::minimumSizeHint() const
{
    return QDialog::minimumSizeHint().expandedTo({560, 180});
}

void RadioStationDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateIcon();
}

void RadioStationDialog::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_validationTimer.timerId()) {
        m_validationTimer.stop();
        validateStation();
    }

    QDialog::timerEvent(event);
}

void RadioStationDialog::handleStreamUrlChanged()
{
    clearLookupDetails();
    scheduleValidation();
}

void RadioStationDialog::scheduleValidation()
{
    if(m_mode == Mode::Details) {
        return;
    }

    m_validated        = false;
    m_validatedStation = {};
    m_lookupUrl.clear();
    m_lookupFallbackUrl.clear();
    updateIcon();

    const RadioStation station = inputStation();
    const QUrl url{station.streamUrl};

    if(station.name.isEmpty() || !url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
        m_validationTimer.stop();
        m_status->setText(tr("Enter a station name and stream URL."));
        updateAcceptState();
        return;
    }

    m_status->setText(tr("Waiting to validate…"));
    m_validationTimer.start(500, this);
    updateAcceptState();
}

void RadioStationDialog::validateStation()
{
    if(m_mode == Mode::Details) {
        return;
    }

    if(!m_controller) {
        setValidationFailed(tr("Radio Browser is unavailable."));
        return;
    }

    setValidationPending();
    m_controller->validateStation(inputStation(), this);
}

void RadioStationDialog::setValidationPending()
{
    m_status->setText(tr("Validating station…"));
    updateAcceptState();
}

void RadioStationDialog::setValidationFailed(const QString& error)
{
    m_validated        = false;
    m_validatedStation = {};
    updateIcon();
    m_status->setText(error.isEmpty() ? tr("Station could not be validated.") : error);
    updateAcceptState();
}

void RadioStationDialog::setValidationSucceeded(const RadioStation& station)
{
    m_validated        = true;
    m_validatedStation = stationWithDetails(station);
    updateIcon();
    m_status->setText(tr("Station validated."));
    updateAcceptState();

    const QString lookupUrl = m_validatedStation.effectiveStreamUrl();
    if(!lookupUrl.isEmpty() && m_controller) {
        const QString inputUrl = m_streamUrl->text().trimmed();
        m_lookupUrl            = lookupUrl;
        m_lookupFallbackUrl    = inputUrl != lookupUrl ? inputUrl : QString{};
        m_status->setText(tr("Station validated. Looking up station details…"));
        m_controller->lookupStationByUrl(lookupUrl, this);
    }
}

void RadioStationDialog::setLookupFinished(const RadioStationList& stations)
{
    const QString currentStreamUrl = m_streamUrl->text().trimmed();
    if(m_lookupUrl.isEmpty()
       || (m_validatedStation.effectiveStreamUrl() != m_lookupUrl && currentStreamUrl != m_lookupUrl)) {
        return;
    }

    m_lookupUrl.clear();

    if(stations.empty()) {
        if(!m_lookupFallbackUrl.isEmpty() && m_controller) {
            m_lookupUrl = std::exchange(m_lookupFallbackUrl, {});
            m_controller->lookupStationByUrl(m_lookupUrl, this);
            return;
        }
        m_status->setText(tr("Station validated."));
        return;
    }

    m_lookupFallbackUrl.clear();

    RadioStation station = stations.front();
    station.name         = m_name->text().trimmed();
    station.streamUrl    = m_validatedStation.streamUrl;
    if(!m_validatedStation.resolvedStreamUrl.isEmpty()) {
        station.resolvedStreamUrl = m_validatedStation.resolvedStreamUrl;
    }
    station.local  = true;
    station.online = true;

    populateDetails(station, false);
    m_validatedStation = stationWithDetails(station);
    updateIcon();
    m_status->setText(tr("Station validated. Details found."));
}

void RadioStationDialog::setLookupFailed()
{
    if(m_lookupUrl.isEmpty()) {
        return;
    }

    m_lookupUrl.clear();

    if(!m_lookupFallbackUrl.isEmpty() && m_controller) {
        m_lookupUrl = std::exchange(m_lookupFallbackUrl, {});
        m_controller->lookupStationByUrl(m_lookupUrl, this);
        return;
    }

    m_status->setText(tr("Station validated."));
}

void RadioStationDialog::updateAcceptState()
{
    if(m_mode == Mode::Details) {
        return;
    }

    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(m_validated);
}

void RadioStationDialog::updateValidatedStationFromFields()
{
    if(m_validated) {
        m_validatedStation = stationWithDetails(m_validatedStation);
    }
    updateIcon();
}

void RadioStationDialog::setDetailsVisible(bool visible)
{
    m_detailsVisible = visible;
    m_detailsWidget->setVisible(visible);
    m_detailsToggle->setText(visible ? tr("▼ Details") : tr("▶ Details"));

    if(layout()) {
        QMetaObject::invokeMethod(this, &QWidget::adjustSize, Qt::QueuedConnection);
    }
}

void RadioStationDialog::populateDetails(const RadioStation& station, bool overwrite)
{
    const DetailSource source = overwrite ? DetailSource::Manual : DetailSource::Lookup;
    setDetailText(DetailField::Homepage, station.homepage, source);
    setDetailText(DetailField::Favicon, station.favicon, source);
    setDetailText(DetailField::Tags, station.tags, source);
    setDetailText(DetailField::Country, station.country, source);
    setDetailText(DetailField::Language, station.language, source);
    setDetailText(DetailField::Codec, station.codec, source);
    if(canPopulateDetail(DetailField::Bitrate, overwrite) && station.bitrate > 0) {
        setDetailValue(DetailField::Bitrate, station.bitrate, source);
    }
}

void RadioStationDialog::clearLookupDetails()
{
    const auto clearText = [this](DetailField field) {
        if(detailSource(field) == DetailSource::Lookup) {
            setDetailText(field, {}, DetailSource::Empty);
        }
    };

    clearText(DetailField::Homepage);
    clearText(DetailField::Favicon);
    clearText(DetailField::Tags);
    clearText(DetailField::Country);
    clearText(DetailField::Language);
    clearText(DetailField::Codec);

    if(detailSource(DetailField::Bitrate) == DetailSource::Lookup) {
        setDetailValue(DetailField::Bitrate, 0, DetailSource::Empty);
    }
}

void RadioStationDialog::markDetailEdited(DetailField field)
{
    if(!m_updatingDetails) {
        m_detailSources.at(static_cast<size_t>(field)) = DetailSource::Manual;
    }
    updateValidatedStationFromFields();
}

void RadioStationDialog::setDetailText(DetailField field, const QString& value, DetailSource source)
{
    const QString trimmed = value.trimmed();
    if(source != DetailSource::Empty && trimmed.isEmpty()) {
        return;
    }
    if(source != DetailSource::Empty && !canPopulateDetail(field, source == DetailSource::Manual)) {
        return;
    }

    QLineEdit* lineEdit = detailLineEdit(field);
    if(!lineEdit) {
        return;
    }

    const QSignalBlocker blocker{lineEdit};
    const bool wasUpdating = std::exchange(m_updatingDetails, true);
    lineEdit->setText(trimmed);
    m_updatingDetails = wasUpdating;

    m_detailSources.at(static_cast<size_t>(field))
        = lineEdit->text().trimmed().isEmpty() ? DetailSource::Empty : source;
}

void RadioStationDialog::setDetailValue(DetailField field, int value, DetailSource source)
{
    if(field != DetailField::Bitrate) {
        return;
    }
    if(source != DetailSource::Empty && value <= 0) {
        return;
    }
    if(source != DetailSource::Empty && !canPopulateDetail(field, source == DetailSource::Manual)) {
        return;
    }

    const QSignalBlocker blocker{m_bitrate};
    const bool wasUpdating = std::exchange(m_updatingDetails, true);
    m_bitrate->setValue(std::max(0, value));
    m_updatingDetails = wasUpdating;

    m_detailSources.at(static_cast<size_t>(field)) = m_bitrate->value() == 0 ? DetailSource::Empty : source;
}

void RadioStationDialog::updateIcon() const
{
    const QSize targetSize
        = m_icon->size().boundedTo({MaxIconSize, MaxIconSize}).expandedTo({MinIconSize, MinIconSize});
    const int iconSize         = std::min(targetSize.width(), targetSize.height());
    const RadioStation station = m_validated ? m_validatedStation : inputStation();

    if(m_controller && m_controller->iconProvider() && !station.favicon.trimmed().isEmpty()) {
        const QIcon icon = m_controller->iconProvider()->icon(station, iconSize * 2);
        if(!icon.isNull()) {
            m_icon->setPixmap(icon.pixmap(iconSize, iconSize));
            return;
        }
        m_controller->iconProvider()->requestIcon(station, iconSize * 2);
    }

    m_icon->setPixmap(stationIcon(station).pixmap(iconSize, iconSize));
}

RadioStation RadioStationDialog::inputStation() const
{
    RadioStation station;
    station.name      = m_name->text().trimmed();
    station.streamUrl = m_streamUrl->text().trimmed();
    station.local     = true;
    station.online    = true;
    return stationWithDetails(station);
}

RadioStation RadioStationDialog::stationWithDetails(RadioStation station) const
{
    station.homepage = m_homepage->text().trimmed();
    station.favicon  = m_favicon->text().trimmed();
    station.tags     = m_tags->text().trimmed();
    station.country  = m_country->text().trimmed();
    station.language = m_language->text().trimmed();
    station.codec    = m_codec->text().trimmed();
    station.bitrate  = m_bitrate->value();
    station.local    = true;
    station.online   = true;

    return station;
}

QLineEdit* RadioStationDialog::detailLineEdit(DetailField field) const
{
    switch(field) {
        case DetailField::Homepage:
            return m_homepage;
        case DetailField::Favicon:
            return m_favicon;
        case DetailField::Tags:
            return m_tags;
        case DetailField::Country:
            return m_country;
        case DetailField::Language:
            return m_language;
        case DetailField::Codec:
            return m_codec;
        case DetailField::Bitrate:
        case DetailField::Count:
            break;
    }

    return nullptr;
}

RadioStationDialog::DetailSource RadioStationDialog::detailSource(DetailField field) const
{
    return m_detailSources.at(static_cast<size_t>(field));
}

bool RadioStationDialog::canPopulateDetail(DetailField field, bool overwrite) const
{
    if(overwrite) {
        return true;
    }

    const DetailSource source = detailSource(field);
    if(source == DetailSource::Manual) {
        return false;
    }

    if(field == DetailField::Bitrate) {
        return m_bitrate->value() == 0;
    }

    const QLineEdit* lineEdit = detailLineEdit(field);
    return lineEdit && lineEdit->text().trimmed().isEmpty();
}
} // namespace Fooyin::RadioBrowser

#include "moc_radiostationdialog.cpp"
