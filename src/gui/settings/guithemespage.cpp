/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <gui/internalguisettings.h>
#include <gui/theme/fytheme.h>
#include <gui/theme/themeregistry.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/fontbutton.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>

#include <optional>

constexpr auto ThemeIdRole = Qt::UserRole;

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
    [[nodiscard]] std::optional<FyTheme> selectedTheme() const;
    [[nodiscard]] FyTheme currentTheme() const;
    void loadDefaults();
    void loadCurrentTheme();

    void saveTheme();
    void newTheme();
    void renameTheme(QListWidgetItem* item);
    void loadTheme();
    void deleteTheme();

    ThemeRegistry* m_themeRegistry;
    SettingsManager* m_settings;

    QListWidget* m_themesList;
    QPushButton* m_newButton;
    QPushButton* m_saveButton;
    QPushButton* m_deleteButton;

    QString m_defaultTheme;
    std::map<PaletteKey, std::pair<ColourButton*, QCheckBox*>> m_colourMapping;
    std::map<QString, std::pair<FontButton*, QCheckBox*>> m_fontMapping;
    std::optional<FyTheme> m_loadedTheme;
};

GuiColoursPageWidget::GuiColoursPageWidget(ThemeRegistry* themeRegistry, SettingsManager* settings)
    : m_themeRegistry{themeRegistry}
    , m_settings{settings}
    , m_themesList{new QListWidget(this)}
    , m_newButton{new QPushButton(tr("&New"), this)}
    , m_saveButton{new QPushButton(tr("&Save"), this)}
    , m_deleteButton{new QPushButton(tr("&Delete"), this)}
    , m_defaultTheme{tr("System default")}
{
    QObject::connect(m_themesList, &QListWidget::currentItemChanged, this, &GuiColoursPageWidget::loadTheme);
    QObject::connect(m_themesList, &QListWidget::itemChanged, this, &GuiColoursPageWidget::renameTheme);
    QObject::connect(m_newButton, &QPushButton::clicked, this, &GuiColoursPageWidget::newTheme);
    QObject::connect(m_saveButton, &QPushButton::clicked, this, &GuiColoursPageWidget::saveTheme);
    QObject::connect(m_deleteButton, &QPushButton::clicked, this, &GuiColoursPageWidget::deleteTheme);

    auto* themesLayout = new QGridLayout();

    int row{0};
    themesLayout->addWidget(m_newButton, row, 0);
    themesLayout->addWidget(m_saveButton, row, 1);
    themesLayout->addWidget(m_deleteButton, row++, 2);
    themesLayout->addWidget(m_themesList, row++, 0, 1, 3);

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

    layout->addLayout(themesLayout, 0, 0);
    layout->addWidget(themeDetails, 0, 1);
    layout->setColumnStretch(1, 1);
}

void GuiColoursPageWidget::load()
{
    const QSignalBlocker blocker{m_themesList};
    m_themesList->clear();

    auto* systemDefault = new QListWidgetItem(m_defaultTheme, m_themesList);
    systemDefault->setData(ThemeIdRole, -1);

    const auto current = m_settings->value<Settings::Gui::CustomTheme>().value<FyTheme>();
    const auto themes  = m_themeRegistry->items();
    int currentThemeRow{0};
    for(const auto& theme : themes) {
        auto* item = new QListWidgetItem(theme.name, m_themesList);
        item->setData(ThemeIdRole, theme.id);
        if(!theme.isDefault) {
            item->setFlags(item->flags() | Qt::ItemIsEditable);
        }
        if(theme.id == current.id && theme.name == current.name) {
            currentThemeRow = m_themesList->row(item);
        }
        else if(currentThemeRow == 0 && theme.name == current.name) {
            currentThemeRow = m_themesList->row(item);
        }
    }
    m_themesList->setCurrentRow(currentThemeRow);

    loadDefaults();
    loadCurrentTheme();
    updateButtonState();
}

void GuiColoursPageWidget::apply()
{
    const FyTheme theme = currentTheme();
    if(theme.isValid()) {
        m_settings->set<Settings::Gui::CustomTheme>(QVariant::fromValue(theme));
    }
    else {
        m_settings->reset<Settings::Gui::CustomTheme>();
    }
    loadDefaults();
    loadCurrentTheme();
}

void GuiColoursPageWidget::reset()
{
    m_settings->reset<Settings::Gui::CustomTheme>();
    m_loadedTheme.reset();
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
    const auto theme      = selectedTheme();
    const bool isEditable = theme && !theme->isDefault;

    m_saveButton->setEnabled(isEditable);
    m_deleteButton->setEnabled(isEditable);
}

