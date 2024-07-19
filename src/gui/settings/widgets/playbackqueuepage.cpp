/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "playbackqueuepage.h"

#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

namespace Fooyin {
class PlaybackQueuePageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit PlaybackQueuePageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QLineEdit* m_titleScript;
    QLineEdit* m_subtitleScript;
    QCheckBox* m_headers;
    QCheckBox* m_scrollBars;
    QCheckBox* m_altRowColours;
    QGroupBox* m_showIcon;
    QSpinBox* m_iconWidth;
    QSpinBox* m_iconHeight;
};

PlaybackQueuePageWidget::PlaybackQueuePageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_titleScript{new QLineEdit(this)}
    , m_subtitleScript{new QLineEdit(this)}
    , m_headers{new QCheckBox(tr("Show header"), this)}
    , m_scrollBars{new QCheckBox(tr("Show scrollbar"), this)}
    , m_altRowColours{new QCheckBox(tr("Alternating row colours"), this)}
    , m_showIcon{new QGroupBox(tr("Icon"), this)}
    , m_iconWidth{new QSpinBox(this)}
    , m_iconHeight{new QSpinBox(this)}
{
    auto* general       = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(general);

    auto* leftScriptLabel  = new QLabel(tr("Left script") + QStringLiteral(":"), this);
    auto* rightScriptLabel = new QLabel(tr("Right script") + QStringLiteral(":"), this);

    generalLayout->addWidget(leftScriptLabel, 0, 0);
    generalLayout->addWidget(m_titleScript, 0, 1);
    generalLayout->addWidget(rightScriptLabel, 1, 0);
    generalLayout->addWidget(m_subtitleScript, 1, 1);

    auto* appearance       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearance);

    m_showIcon->setCheckable(true);

    auto* iconGroupLayout = new QGridLayout(m_showIcon);

    auto* widthLabel  = new QLabel(tr("Width") + QStringLiteral(":"), this);
    auto* heightLabel = new QLabel(tr("Height") + QStringLiteral(":"), this);

    m_iconWidth->setSuffix(QStringLiteral("px"));
    m_iconHeight->setSuffix(QStringLiteral("px"));

    m_iconWidth->setMaximum(512);
    m_iconHeight->setMaximum(512);

    m_iconWidth->setSingleStep(5);
    m_iconHeight->setSingleStep(5);

    auto* iconSizeHint = new QLabel(
        QStringLiteral("🛈 ")
            + tr("Size can also be changed using %1 in the widget.").arg(QStringLiteral("<b>Ctrl+Scroll</b>")),
        this);

    int row{0};
    iconGroupLayout->addWidget(widthLabel, row, 0);
    iconGroupLayout->addWidget(m_iconWidth, row++, 1);
    iconGroupLayout->addWidget(heightLabel, row, 0);
    iconGroupLayout->addWidget(m_iconHeight, row++, 1);
    iconGroupLayout->addWidget(iconSizeHint, row, 0, 1, 4);
    iconGroupLayout->setColumnStretch(2, 1);

    row = 0;
    appearanceLayout->addWidget(m_headers, row++, 0);
    appearanceLayout->addWidget(m_scrollBars, row++, 0);
    appearanceLayout->addWidget(m_altRowColours, row++, 0);
    appearanceLayout->addWidget(m_showIcon, row++, 0);

    auto* layout = new QGridLayout(this);
    layout->addWidget(general, 0, 0);
    layout->addWidget(appearance, 1, 0);

    layout->setColumnStretch(0, 1);
    layout->setRowStretch(2, 1);
}

void PlaybackQueuePageWidget::load()
{
    m_titleScript->setText(m_settings->value<Settings::Gui::Internal::QueueViewerLeftScript>());
    m_subtitleScript->setText(m_settings->value<Settings::Gui::Internal::QueueViewerRightScript>());
    m_showIcon->setChecked(m_settings->value<Settings::Gui::Internal::QueueViewerShowIcon>());
    const auto iconSize = m_settings->value<Settings::Gui::Internal::QueueViewerIconSize>().toSize();
    m_iconWidth->setValue(iconSize.width());
    m_iconHeight->setValue(iconSize.height());
    m_headers->setChecked(m_settings->value<Settings::Gui::Internal::QueueViewerHeader>());
    m_scrollBars->setChecked(m_settings->value<Settings::Gui::Internal::QueueViewerScrollBar>());
    m_altRowColours->setChecked(m_settings->value<Settings::Gui::Internal::QueueViewerAltColours>());
}

void PlaybackQueuePageWidget::apply()
{
    m_settings->set<Settings::Gui::Internal::QueueViewerLeftScript>(m_titleScript->text());
    m_settings->set<Settings::Gui::Internal::QueueViewerRightScript>(m_subtitleScript->text());
    m_settings->set<Settings::Gui::Internal::QueueViewerShowIcon>(m_showIcon->isChecked());
    const QSize iconSize{m_iconWidth->value(), m_iconHeight->value()};
    m_settings->set<Settings::Gui::Internal::QueueViewerIconSize>(iconSize);
    m_settings->set<Settings::Gui::Internal::QueueViewerHeader>(m_headers->isChecked());
    m_settings->set<Settings::Gui::Internal::QueueViewerScrollBar>(m_scrollBars->isChecked());
    m_settings->set<Settings::Gui::Internal::QueueViewerAltColours>(m_altRowColours->isChecked());
}

void PlaybackQueuePageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::QueueViewerLeftScript>();
    m_settings->reset<Settings::Gui::Internal::QueueViewerRightScript>();
    m_settings->reset<Settings::Gui::Internal::QueueViewerShowIcon>();
    m_settings->reset<Settings::Gui::Internal::QueueViewerIconSize>();
    m_settings->reset<Settings::Gui::Internal::QueueViewerHeader>();
    m_settings->reset<Settings::Gui::Internal::QueueViewerScrollBar>();
    m_settings->reset<Settings::Gui::Internal::QueueViewerAltColours>();
}

PlaybackQueuePage::PlaybackQueuePage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::PlaybackQueue);
    setName(tr("General"));
    setCategory({tr("Widgets"), tr("Playback Queue")});
    setWidgetCreator([settings] { return new PlaybackQueuePageWidget(settings); });
}
} // namespace Fooyin

#include "moc_playbackqueuepage.cpp"
#include "playbackqueuepage.moc"
