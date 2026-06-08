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

#include "radiobrowserconfigdialog.h"

#include <gui/guiutils.h>
#include <gui/trackselectioncontroller.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin::RadioBrowser {
RadioBrowserConfigDialog::RadioBrowserConfigDialog(RadioBrowserWidget* radioBrowser, QWidget* parent)
    : WidgetConfigDialog{radioBrowser, tr("Radio Browser Settings"), parent}
    , m_middleClick{new QComboBox(this)}
    , m_doubleClick{new QComboBox(this)}
    , m_playbackOnSend{new QCheckBox(tr("Start playback immediately"), this)}
    , m_hideBroken{new QCheckBox(tr("Hide broken stations"), this)}
    , m_sendClicks{new QCheckBox(tr("Send clicks to radio-browser.info"), this)}
    , m_overrideRowHeight{new QCheckBox(tr("Override row height") + u":"_s, this)}
    , m_rowHeight{new QSpinBox(this)}
    , m_iconSize{new QSpinBox(this)}
    , m_iconHorizontalGap{new QSpinBox(this)}
    , m_iconVerticalGap{new QSpinBox(this)}
    , m_iconItemBorder{new QSpinBox(this)}
    , m_uniformStationIcons{new QCheckBox(tr("Use uniform station icon frames"), this)}
{
    const auto addTrackAction = [](QComboBox* box, const QString& text, const TrackAction action) {
        box->addItem(text, static_cast<int>(action));
    };

    addTrackAction(m_doubleClick, tr("None"), TrackAction::None);
    addTrackAction(m_doubleClick, tr("Play"), TrackAction::Play);
    addTrackAction(m_doubleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist);
    addTrackAction(m_doubleClick, tr("Add to current playlist and play if stopped"),
                   TrackAction::AddCurrentPlaylistAndPlayIfStopped);
    addTrackAction(m_doubleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist);
    addTrackAction(m_doubleClick, tr("Replace current playlist"), TrackAction::SendCurrentPlaylist);
    addTrackAction(m_doubleClick, tr("Create new playlist"), TrackAction::SendNewPlaylist);
    addTrackAction(m_doubleClick, tr("Add to playback queue"), TrackAction::AddToQueue);
    addTrackAction(m_doubleClick, tr("Add to front of playback queue"), TrackAction::QueueNext);
    addTrackAction(m_doubleClick, tr("Replace playback queue"), TrackAction::SendToQueue);

    addTrackAction(m_middleClick, tr("None"), TrackAction::None);
    addTrackAction(m_middleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist);
    addTrackAction(m_middleClick, tr("Add to current playlist and play if stopped"),
                   TrackAction::AddCurrentPlaylistAndPlayIfStopped);
    addTrackAction(m_middleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist);
    addTrackAction(m_middleClick, tr("Replace current playlist"), TrackAction::SendCurrentPlaylist);
    addTrackAction(m_middleClick, tr("Create new playlist"), TrackAction::SendNewPlaylist);
    addTrackAction(m_middleClick, tr("Add to playback queue"), TrackAction::AddToQueue);
    addTrackAction(m_middleClick, tr("Add to front of playback queue"), TrackAction::QueueNext);
    addTrackAction(m_middleClick, tr("Replace playback queue"), TrackAction::SendToQueue);

    m_playbackOnSend->setToolTip(
        tr(R"(For "Replace current playlist" and "Create new playlist", start playback immediately.)"));
    m_hideBroken->setToolTip(tr("Exclude stations marked as broken by radio-browser.info from search results."));
    m_sendClicks->setToolTip(
        tr("Report played stations to radio-browser.info so click counts and station statistics stay up to date."));
    m_uniformStationIcons->setToolTip(
        tr("Draw station icons inside equally sized frames so rows and icon captions align consistently."));

    m_rowHeight->setRange(0, 256);
    m_rowHeight->setSuffix(u" px"_s);

    m_iconItemBorder->setRange(0, 16);
    m_iconItemBorder->setSuffix(u" px"_s);
    m_iconItemBorder->setSpecialValueText(tr("None"));
    m_iconItemBorder->setToolTip(tr("Size of the border around each station icon in icon display mode."));

    m_iconSize->setRange(16, 1024);
    m_iconSize->setSuffix(u" px"_s);
    m_iconSize->setSingleStep(4);

    m_iconHorizontalGap->setRange(-1, 256);
    m_iconHorizontalGap->setSpecialValueText(tr("Auto"));
    m_iconHorizontalGap->setSuffix(u" px"_s);
    m_iconHorizontalGap->setToolTip(
        tr("Horizontal spacing between stations in icon display mode. Auto uses the view's default spacing."));

    m_iconVerticalGap->setRange(0, 256);
    m_iconVerticalGap->setSuffix(u" px"_s);
    m_iconVerticalGap->setToolTip(tr("Vertical spacing between stations in icon display mode."));

    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    int row{0};
    clickBehaviourLayout->addWidget(new QLabel(tr("Double-click") + u":"_s, this), row, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, row++, 1);
    clickBehaviourLayout->addWidget(new QLabel(tr("Middle-click") + u":"_s, this), row, 0);
    clickBehaviourLayout->addWidget(m_middleClick, row++, 1);
    clickBehaviourLayout->addWidget(m_playbackOnSend, row++, 0, 1, 2);
    clickBehaviourLayout->setColumnStretch(2, 1);

    auto* search       = new QGroupBox(tr("Search"), this);
    auto* searchLayout = new QGridLayout(search);

    row = 0;
    searchLayout->addWidget(m_hideBroken, row++, 0, 1, 2);
    searchLayout->addWidget(m_sendClicks, row++, 0, 1, 2);
    searchLayout->setColumnStretch(2, 1);

    auto* appearance       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearance);

    row = 0;
    appearanceLayout->addWidget(m_overrideRowHeight, row, 0);
    appearanceLayout->addWidget(m_rowHeight, row++, 1);
    appearanceLayout->addWidget(m_uniformStationIcons, row++, 0, 1, 2);
    appearanceLayout->setColumnStretch(2, 1);

    auto* iconMode   = new QGroupBox(tr("Icon Mode"), this);
    auto* iconLayout = new QGridLayout(iconMode);

    auto* gapLayout = new QGridLayout();

    gapLayout->addWidget(new QLabel(tr("Horizontal") + u":"_s, this), 0, 0);
    gapLayout->addWidget(m_iconHorizontalGap, 0, 1);
    gapLayout->addWidget(new QLabel(tr("Vertical") + u":"_s, this), 0, 2);
    gapLayout->addWidget(m_iconVerticalGap, 0, 3);
    gapLayout->setColumnStretch(4, 1);

    row = 0;
    iconLayout->addWidget(new QLabel(tr("Item border") + u":"_s, this), row, 0);
    iconLayout->addWidget(m_iconItemBorder, row++, 1);
    iconLayout->addWidget(new QLabel(tr("Icon size") + u":"_s, this), row, 0);
    iconLayout->addWidget(m_iconSize, row++, 1);
    iconLayout->addWidget(new QLabel(u"🛈 "_s + tr("Use <b>Ctrl+Scroll</b> in the widget to resize icons."), this),
                          row++, 0, 1, 3);
    iconLayout->addWidget(Gui::createSectionHeader(tr("Gap"), this), row++, 0);
    iconLayout->addLayout(gapLayout, row++, 0, 1, 3);
    iconLayout->setColumnStretch(2, 1);

    auto* layout{contentLayout()};

    row = 0;
    layout->addWidget(clickBehaviour, row++, 0);
    layout->addWidget(search, row++, 0);
    layout->addWidget(appearance, row++, 0);
    layout->addWidget(iconMode, row++, 0);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(row, 1);

    QObject::connect(m_overrideRowHeight, &QCheckBox::toggled, m_rowHeight, &QWidget::setEnabled);

    loadCurrentConfig();
}

