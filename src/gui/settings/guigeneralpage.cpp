/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "guigeneralpage.h"

#include "internalguisettings.h"
#include "quicksetup/quicksetupdialog.h"

#include <core/internalcoresettings.h>
#include <gui/editablelayout.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QLabel>
#include <QProxyStyle>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStyleFactory>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Settings::Gui;
using namespace Settings::Gui::Internal;

class GuiGeneralPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit GuiGeneralPageWidget(LayoutProvider* layoutProvider, EditableLayout* editableLayout,
                                  SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void showQuickSetup();
    void importLayout();
    void exportLayout();

    LayoutProvider* m_layoutProvider;
    EditableLayout* m_editableLayout;
    SettingsManager* m_settings;

    QComboBox* m_styles;

    QRadioButton* m_detectIconTheme;
    QRadioButton* m_lightTheme;
    QRadioButton* m_darkTheme;
    QRadioButton* m_systemTheme;

    QCheckBox* m_overrideMargin;
    QSpinBox* m_editableLayoutMargin;

    QCheckBox* m_splitterHandles;
    QCheckBox* m_lockSplitters;
    QCheckBox* m_overrideSplitterHandle;
    QSpinBox* m_splitterHandleGap;

    QCheckBox* m_buttonRaise;
    QCheckBox* m_buttonStretch;

    ScriptLineEdit* m_titleScript;
    QSpinBox* m_vbrInterval;

    QRadioButton* m_preferPlaying;
    QRadioButton* m_preferSelection;

    QSpinBox* m_seekStep;
    QDoubleSpinBox* m_volumeStep;
    QSpinBox* m_starRatingSize;
};

