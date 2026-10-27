/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/layout/editablelayout.h>
#include <gui/layout/layoutprovider.h>
#include <gui/theme/fytheme.h>
#include <gui/theme/themeregistry.h>
#include <playlist/playlistwidget.h>
#include <playlist/presetregistry.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
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
                                  ThemeRegistry* themeRegistry, PresetRegistry* presetRegistry,
                                  SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void showQuickSetup();
    void importLayout();
    void exportLayout();
    void updateTransitionOptions();

#ifdef Q_OS_WIN
    void updateDarkModeState();
#endif

    LayoutProvider* m_layoutProvider;
    EditableLayout* m_editableLayout;
    ThemeRegistry* m_themeRegistry;
    PresetRegistry* m_presetRegistry;
    SettingsManager* m_settings;

    QComboBox* m_styles;

#ifdef Q_OS_WIN
    QCheckBox* m_darkMode;
#endif

    QRadioButton* m_detectIconTheme;
    QRadioButton* m_lightTheme;
    QRadioButton* m_darkTheme;
    QRadioButton* m_systemTheme;
    QComboBox* m_applicationIcon;
    QCheckBox* m_useCustomApplicationIcon;
    QLineEdit* m_customApplicationIcon;
    QPushButton* m_browseApplicationIcon;

    QCheckBox* m_showMenuBar;
    QCheckBox* m_menuBarMovable;
    QGroupBox* m_layoutAnimation;
    QSpinBox* m_layoutTransitionDuration;
    QComboBox* m_layoutTransitionMethod;
    QLabel* m_slideDirectionLabel;
    QComboBox* m_slideDirection;
    QLabel* m_scaleDirectionLabel;
    QComboBox* m_scaleDirection;
    QLabel* m_scaleAmountLabel;
    QSpinBox* m_scaleAmount;

    QCheckBox* m_overrideMargin;
    QSpinBox* m_editableLayoutMargin;

    QCheckBox* m_splitterHandles;
    QCheckBox* m_lockSplitters;
    QCheckBox* m_lockedWidgetsResizeAdjacentOnly;
    QCheckBox* m_overrideSplitterHandle;
    QSpinBox* m_splitterHandleGap;

    QCheckBox* m_buttonRaise;
    QCheckBox* m_buttonStretch;
};

GuiGeneralPageWidget::GuiGeneralPageWidget(LayoutProvider* layoutProvider, EditableLayout* editableLayout,
                                           ThemeRegistry* themeRegistry, PresetRegistry* presetRegistry,
                                           SettingsManager* settings)
    : m_layoutProvider{layoutProvider}
    , m_editableLayout{editableLayout}
    , m_themeRegistry{themeRegistry}
    , m_presetRegistry{presetRegistry}
    , m_settings{settings}
    , m_styles{new QComboBox(this)}
#ifdef Q_OS_WIN
    , m_darkMode{new QCheckBox(tr("Dark mode"), this)}