std::optional<FyTheme> GuiColoursPageWidget::selectedTheme() const
{
    const auto* item = m_themesList->currentItem();
    if(!item) {
        return {};
    }

    const int themeId = item->data(ThemeIdRole).toInt();
    if(themeId < 0) {
        return {};
    }

    return m_themeRegistry->itemById(themeId);
}

FyTheme GuiColoursPageWidget::currentTheme() const
{
    FyTheme theme;
    if(m_loadedTheme) {
        theme = *m_loadedTheme;
        theme.colours.clear();
        theme.fonts.clear();
    }

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
    const auto systemPalette  = m_settings->value<Settings::Gui::Internal::SystemPalette>().value<QPalette>();
    const auto currentColours = Gui::coloursFromPalette(systemPalette);
    for(const auto& [key, colour] : Utils::asRange(currentColours)) {
        if(m_colourMapping.contains(key)) {
            const auto& [button, option] = m_colourMapping.at(key);
            button->setColour(colour);
            option->setChecked(false);
        }
    }

    const auto systemFont = m_settings->value<Settings::Gui::Internal::SystemFont>().value<QFont>();
    for(const auto& [className, font] : m_fontMapping) {
        const auto& [button, option] = font;
        button->setButtonFont(systemFont);
        option->setChecked(false);
    }
}

void GuiColoursPageWidget::loadCurrentTheme()
{
    const auto currentTheme = m_settings->value<Settings::Gui::CustomTheme>().value<FyTheme>();
    m_loadedTheme           = currentTheme.isValid() ? std::make_optional(currentTheme) : std::nullopt;
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
    const auto existingTheme = selectedTheme();
    if(!existingTheme || existingTheme->isDefault) {
        return;
    }

    FyTheme theme   = currentTheme();
    theme.id        = existingTheme->id;
    theme.index     = existingTheme->index;
    theme.name      = existingTheme->name;
    theme.isDefault = existingTheme->isDefault;

    if(m_themeRegistry->changeItem(theme)) {
        m_loadedTheme = m_themeRegistry->itemById(theme.id).value_or(theme);
    }
}

void GuiColoursPageWidget::newTheme()
{
    FyTheme theme   = currentTheme();
    theme.name      = tr("New Theme");
    theme.id        = 0;
    theme.index     = 0;
    theme.isDefault = false;
    theme           = m_themeRegistry->addItem(theme);
    m_loadedTheme   = theme;

    auto* item = new QListWidgetItem(theme.name, m_themesList);
    item->setData(ThemeIdRole, theme.id);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    m_themesList->setCurrentItem(item);
    m_themesList->editItem(item);

    updateButtonState();
}

void GuiColoursPageWidget::renameTheme(QListWidgetItem* item)
{
    if(!item) {
        return;
    }

    const int themeId = item->data(ThemeIdRole).toInt();
    if(themeId < 0) {
        return;
    }

    const auto theme = m_themeRegistry->itemById(themeId);
    if(!theme || theme->isDefault) {
        return;
    }

    const QString name = item->text().trimmed();
    if(name.isEmpty() || name == theme->name) {
        const QSignalBlocker blocker{m_themesList};
        item->setText(theme->name);
        return;
    }

    FyTheme renamedTheme{*theme};
    renamedTheme.name = Utils::findUniqueString(name, m_themeRegistry->items(), [themeId](const FyTheme& candidate) {
        return candidate.id == themeId ? QString{} : candidate.name;
    });
    if(m_themeRegistry->changeItem(renamedTheme)) {
        const auto changedTheme = m_themeRegistry->itemById(themeId).value_or(renamedTheme);
        const QSignalBlocker blocker{m_themesList};
        item->setText(changedTheme.name);
        if(m_loadedTheme && m_loadedTheme->id == themeId) {
            m_loadedTheme->name = changedTheme.name;
        }
    }
}

void GuiColoursPageWidget::loadTheme()
{
    const auto theme = selectedTheme();
    loadDefaults();
    m_loadedTheme = theme;

    for(const auto& [key, pair] : m_colourMapping) {
        if(theme && theme->colours.contains(key)) {
            const auto& [button, option] = pair;
            button->setColour(theme->colours.value(key));
            option->setChecked(true);
        }
    }
    for(const auto& [key, pair] : m_fontMapping) {
        if(theme && theme->fonts.contains(key)) {
            const auto& [button, option] = pair;
            button->setButtonFont(theme->fonts.value(key));
            option->setChecked(true);
        }
    }

    updateButtonState();
}

void GuiColoursPageWidget::deleteTheme()
{
    auto* item = m_themesList->currentItem();
    if(!item) {
        return;
    }

    const auto theme = selectedTheme();
    if(theme && !theme->isDefault) {
        const int row = m_themesList->row(item);
        m_themeRegistry->removeById(theme->id);
        delete m_themesList->takeItem(row);
        m_themesList->setCurrentRow(std::max(0, row - 1));
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