void RadioBrowserConfigDialog::apply()
{
    const bool sendClicks = m_sendClicks->isChecked();
    WidgetConfigDialog::apply();
    widget()->setSendClicks(sendClicks);
    m_sendClicks->setChecked(widget()->sendClicks());
}

RadioBrowserWidget::ConfigData RadioBrowserConfigDialog::config() const
{
    auto config                     = widget()->currentConfig();
    config.doubleClickAction        = m_doubleClick->currentData().toInt();
    config.middleClickAction        = m_middleClick->currentData().toInt();
    config.playbackOnSend           = m_playbackOnSend->isChecked();
    config.hideBroken               = m_hideBroken->isChecked();
    config.view.rowHeight           = m_overrideRowHeight->isChecked() ? m_rowHeight->value() : 0;
    config.view.iconSize            = {m_iconSize->value(), m_iconSize->value()};
    config.view.iconHorizontalGap   = m_iconHorizontalGap->value();
    config.view.iconVerticalGap     = m_iconVerticalGap->value();
    config.view.iconItemBorderWidth = m_iconItemBorder->value();
    config.view.uniformStationIcons = m_uniformStationIcons->isChecked();
    return config;
}

void RadioBrowserConfigDialog::setConfig(const RadioBrowserWidget::ConfigData& config)
{
    if(const int index = m_doubleClick->findData(config.doubleClickAction); index >= 0) {
        m_doubleClick->setCurrentIndex(index);
    }
    if(const int index = m_middleClick->findData(config.middleClickAction); index >= 0) {
        m_middleClick->setCurrentIndex(index);
    }

    m_playbackOnSend->setChecked(config.playbackOnSend);
    m_hideBroken->setChecked(config.hideBroken);
    m_sendClicks->setChecked(widget()->sendClicks());
    m_rowHeight->setEnabled(config.view.rowHeight > 0);
    m_overrideRowHeight->setChecked(config.view.rowHeight > 0);
    m_rowHeight->setValue(config.view.rowHeight);
    m_iconSize->setValue(std::max(config.view.iconSize.width(), config.view.iconSize.height()));
    m_iconHorizontalGap->setValue(config.view.iconHorizontalGap);
    m_iconVerticalGap->setValue(config.view.iconVerticalGap);
    m_iconItemBorder->setValue(config.view.iconItemBorderWidth);
    m_uniformStationIcons->setChecked(config.view.uniformStationIcons);
}
} // namespace Fooyin::RadioBrowser