#endif
    , m_detectIconTheme{new QRadioButton(tr("Auto-detect theme"), this)}
    , m_lightTheme{new QRadioButton(tr("Light"), this)}
    , m_darkTheme{new QRadioButton(tr("Dark"), this)}
    , m_systemTheme{new QRadioButton(tr("Use system icons"), this)}
    , m_applicationIcon{new QComboBox(this)}
    , m_useCustomApplicationIcon{new QCheckBox(tr("Custom icon") + u":"_s, this)}
    , m_customApplicationIcon{new QLineEdit(this)}
    , m_browseApplicationIcon{new QPushButton(tr("Browse…"), this)}
    , m_showMenuBar{new QCheckBox(tr("Show menu bar"), this)}
    , m_menuBarMovable{new QCheckBox(tr("Place menu bar in a toolbar"), this)}
    , m_layoutAnimation{new QGroupBox(tr("Layout animation"), this)}
    , m_layoutTransitionDuration{new QSpinBox(this)}
    , m_layoutTransitionMethod{new QComboBox(this)}
    , m_slideDirectionLabel{new QLabel(tr("Enter from") + u":"_s, this)}
    , m_slideDirection{new QComboBox(this)}
    , m_scaleDirectionLabel{new QLabel(tr("Scale direction") + u":"_s, this)}
    , m_scaleDirection{new QComboBox(this)}
    , m_scaleAmountLabel{new QLabel(tr("Scale amount") + u":"_s, this)}
    , m_scaleAmount{new QSpinBox(this)}
    , m_overrideMargin{new QCheckBox(tr("Override root margin") + u":"_s, this)}
    , m_editableLayoutMargin{new QSpinBox(this)}
    , m_splitterHandles{new QCheckBox(tr("Show splitter handles"), this)}
    , m_lockSplitters{new QCheckBox(tr("Lock splitters"), this)}
    , m_lockedWidgetsResizeAdjacentOnly{new QCheckBox(tr("Only resize locked widgets using adjacent handles"), this)}
    , m_overrideSplitterHandle{new QCheckBox(tr("Override splitter handle size") + u":"_s, this)}
    , m_splitterHandleGap{new QSpinBox(this)}
    , m_buttonRaise{new QCheckBox(tr("Raise"), this)}
    , m_buttonStretch{new QCheckBox(tr("Stretch"), this)}
{
    auto* setupBox        = new QGroupBox(tr("Setup"), this);
    auto* setupBoxLayout  = new QHBoxLayout(setupBox);
    auto* quickSetup      = new QPushButton(tr("Quick Setup"), this);
    auto* importLayoutBtn = new QPushButton(tr("Import Layout"), this);
    auto* exportLayoutBtn = new QPushButton(tr("Export Layout"), this);

    setupBoxLayout->addWidget(quickSetup);
    setupBoxLayout->addWidget(importLayoutBtn);
    setupBoxLayout->addWidget(exportLayoutBtn);

    auto* appearanceGroup       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceGroupLayout = new QGridLayout(appearanceGroup);

    auto* iconThemeBox       = new QGroupBox(tr("Icons"), appearanceGroup);
    auto* iconThemeBoxLayout = new QGridLayout(iconThemeBox);

    int row{0};
    iconThemeBoxLayout->addWidget(m_detectIconTheme, row++, 0, 1, 2);
    iconThemeBoxLayout->addWidget(m_lightTheme, row, 0);
    iconThemeBoxLayout->addWidget(m_darkTheme, row++, 1);
    iconThemeBoxLayout->addWidget(m_systemTheme, row++, 0, 1, 2);
    iconThemeBoxLayout->setColumnStretch(2, 1);

    m_applicationIcon->addItem(QIcon{u":/icons/32-fooyin.png"_s}, tr("Dragon"),
                               static_cast<int>(Gui::ApplicationIconOption::Dragon));
    m_applicationIcon->addItem(QIcon{u":/icons/32-fooyin-typographic.png"_s}, tr("Typographic"),
                               static_cast<int>(Gui::ApplicationIconOption::Typographic));

    iconThemeBoxLayout->addWidget(new QLabel(tr("Application icon") + u":"_s, iconThemeBox), 0, 3);
    iconThemeBoxLayout->addWidget(m_applicationIcon, 0, 4, 1, 2);
    iconThemeBoxLayout->addWidget(m_useCustomApplicationIcon, 1, 3);
    iconThemeBoxLayout->addWidget(m_customApplicationIcon, 1, 4);
    iconThemeBoxLayout->addWidget(m_browseApplicationIcon, 1, 5);
    iconThemeBoxLayout->setColumnStretch(4, 1);

    m_customApplicationIcon->setEnabled(false);
    m_browseApplicationIcon->setEnabled(false);

    row = 0;
    appearanceGroupLayout->addWidget(new QLabel(tr("Style") + u":"_s, appearanceGroup), row, 0);
    appearanceGroupLayout->addWidget(m_styles, row++, 1);
#ifdef Q_OS_WIN
    appearanceGroupLayout->addWidget(m_darkMode, row++, 0, 1, 2);
#endif
    appearanceGroupLayout->addWidget(m_showMenuBar, row++, 0, 1, 2);
    appearanceGroupLayout->addWidget(m_menuBarMovable, row++, 0, 1, 2);
    appearanceGroupLayout->setColumnStretch(1, 1);

    m_layoutTransitionDuration->setRange(50, 3000);
    m_layoutTransitionDuration->setSingleStep(50);
    m_layoutTransitionDuration->setSuffix(u" ms"_s);

    m_layoutTransitionMethod->addItem(tr("Crossfade"), static_cast<int>(LayoutTransitionType::CrossFade));
    m_layoutTransitionMethod->addItem(tr("Fade through background"),
                                      static_cast<int>(LayoutTransitionType::FadeThroughBackground));
    m_layoutTransitionMethod->addItem(tr("Scale & fade"), static_cast<int>(LayoutTransitionType::ScaleAndFade));
    m_layoutTransitionMethod->addItem(tr("Slide & fade"), static_cast<int>(LayoutTransitionType::SlideAndFade));

    m_slideDirection->addItem(tr("Automatic"), static_cast<int>(LayoutSlideDirection::Automatic));
    m_slideDirection->addItem(tr("Left"), static_cast<int>(LayoutSlideDirection::Left));
    m_slideDirection->addItem(tr("Right"), static_cast<int>(LayoutSlideDirection::Right));
    m_slideDirection->addItem(tr("Top"), static_cast<int>(LayoutSlideDirection::Top));
    m_slideDirection->addItem(tr("Bottom"), static_cast<int>(LayoutSlideDirection::Bottom));
    m_slideDirection->setToolTip(tr("Automatic follows the interface’s reading direction."));

    m_scaleDirection->addItem(tr("Grow"), static_cast<int>(LayoutScaleDirection::Grow));
    m_scaleDirection->addItem(tr("Shrink"), static_cast<int>(LayoutScaleDirection::Shrink));
    m_scaleAmount->setRange(1, 100);
    m_scaleAmount->setSuffix(u" %"_s);
    m_scaleAmountLabel->setBuddy(m_scaleAmount);

    m_layoutAnimation->setCheckable(true);
    auto* animationLayout = new QGridLayout(m_layoutAnimation);

    row = 0;
    animationLayout->addWidget(new QLabel(tr("Effect") + u":"_s, m_layoutAnimation), row, 0);
    animationLayout->addWidget(m_layoutTransitionMethod, row++, 1);
    animationLayout->addWidget(new QLabel(tr("Duration") + u":"_s, m_layoutAnimation), row, 0);
    animationLayout->addWidget(m_layoutTransitionDuration, row++, 1);
    animationLayout->addWidget(m_slideDirectionLabel, row, 0);
    animationLayout->addWidget(m_slideDirection, row++, 1);
    animationLayout->addWidget(m_scaleDirectionLabel, row, 0);
    animationLayout->addWidget(m_scaleDirection, row++, 1);
    animationLayout->addWidget(m_scaleAmountLabel, row, 0);
    animationLayout->addWidget(m_scaleAmount, row++, 1);
    animationLayout->setColumnStretch(2, 1);

    QObject::connect(m_layoutTransitionMethod, &QComboBox::currentIndexChanged, this,
                     &GuiGeneralPageWidget::updateTransitionOptions);
    updateTransitionOptions();

    auto* layoutGroup  = new QGroupBox(tr("Layout"), this);
    auto* layoutLayout = new QGridLayout(layoutGroup);

    row = 0;
    layoutLayout->addWidget(m_splitterHandles, row++, 0, 1, 3);
    layoutLayout->addWidget(m_lockSplitters, row++, 0, 1, 3);
    layoutLayout->addWidget(m_lockedWidgetsResizeAdjacentOnly, row++, 0, 1, 3);
    layoutLayout->addWidget(m_overrideSplitterHandle, row, 0);
    layoutLayout->addWidget(m_splitterHandleGap, row++, 1);
    layoutLayout->addWidget(m_overrideMargin, row, 0);
    layoutLayout->addWidget(m_editableLayoutMargin, row++, 1);
    layoutLayout->setRowStretch(row, 1);
    layoutLayout->setColumnStretch(2, 1);

    m_editableLayoutMargin->setRange(0, 20);
    m_editableLayoutMargin->setSuffix(u" px"_s);

    m_splitterHandleGap->setRange(0, 20);
    m_splitterHandleGap->setSuffix(u" px"_s);

    m_lockedWidgetsResizeAdjacentOnly->setToolTip(
        tr("Prevent other splitter handles and parent splitters from changing the size of a locked widget."));

    auto* toolButtonGroup       = new QGroupBox(tr("Tool Buttons"), this);
    auto* toolButtonGroupLayout = new QVBoxLayout(toolButtonGroup);

    toolButtonGroupLayout->addWidget(m_buttonRaise);
    toolButtonGroupLayout->addWidget(m_buttonStretch);

    auto* mainLayout = new QGridLayout(this);

    row = 0;
    mainLayout->addWidget(setupBox, row++, 0, 1, 2);
    mainLayout->addWidget(appearanceGroup, row, 0);
    mainLayout->addWidget(layoutGroup, row++, 1);
    mainLayout->addWidget(iconThemeBox, row++, 0, 1, 2);
    mainLayout->addWidget(m_layoutAnimation, row, 0);
    mainLayout->addWidget(toolButtonGroup, row++, 1);
    mainLayout->setColumnStretch(0, 1);
    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    QObject::connect(quickSetup, &QPushButton::clicked, this, &GuiGeneralPageWidget::showQuickSetup);
    QObject::connect(importLayoutBtn, &QPushButton::clicked, this, &GuiGeneralPageWidget::importLayout);
    QObject::connect(exportLayoutBtn, &QPushButton::clicked, this, &GuiGeneralPageWidget::exportLayout);

    QObject::connect(m_overrideMargin, &QCheckBox::toggled, m_editableLayoutMargin, &QWidget::setEnabled);

    QObject::connect(m_useCustomApplicationIcon, &QCheckBox::toggled, this, [this](bool custom) {
        m_applicationIcon->setEnabled(!custom);
        m_customApplicationIcon->setEnabled(custom);
        m_browseApplicationIcon->setEnabled(custom);
    });
    QObject::connect(m_browseApplicationIcon, &QPushButton::clicked, this, [this]() {
        const QString path
            = QFileDialog::getOpenFileName(this, tr("Choose application icon"), m_customApplicationIcon->text(),
                                           tr("Images (*.png *.jpg *.jpeg *.svg *.ico);;All files (*)"));
        if(!path.isEmpty()) {
            m_customApplicationIcon->setText(path);
        }
    });

    QObject::connect(m_layoutTransitionMethod, &QComboBox::currentIndexChanged, this,
                     &GuiGeneralPageWidget::updateTransitionOptions);

    QObject::connect(m_overrideSplitterHandle, &QCheckBox::toggled, m_splitterHandleGap, &QWidget::setEnabled);
#ifdef Q_OS_WIN
    QObject::connect(m_styles, &QComboBox::currentTextChanged, this, &GuiGeneralPageWidget::updateDarkModeState);
#endif

    m_settings->subscribe<ShowMenuBar>(m_showMenuBar, &QCheckBox::setChecked);
    m_settings->subscribe<MenuBarMovable>(m_menuBarMovable, &QCheckBox::setChecked);
    m_settings->subscribe<LockSplitterHandles>(m_lockSplitters, &QCheckBox::setChecked);
}