GuiGeneralPageWidget::GuiGeneralPageWidget(LayoutProvider* layoutProvider, EditableLayout* editableLayout,
                                           SettingsManager* settings)
    : m_layoutProvider{layoutProvider}
    , m_editableLayout{editableLayout}
    , m_settings{settings}
    , m_styles{new QComboBox(this)}
    , m_detectIconTheme{new QRadioButton(tr("Auto-detect theme"), this)}
    , m_lightTheme{new QRadioButton(tr("Light"), this)}
    , m_darkTheme{new QRadioButton(tr("Dark"), this)}
    , m_systemTheme{new QRadioButton(tr("Use system icons"), this)}
    , m_overrideMargin{new QCheckBox(tr("Override root margin") + u":"_s, this)}
    , m_editableLayoutMargin{new QSpinBox(this)}
    , m_splitterHandles{new QCheckBox(tr("Show splitter handles"), this)}
    , m_lockSplitters{new QCheckBox(tr("Lock splitters"), this)}
    , m_overrideSplitterHandle{new QCheckBox(tr("Override splitter handle size") + u":"_s, this)}
    , m_splitterHandleGap{new QSpinBox(this)}
    , m_buttonRaise{new QCheckBox(tr("Raise"), this)}
    , m_buttonStretch{new QCheckBox(tr("Stretch"), this)}
    , m_titleScript{new ScriptLineEdit(this)}
    , m_vbrInterval{new QSpinBox(this)}
    , m_preferPlaying{new QRadioButton(tr("Prefer currently playing track"), this)}
    , m_preferSelection{new QRadioButton(tr("Prefer current selection"), this)}
    , m_seekStep{new QSpinBox(this)}
    , m_volumeStep{new QDoubleSpinBox(this)}
    , m_starRatingSize{new QSpinBox(this)}
{
    auto* setupBox        = new QGroupBox(tr("Setup"));
    auto* setupBoxLayout  = new QHBoxLayout(setupBox);
    auto* quickSetup      = new QPushButton(tr("Quick Setup"), this);
    auto* importLayoutBtn = new QPushButton(tr("Import Layout"), this);
    auto* exportLayoutBtn = new QPushButton(tr("Export Layout"), this);

    setupBoxLayout->addWidget(quickSetup);
    setupBoxLayout->addWidget(importLayoutBtn);
    setupBoxLayout->addWidget(exportLayoutBtn);

    auto* themeBox       = new QGroupBox(tr("Theme"), this);
    auto* themeBoxLayout = new QGridLayout(themeBox);

    auto* iconThemeBox       = new QGroupBox(tr("Icons"), themeBox);
    auto* iconThemeBoxLayout = new QGridLayout(iconThemeBox);

    int row{0};
    iconThemeBoxLayout->addWidget(m_detectIconTheme, row++, 0, 1, 2);
    iconThemeBoxLayout->addWidget(m_lightTheme, row, 0);
    iconThemeBoxLayout->addWidget(m_darkTheme, row++, 1);
    iconThemeBoxLayout->addWidget(m_systemTheme, row++, 0, 1, 2);
    iconThemeBoxLayout->setColumnStretch(2, 1);

    row = 0;
    themeBoxLayout->addWidget(new QLabel(tr("Style") + ":"_L1, this), row, 0);
    themeBoxLayout->addWidget(m_styles, row++, 1);
    themeBoxLayout->addWidget(iconThemeBox, row++, 0, 1, 3);
    themeBoxLayout->setColumnStretch(2, 1);

    auto* layoutGroup       = new QGroupBox(tr("Layout"), this);
    auto* layoutGroupLayout = new QGridLayout(layoutGroup);

    row = 0;
    layoutGroupLayout->addWidget(m_splitterHandles, row++, 0, 1, 3);
    layoutGroupLayout->addWidget(m_lockSplitters, row++, 0, 1, 3);
    layoutGroupLayout->addWidget(m_overrideSplitterHandle, row, 0);
    layoutGroupLayout->addWidget(m_splitterHandleGap, row++, 1);
    layoutGroupLayout->addWidget(m_overrideMargin, row, 0);
    layoutGroupLayout->addWidget(m_editableLayoutMargin, row++, 1);
    layoutGroupLayout->setColumnStretch(2, 1);

    m_editableLayoutMargin->setRange(0, 20);
    m_editableLayoutMargin->setSuffix(u"px"_s);

    m_splitterHandleGap->setRange(0, 20);
    m_splitterHandleGap->setSuffix(u"px"_s);

    auto* toolButtonGroup       = new QGroupBox(tr("Tool Buttons"), this);
    auto* toolButtonGroupLayout = new QVBoxLayout(toolButtonGroup);

    toolButtonGroupLayout->addWidget(m_buttonRaise);
    toolButtonGroupLayout->addWidget(m_buttonStretch);

    auto* playbackGroup       = new QGroupBox(tr("Playback"), this);
    auto* playbackGroupLayout = new QGridLayout(playbackGroup);

    m_vbrInterval->setRange(100, 300000);
    m_vbrInterval->setSingleStep(250);
    m_vbrInterval->setSuffix(u" ms"_s);

    row = 0;
    playbackGroupLayout->addWidget(new QLabel(tr("Window title") + u":"_s, this), row, 0);
    playbackGroupLayout->addWidget(m_titleScript, row++, 1, 1, 2);
    playbackGroupLayout->addWidget(new QLabel(tr("VBR update interval") + u":"_s, this), row, 0);
    playbackGroupLayout->addWidget(m_vbrInterval, row++, 1);
    playbackGroupLayout->setColumnStretch(2, 1);

    auto* selectionGroupBox    = new QGroupBox(tr("Selection Info"), this);
    auto* selectionGroup       = new QButtonGroup(this);
    auto* selectionGroupLayout = new QVBoxLayout(selectionGroupBox);

    selectionGroup->addButton(m_preferPlaying);
    selectionGroup->addButton(m_preferSelection);

    selectionGroupLayout->addWidget(m_preferPlaying);
    selectionGroupLayout->addWidget(m_preferSelection);

    auto* controlsGroup  = new QGroupBox(tr("Controls"), this);
    auto* controlsLayout = new QGridLayout(controlsGroup);

    m_seekStep->setRange(100, 30000);
    m_seekStep->setSuffix(u" ms"_s);

    m_volumeStep->setRange(1, 5);
    m_volumeStep->setSuffix(u" dB"_s);

    row = 0;
    controlsLayout->addWidget(new QLabel(tr("Seek step") + ":"_L1, this), row, 0);
    controlsLayout->addWidget(m_seekStep, row++, 1);
    controlsLayout->addWidget(new QLabel(tr("Volume step") + ":"_L1, this), row, 0);
    controlsLayout->addWidget(m_volumeStep, row++, 1);
    controlsLayout->setColumnStretch(2, 1);

    auto* ratingGroupBox    = new QGroupBox(tr("Rating"), this);
    auto* ratingGroupLayout = new QGridLayout(ratingGroupBox);

    m_starRatingSize->setRange(5, 30);
    m_starRatingSize->setSuffix(u"px"_s);

    ratingGroupLayout->addWidget(new QLabel(tr("Star size"), this), 0, 0);
    ratingGroupLayout->addWidget(m_starRatingSize, 0, 1);
    ratingGroupLayout->setColumnStretch(2, 1);

    auto* mainLayout = new QGridLayout(this);

    row = 0;
    mainLayout->addWidget(setupBox, row++, 0, 1, 2);
    mainLayout->addWidget(themeBox, row++, 0, 1, 2);
    mainLayout->addWidget(layoutGroup, row++, 0, 1, 2);
    mainLayout->addWidget(toolButtonGroup, row++, 0, 1, 2);
    mainLayout->addWidget(playbackGroup, row++, 0, 1, 2);
    mainLayout->addWidget(selectionGroupBox, row++, 0, 1, 2);
    mainLayout->addWidget(controlsGroup, row++, 0, 1, 2);
    mainLayout->addWidget(ratingGroupBox, row++, 0, 1, 2);

    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    QObject::connect(quickSetup, &QPushButton::clicked, this, &GuiGeneralPageWidget::showQuickSetup);
    QObject::connect(importLayoutBtn, &QPushButton::clicked, this, &GuiGeneralPageWidget::importLayout);
    QObject::connect(exportLayoutBtn, &QPushButton::clicked, this, &GuiGeneralPageWidget::exportLayout);

    QObject::connect(m_overrideMargin, &QCheckBox::toggled, m_editableLayoutMargin, &QWidget::setEnabled);
    QObject::connect(m_overrideSplitterHandle, &QCheckBox::toggled, m_splitterHandleGap, &QWidget::setEnabled);
}

