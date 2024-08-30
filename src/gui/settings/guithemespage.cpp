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

#include "guithemespage.h"

#include "guiutils.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/theme/fytheme.h>
#include <gui/theme/themeregistry.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>
#include <utils/widgets/colourbutton.h>
#include <utils/widgets/fontbutton.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>

namespace Fooyin {
class GuiColoursPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit GuiColoursPageWidget(ThemeRegistry* themeRegistry, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void addColourOption(QGridLayout* layout, const QString& title, QPalette::ColorRole role,
                         QPalette::ColorGroup group = QPalette::All);
    void addFontOption(QGridLayout* layout, const QString& title, const QString& className);

    void updateButtonState() const;
    [[nodiscard]] FyTheme currentTheme() const;
    void loadDefaults();
    void loadCurrentTheme();

    void saveTheme();
    void loadTheme();
    void deleteTheme();

    ThemeRegistry* m_themeRegistry;
    SettingsManager* m_settings;

    QComboBox* m_themesBox;
    QPushButton* m_loadButton;
    QPushButton* m_saveButton;
    QPushButton* m_deleteButton;

    QString m_defaultTheme;
    std::map<PaletteKey, std::pair<ColourButton*, QCheckBox*>> m_colourMapping;
    std::map<QString, std::pair<FontButton*, QCheckBox*>> m_fontMapping;
};

GuiColoursPageWidget::GuiColoursPageWidget(ThemeRegistry* themeRegistry, SettingsManager* settings)
    : m_themeRegistry{themeRegistry}
    , m_settings{settings}
    , m_themesBox{new QComboBox(this)}
    , m_loadButton{new QPushButton(tr("&Load"), this)}
    , m_saveButton{new QPushButton(tr("&Save"), this)}
    , m_deleteButton{new QPushButton(tr("&Delete"), this)}
    , m_defaultTheme{tr("System default")}
{
    auto* themesGroup  = new QGroupBox(tr("Themes"), this);
    auto* themesLayout = new QGridLayout(themesGroup);

    m_themesBox->setEditable(true);

    QObject::connect(m_loadButton, &QPushButton::clicked, this, &GuiColoursPageWidget::loadTheme);
    QObject::connect(m_saveButton, &QPushButton::clicked, this, &GuiColoursPageWidget::saveTheme);
    QObject::connect(m_deleteButton, &QPushButton::clicked, this, &GuiColoursPageWidget::deleteTheme);
    QObject::connect(m_themesBox, &QComboBox::currentTextChanged, this, &GuiColoursPageWidget::updateButtonState);

    themesLayout->addWidget(m_themesBox, 0, 0, 1, 3);
    themesLayout->addWidget(m_loadButton, 1, 0);
    themesLayout->addWidget(m_saveButton, 1, 1);
    themesLayout->addWidget(m_deleteButton, 1, 2);

    auto* baseColours = new QWidget(this);
    auto* baseLayout  = new QGridLayout(baseColours);

    addColourOption(baseLayout, tr("Background"), QPalette::Window);
    addColourOption(baseLayout, tr("Foreground"), QPalette::WindowText);
    addColourOption(baseLayout, tr("Foreground (Bright)"), QPalette::BrightText);
    addColourOption(baseLayout, tr("Text"), QPalette::Text);
    addColourOption(baseLayout, tr("Placeholder text"), QPalette::PlaceholderText);
    addColourOption(baseLayout, tr("Base"), QPalette::Base);
    addColourOption(baseLayout, tr("Base (Alternate)"), QPalette::AlternateBase);
    addColourOption(baseLayout, tr("Highlight"), QPalette::Highlight);
    addColourOption(baseLayout, tr("Highlighted text"), QPalette::HighlightedText);
    addColourOption(baseLayout, tr("Button (Background)"), QPalette::Button);
    addColourOption(baseLayout, tr("Button (Foreground)"), QPalette::ButtonText);

    baseLayout->setColumnStretch(1, 1);
    baseLayout->setRowStretch(baseLayout->rowCount(), 1);

    auto* advancedColours = new QWidget(this);
    auto* advancedLayout  = new QGridLayout(advancedColours);

    addColourOption(advancedLayout, tr("Foreground (Disabled)"), QPalette::WindowText, QPalette::Disabled);
    addColourOption(advancedLayout, tr("Highlight (Disabled)"), QPalette::Highlight, QPalette::Disabled);
    addColourOption(advancedLayout, tr("Highlighted Text (Disabled)"), QPalette::HighlightedText, QPalette::Disabled);
    addColourOption(advancedLayout, tr("Text (Disabled)"), QPalette::Text, QPalette::Disabled);
    addColourOption(advancedLayout, tr("Button Text (Disabled)"), QPalette::ButtonText, QPalette::Disabled);
    addColourOption(advancedLayout, tr("ToolTip (Background)"), QPalette::ToolTipBase);
    addColourOption(advancedLayout, tr("ToolTip (Foreground)"), QPalette::ToolTipText);
    addColourOption(advancedLayout, tr("Link"), QPalette::Link);
    addColourOption(advancedLayout, tr("Link (Visited)"), QPalette::LinkVisited);
    addColourOption(advancedLayout, tr("Light"), QPalette::Light);
    addColourOption(advancedLayout, tr("Midlight"), QPalette::Midlight);
    addColourOption(advancedLayout, tr("Dark"), QPalette::Dark);
    addColourOption(advancedLayout, tr("Mid"), QPalette::Mid);
    addColourOption(advancedLayout, tr("Shadow"), QPalette::Shadow);

    advancedLayout->setColumnStretch(1, 1);
    advancedLayout->setRowStretch(advancedLayout->rowCount(), 1);

    auto* fonts       = new QWidget(this);
    auto* fontsLayout = new QGridLayout(fonts);

    const auto fontEntries = m_themeRegistry->fontEntries();

    std::map<QString, QString> sortedEntries;
    for(const auto& [className, title] : fontEntries) {
        sortedEntries.emplace(title, className);
    }
    for(const auto& [title, className] : sortedEntries) {
        addFontOption(fontsLayout, title, className);
    }

    fontsLayout->setColumnStretch(1, 1);
    fontsLayout->setRowStretch(baseLayout->rowCount(), 1);

    auto* themeDetails = new QTabWidget(this);
    themeDetails->addTab(baseColours, tr("Basic Colours"));
    themeDetails->addTab(advancedColours, tr("Advanced Colours"));
    themeDetails->addTab(fonts, tr("Fonts"));

    auto* layout = new QGridLayout(this);

    int row{0};
    layout->addWidget(themesGroup, row++, 0, 1, 2);
    layout->addWidget(themeDetails, row++, 0, 1, 2);
}

void GuiColoursPageWidget::load()
{
    m_themesBox->clear();

    m_themesBox->addItem(m_defaultTheme, 1);

    const auto themes = m_themeRegistry->items();
    for(const auto& theme : themes) {
        m_themesBox->addItem(theme.name);
    }

    loadDefaults();
    loadCurrentTheme();
}

void GuiColoursPageWidget::apply()
{
    const FyTheme theme = currentTheme();
    m_settings->set<Settings::Gui::Theme>(QVariant::fromValue(theme));
    loadDefaults();
    loadCurrentTheme();
}

void GuiColoursPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Theme>();
}