void GuiGeneralPageWidget::load()
{
    m_styles->clear();
    m_styles->addItem(tr("System default"));
    m_styles->addItems(QStyleFactory::keys());

    const auto style     = m_settings->value<Style>();
    const int styleIndex = m_styles->findText(style);
    m_styles->setCurrentIndex(style.isEmpty() || styleIndex < 0 ? 0 : styleIndex);
#ifdef Q_OS_WIN
    m_darkMode->setChecked(m_settings->value<DarkMode>());
    updateDarkModeState();
#endif

    const auto iconTheme = static_cast<IconThemeOption>(m_settings->value<IconTheme>());
    switch(iconTheme) {
        case IconThemeOption::AutoDetect:
            m_detectIconTheme->setChecked(true);
            break;
        case IconThemeOption::System:
            m_systemTheme->setChecked(true);
            break;
        case IconThemeOption::Light:
            m_lightTheme->setChecked(true);
            break;
        case IconThemeOption::Dark:
            m_darkTheme->setChecked(true);
            break;
    }

    const int applicationIconIndex = m_applicationIcon->findData(m_settings->value<ApplicationIcon>());
    m_applicationIcon->setCurrentIndex(applicationIconIndex >= 0 ? applicationIconIndex : 0);
    m_useCustomApplicationIcon->setChecked(m_settings->value<ApplicationIcon>()
                                           == static_cast<int>(Gui::ApplicationIconOption::Custom));
    m_customApplicationIcon->setText(m_settings->value<CustomApplicationIcon>());

    m_showMenuBar->setChecked(m_settings->value<ShowMenuBar>());
    m_menuBarMovable->setChecked(m_settings->value<MenuBarMovable>());

    m_layoutAnimation->setChecked(
        m_settings->fileValue(LayoutTransitionEnabled, DefaultLayoutTransitionEnabled).toBool());
    m_layoutTransitionDuration->setValue(
        m_settings->fileValue(LayoutTransitionDuration, DefaultLayoutTransitionDuration).toInt());
    const int defaultMethod    = static_cast<int>(DefaultLayoutTransitionMethod);
    const int transitionMethod = m_settings->fileValue(LayoutTransitionMethod, defaultMethod).toInt();
    const int methodIndex      = m_layoutTransitionMethod->findData(transitionMethod);
    m_layoutTransitionMethod->setCurrentIndex(methodIndex >= 0 ? methodIndex
                                                               : m_layoutTransitionMethod->findData(defaultMethod));
    const int defaultSlide = static_cast<int>(DefaultLayoutTransitionSlideDirection);
    const int slideIndex
        = m_slideDirection->findData(m_settings->fileValue(LayoutTransitionSlideDirection, defaultSlide).toInt());
    m_slideDirection->setCurrentIndex(slideIndex >= 0 ? slideIndex : m_slideDirection->findData(defaultSlide));
    const int defaultScale = static_cast<int>(DefaultLayoutTransitionScaleDirection);
    const int scaleIndex
        = m_scaleDirection->findData(m_settings->fileValue(LayoutTransitionScaleDirection, defaultScale).toInt());
    m_scaleDirection->setCurrentIndex(scaleIndex >= 0 ? scaleIndex : m_scaleDirection->findData(defaultScale));
    m_scaleAmount->setValue(
        m_settings->fileValue(LayoutTransitionScaleAmount, DefaultLayoutTransitionScaleAmount).toInt());
    updateTransitionOptions();

    m_splitterHandles->setChecked(m_settings->value<ShowSplitterHandles>());
    m_lockSplitters->setChecked(m_settings->value<LockSplitterHandles>());
    m_lockedWidgetsResizeAdjacentOnly->setChecked(m_settings->value<ResizeLockedAdjacentOnly>());

    m_overrideMargin->setChecked(m_settings->value<EditableLayoutMargin>() >= 0);
    m_editableLayoutMargin->setValue(m_settings->value<EditableLayoutMargin>());
    m_editableLayoutMargin->setEnabled(m_overrideMargin->isChecked());

    m_overrideSplitterHandle->setChecked(m_settings->value<SplitterHandleSize>() >= 0);
    m_splitterHandleGap->setValue(m_settings->value<SplitterHandleSize>());
    m_splitterHandleGap->setEnabled(m_overrideSplitterHandle->isChecked());

    const auto buttonOptions = m_settings->value<ToolButtonStyle>();
    m_buttonRaise->setChecked(buttonOptions & Raise);
    m_buttonStretch->setChecked(buttonOptions & Stretch);
}