void GuiGeneralPageWidget::load()
{
    m_styles->clear();

    m_styles->addItem(u"System default"_s);
    const QStringList keys = QStyleFactory::keys();
    for(const QString& key : keys) {
        m_styles->addItem(key);
    }

    const auto style = m_settings->value<Style>();
    if(!style.isEmpty()) {
        m_styles->setCurrentText(style);
    }
    else {
        m_styles->setCurrentIndex(0);
    }

    const auto iconTheme = static_cast<IconThemeOption>(m_settings->value<IconTheme>());
    switch(iconTheme) {
        case(IconThemeOption::AutoDetect):
            m_detectIconTheme->setChecked(true);
            break;
        case(IconThemeOption::System):
            m_systemTheme->setChecked(true);
            break;
        case(IconThemeOption::Light):
            m_lightTheme->setChecked(true);
            break;
        case(IconThemeOption::Dark):
            m_darkTheme->setChecked(true);
            break;
    }

    m_splitterHandles->setChecked(m_settings->value<ShowSplitterHandles>());
    m_lockSplitters->setChecked(m_settings->value<LockSplitterHandles>());

    m_overrideMargin->setChecked(m_settings->value<EditableLayoutMargin>() >= 0);
    m_editableLayoutMargin->setValue(m_settings->value<EditableLayoutMargin>());
    m_editableLayoutMargin->setEnabled(m_overrideMargin->isChecked());

    m_overrideSplitterHandle->setChecked(m_settings->value<SplitterHandleSize>() >= 0);
    m_splitterHandleGap->setValue(m_settings->value<SplitterHandleSize>());
    m_splitterHandleGap->setEnabled(m_overrideSplitterHandle->isChecked());

    const auto buttonOptions = m_settings->value<ToolButtonStyle>();
    m_buttonRaise->setChecked(buttonOptions & Raise);
    m_buttonStretch->setChecked(buttonOptions & Stretch);

    m_titleScript->setText(m_settings->value<WindowTitleTrackScript>());
    m_vbrInterval->setValue(m_settings->value<Settings::Core::Internal::VBRUpdateInterval>());

    const auto option = static_cast<SelectionDisplay>(m_settings->value<InfoDisplayPrefer>());
    if(option == SelectionDisplay::PreferPlaying) {
        m_preferPlaying->setChecked(true);
    }
    else {
        m_preferSelection->setChecked(true);
    }

    m_seekStep->setValue(m_settings->value<SeekStep>());
    m_volumeStep->setValue(m_settings->value<VolumeStep>());
    m_starRatingSize->setValue(m_settings->value<StarRatingSize>());
}