void GuiColoursPageWidget::addColourOption(QGridLayout* layout, const QString& title, QPalette::ColorRole role,
                                           QPalette::ColorGroup group)
{
    auto* option = new QCheckBox(title, this);
    auto* colour = new ColourButton(this);
    colour->setDisabled(true);

    const int row = layout->rowCount();
    layout->addWidget(option, row, 0);
    layout->addWidget(colour, row, 1);

    QObject::connect(option, &QCheckBox::toggled, colour, &QWidget::setEnabled);
    m_colourMapping[PaletteKey{role, group}] = std::make_pair(colour, option);
}

void GuiColoursPageWidget::addFontOption(QGridLayout* layout, const QString& title, const QString& className)
{
    auto* option = new QCheckBox(title, this);
    auto* font   = new FontButton(this);
    font->setDisabled(true);

    const int row = layout->rowCount();
    layout->addWidget(option, row, 0);
    layout->addWidget(font, row, 1);

    QObject::connect(option, &QCheckBox::toggled, font, &QWidget::setEnabled);
    m_fontMapping[className] = std::make_pair(font, option);
}

void GuiColoursPageWidget::updateButtonState() const
{
    const auto currentItem = m_themeRegistry->itemByName(m_themesBox->currentText());
    const bool isDefault   = m_themesBox->currentText() == m_defaultTheme || (currentItem && currentItem->isDefault);

    m_loadButton->setEnabled(currentItem.has_value() || isDefault);
    m_saveButton->setEnabled(!m_themesBox->currentText().isEmpty() && !isDefault);
    m_deleteButton->setEnabled(!isDefault);
}