void GuiGeneralPageWidget::apply()
{
    m_settings->set<Style>(m_styles->currentIndex() == 0 ? QString{} : m_styles->currentText());
#ifdef Q_OS_WIN
    m_settings->set<DarkMode>(m_darkMode->isChecked());
#endif

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
    m_settings->set<CustomApplicationIcon>(m_customApplicationIcon->text());
    m_settings->set<ApplicationIcon>(m_useCustomApplicationIcon->isChecked()
                                         ? static_cast<int>(Gui::ApplicationIconOption::Custom)
                                         : m_applicationIcon->currentData().toInt());

    m_settings->set<ShowMenuBar>(m_showMenuBar->isChecked());
    m_settings->set<MenuBarMovable>(m_menuBarMovable->isChecked());

    m_settings->fileSet(LayoutTransitionEnabled, m_layoutAnimation->isChecked());
    m_settings->fileSet(LayoutTransitionDuration, m_layoutTransitionDuration->value());
    m_settings->fileSet(LayoutTransitionMethod, m_layoutTransitionMethod->currentData().toInt());
    m_settings->fileSet(LayoutTransitionSlideDirection, m_slideDirection->currentData().toInt());
    m_settings->fileSet(LayoutTransitionScaleDirection, m_scaleDirection->currentData().toInt());
    m_settings->fileSet(LayoutTransitionScaleAmount, m_scaleAmount->value());

    m_settings->set<ShowSplitterHandles>(m_splitterHandles->isChecked());
    m_settings->set<LockSplitterHandles>(m_lockSplitters->isChecked());
    m_settings->set<ResizeLockedAdjacentOnly>(m_lockedWidgetsResizeAdjacentOnly->isChecked());

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
}