void GuiGeneralPageWidget::apply()
{
    m_settings->set<Style>(m_styles->currentText());

    IconThemeOption iconThemeOption;
    if(m_detectIconTheme->isChecked()) {
        iconThemeOption = IconThemeOption::AutoDetect;
    }
    else if(m_lightTheme->isChecked()) {
        iconThemeOption = IconThemeOption::Light;
    }
    else if(m_darkTheme->isChecked()) {
        iconThemeOption = IconThemeOption::Dark;
    }
    else {
        iconThemeOption = IconThemeOption::System;
    }
    m_settings->set<IconTheme>(static_cast<int>(iconThemeOption));

    m_settings->set<ShowSplitterHandles>(m_splitterHandles->isChecked());
    m_settings->set<LockSplitterHandles>(m_lockSplitters->isChecked());

    if(m_overrideMargin->isChecked()) {
        m_settings->set<EditableLayoutMargin>(m_editableLayoutMargin->value());
    }
    else {
        m_settings->reset<EditableLayoutMargin>();
    }

    if(m_overrideSplitterHandle->isChecked()) {
        m_settings->set<SplitterHandleSize>(m_splitterHandleGap->value());
    }
    else {
        m_settings->reset<SplitterHandleSize>();
    }

    ToolButtonOptions buttonOptions;
    buttonOptions.setFlag(Raise, m_buttonRaise->isChecked());
    buttonOptions.setFlag(Stretch, m_buttonStretch->isChecked());

    m_settings->set<ToolButtonStyle>(static_cast<int>(buttonOptions));

    m_settings->set<WindowTitleTrackScript>(m_titleScript->text());
    m_settings->set<Settings::Core::Internal::VBRUpdateInterval>(m_vbrInterval->value());

    const SelectionDisplay option
        = m_preferPlaying->isChecked() ? SelectionDisplay::PreferPlaying : SelectionDisplay::PreferSelection;
    m_settings->set<InfoDisplayPrefer>(static_cast<int>(option));

    m_settings->set<SeekStep>(m_seekStep->value());
    m_settings->set<VolumeStep>(m_volumeStep->value());
    m_settings->set<StarRatingSize>(m_starRatingSize->value());
}

void GuiGeneralPageWidget::reset()
{
    m_settings->reset<Style>();
    m_settings->reset<IconTheme>();
    m_settings->reset<ShowSplitterHandles>();
    m_settings->reset<LockSplitterHandles>();
    m_settings->reset<EditableLayoutMargin>();
    m_settings->reset<SplitterHandleSize>();
    m_settings->reset<WindowTitleTrackScript>();
    m_settings->reset<Settings::Core::Internal::VBRUpdateInterval>();
    m_settings->reset<InfoDisplayPrefer>();
    m_settings->reset<SeekStep>();
    m_settings->reset<VolumeStep>();
    m_settings->reset<StarRatingSize>();
}

void GuiGeneralPageWidget::showQuickSetup()
{
    auto* quickSetup = new QuickSetupDialog(m_layoutProvider, this);
    quickSetup->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(quickSetup, &QuickSetupDialog::layoutChanged, m_editableLayout, &EditableLayout::changeLayout);
    quickSetup->show();
}

void GuiGeneralPageWidget::importLayout()
{
    m_layoutProvider->importLayout(this);
}

void GuiGeneralPageWidget::exportLayout()
{
    m_editableLayout->exportLayout(this);
}

GuiGeneralPage::GuiGeneralPage(LayoutProvider* layoutProvider, EditableLayout* editableLayout,
                               SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::InterfaceGeneral);
    setName(tr("General"));
    setCategory({tr("Interface")});
    setWidgetCreator([layoutProvider, editableLayout, settings] {
        return new GuiGeneralPageWidget(layoutProvider, editableLayout, settings);
    });
}
} // namespace Fooyin

#include "guigeneralpage.moc"
#include "moc_guigeneralpage.cpp"