FyTheme GuiColoursPageWidget::currentTheme() const
{
    FyTheme theme;

    for(const auto& [key, pair] : m_colourMapping) {
        const auto& [button, option] = pair;
        if(option->isChecked()) {
            theme.colours[key] = button->colour();
        }
    }
    for(const auto& [key, pair] : m_fontMapping) {
        const auto& [button, option] = pair;
        if(option->isChecked()) {
            theme.fonts[key] = button->buttonFont();
        }
    }

    return theme;
}

void GuiColoursPageWidget::loadDefaults()
{
    const auto currentColours = Gui::coloursFromStylePalette();
    for(const auto& [key, colour] : Utils::asRange(currentColours)) {
        if(m_colourMapping.contains(key)) {
            const auto& [button, option] = m_colourMapping.at(key);
            button->setColour(colour);
            option->setChecked(false);
        }
    }

    for(const auto& [className, font] : m_fontMapping) {
        const auto& [button, option] = font;
        if(className.isEmpty()) {
            button->setButtonFont(QApplication::font());
        }
        else {
            button->setButtonFont(QApplication::font(className.toUtf8().constData()));
        }
        option->setChecked(false);
    }
}

void GuiColoursPageWidget::loadCurrentTheme()
{
    const auto currentTheme = m_settings->value<Settings::Gui::Theme>().value<FyTheme>();
    if(currentTheme.isValid()) {
        for(const auto& [key, colour] : Utils::asRange(currentTheme.colours)) {
            if(m_colourMapping.contains(key)) {
                const auto& [button, option] = m_colourMapping.at(key);
                button->setColour(colour);
                option->setChecked(true);
            }
        }
        for(const auto& [key, font] : Utils::asRange(currentTheme.fonts)) {
            if(m_fontMapping.contains(key)) {
                const auto& [button, option] = m_fontMapping.at(key);
                button->setButtonFont(font);
                option->setChecked(true);
            }
        }
    }
}

void GuiColoursPageWidget::saveTheme()
{
    const QString name = m_themesBox->currentText();

    FyTheme theme = currentTheme();
    theme.name    = name;

    const auto existingTheme = m_themeRegistry->itemByName(name);
    if(existingTheme) {
        QMessageBox msg{QMessageBox::Question, tr("Theme already exists"),
                        tr("Theme %1 already exists. Overwrite?").arg(name), QMessageBox::Yes | QMessageBox::No};
        if(msg.exec() == QMessageBox::Yes) {
            theme.id    = existingTheme->id;
            theme.index = existingTheme->index;
            m_themeRegistry->changeItem(theme);
            m_themesBox->setCurrentIndex(m_themesBox->findText(name));
        }
    }
    else {
        const auto newItem = m_themeRegistry->addItem(theme);
        m_themesBox->addItem(newItem.name);
        m_themesBox->setCurrentIndex(m_themesBox->count() - 1);
        updateButtonState();
    }
}

void GuiColoursPageWidget::loadTheme()
{
    if(m_themesBox->count() == 0) {
        return;
    }

    if(m_themesBox->currentData().toInt() == 1) {
        loadDefaults();
        return;
    }

    const auto theme = m_themeRegistry->itemByName(m_themesBox->currentText());
    if(!theme) {
        return;
    }

    for(const auto& [key, pair] : m_colourMapping) {
        if(theme->colours.contains(key)) {
            const auto& [button, option] = pair;
            button->setColour(theme->colours.value(key));
            option->setChecked(true);
        }
    }
    for(const auto& [key, pair] : m_fontMapping) {
        if(theme->fonts.contains(key)) {
            const auto& [button, option] = pair;
            button->setButtonFont(theme->fonts.value(key));
            option->setChecked(true);
        }
    }
}

void GuiColoursPageWidget::deleteTheme()
{
    if(m_themesBox->count() == 0) {
        return;
    }

    const QString name = m_themesBox->currentText();

    const auto theme = m_themeRegistry->itemByName(m_themesBox->currentText());
    if(theme && !theme->isDefault) {
        m_themeRegistry->removeById(theme->id);
        m_themesBox->removeItem(m_themesBox->findText(name));
    }
}

GuiThemesPage::GuiThemesPage(ThemeRegistry* colourRegistry, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::InterfaceTheme);
    setName(tr("Themes"));
    setCategory({tr("Interface"), tr("Colours & Fonts")});
    setWidgetCreator([colourRegistry, settings] { return new GuiColoursPageWidget(colourRegistry, settings); });
}
} // namespace Fooyin

#include "guithemespage.moc"
#include "moc_guithemespage.cpp"