void GuiGeneralPageWidget::reset()
{
    m_settings->reset<Style>();
#ifdef Q_OS_WIN
    m_settings->reset<DarkMode>();
#endif
    m_settings->reset<IconTheme>();
    m_settings->reset<ApplicationIcon>();
    m_settings->reset<CustomApplicationIcon>();
    m_settings->reset<ShowMenuBar>();
    m_settings->reset<MenuBarMovable>();
    m_settings->fileRemove(LayoutTransitionEnabled);
    m_settings->fileRemove(LayoutTransitionDuration);
    m_settings->fileRemove(LayoutTransitionMethod);
    m_settings->fileRemove(LayoutTransitionSlideDirection);
    m_settings->fileRemove(LayoutTransitionScaleDirection);
    m_settings->fileRemove(LayoutTransitionScaleAmount);
    m_settings->reset<ShowSplitterHandles>();
    m_settings->reset<LockSplitterHandles>();
    m_settings->reset<ResizeLockedAdjacentOnly>();
    m_settings->reset<EditableLayoutMargin>();
    m_settings->reset<SplitterHandleSize>();
    m_settings->reset<ToolButtonStyle>();
}

#ifdef Q_OS_WIN
void GuiGeneralPageWidget::updateDarkModeState()
{
    const QString styleName
        = m_styles->currentIndex() == 0 ? m_settings->value<SystemStyle>() : m_styles->currentText();
    const bool supported = Gui::styleSupportsDarkMode(styleName);
    m_darkMode->setEnabled(supported);
    m_darkMode->setToolTip(supported ? QString{} : tr("Dark mode is not supported by this style."));
}
#endif

void GuiGeneralPageWidget::showQuickSetup()
{
    auto* quickSetup
        = new QuickSetupDialog(m_layoutProvider, m_themeRegistry, m_presetRegistry, m_editableLayout, m_settings, this);
    quickSetup->setAttribute(Qt::WA_DeleteOnClose);
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

void GuiGeneralPageWidget::updateTransitionOptions()
{
    const auto method = static_cast<LayoutTransitionType>(m_layoutTransitionMethod->currentData().toInt());
    const bool slide  = method == LayoutTransitionType::SlideAndFade;
    const bool scale  = method == LayoutTransitionType::ScaleAndFade;

    m_slideDirectionLabel->setVisible(slide);
    m_slideDirection->setVisible(slide);
    m_scaleDirectionLabel->setVisible(scale);
    m_scaleDirection->setVisible(scale);
    m_scaleAmountLabel->setVisible(scale);
    m_scaleAmount->setVisible(scale);
}

GuiGeneralPage::GuiGeneralPage(LayoutProvider* layoutProvider, EditableLayout* editableLayout,
                               ThemeRegistry* themeRegistry, PresetRegistry* presetRegistry, SettingsManager* settings,
                               QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::InterfaceGeneral);
    setName(tr("General"));
    setCategory({tr("Interface")});
    setWidgetCreator([layoutProvider, editableLayout, themeRegistry, presetRegistry, settings] {
        return new GuiGeneralPageWidget(layoutProvider, editableLayout, themeRegistry, presetRegistry, settings);
    });
}
} // namespace Fooyin

#include "guigeneralpage.moc"
#include "moc_guigeneralpage.cpp"
