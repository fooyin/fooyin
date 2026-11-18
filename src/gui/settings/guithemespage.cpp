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

#include "theme/editor/themeappearancedialog.h"
#include "theme/themeappearance.h"
#include "theme/themepreset.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <gui/internalguisettings.h>
#include <gui/theme/fytheme.h>
#include <gui/theme/fythemefile.h>
#include <gui/theme/themeregistry.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/fontbutton.h>
#include <gui/widgets/hintlabel.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTableWidget>

#include <array>
#include <optional>
#include <ranges>
#include <set>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
template <typename T>
T mergeThemeValue(const T& base, const T& external, const T& draft)
{
    return draft == base ? external : draft;
}

template <typename Map>
Map mergeThemeMap(const Map& base, const Map& external, const Map& draft)
{
    using Key   = Map::key_type;
    using Value = Map::mapped_type;

    std::set<Key> keys;
    for(auto it = base.cbegin(); it != base.cend(); ++it) {
        keys.emplace(it.key());
    }
    for(auto it = external.cbegin(); it != external.cend(); ++it) {
        keys.emplace(it.key());
    }
    for(auto it = draft.cbegin(); it != draft.cend(); ++it) {
        keys.emplace(it.key());
    }

    const auto value = [](const Map& map, const Key& key) -> std::optional<Value> {
        const auto it = map.constFind(key);
        return it == map.cend() ? std::nullopt : std::make_optional(it.value());
    };

    Map merged;
    for(const auto& key : keys) {
        const auto mergedValue = mergeThemeValue(value(base, key), value(external, key), value(draft, key));
        if(mergedValue) {
            merged.insert(key, *mergedValue);
        }
    }
    return merged;
}

FyTheme mergeTheme(const FyTheme& base, const FyTheme& external, const FyTheme& draft)
{
    return {
        .id         = mergeThemeValue(base.id, external.id, draft.id),
        .name       = mergeThemeValue(base.name, external.name, draft.name),
        .index      = mergeThemeValue(base.index, external.index, draft.index),
        .isDefault  = mergeThemeValue(base.isDefault, external.isDefault, draft.isDefault),
        .colours    = mergeThemeMap(base.colours, external.colours, draft.colours),
        .fonts      = mergeThemeMap(base.fonts, external.fonts, draft.fonts),
        .appearance = mergeThemeValue(base.appearance, external.appearance, draft.appearance),
    };
}
} // namespace

class ThemePresetSelector : public QWidget
{
    Q_OBJECT

public:
    ThemePresetSelector(ThemePart part, ThemeRegistry* themes, SettingsManager* settings, QWidget* parent);

    void setCurrentTheme(const FyTheme& theme, bool resetSelection = false);

Q_SIGNALS:
    void presetSelected(const Fooyin::FyTheme& preset);

private:
    int selectedId() const;
    int presetRow(int id) const;

    void addPreset(const QString& name, int id);
    std::optional<FyTheme> selectedPreset() const;
    void updateButtons();

    void loadPreset();
    void savePreset();
    void newPreset();
    void renamePreset();
    void deletePreset();
    void importPreset();
    void exportPreset();

    ThemePart m_part;
    ThemePresetRegistry m_registry;
    QTableWidget* m_presets;
    QPushButton* m_save;
    QPushButton* m_saveAs;
    QPushButton* m_more;
    QAction* m_rename;
    QAction* m_delete;
    QAction* m_export;
    FyTheme m_current;
};

ThemePresetSelector::ThemePresetSelector(ThemePart part, ThemeRegistry* themes, SettingsManager* settings,
                                         QWidget* parent)
    : QWidget{parent}
    , m_part{part}
    , m_registry{part, themes, settings}
    , m_presets{new QTableWidget(this)}
    , m_save{new QPushButton(tr("Save"), this)}
    , m_saveAs{new QPushButton(tr("Save as…"), this)}
    , m_more{new QPushButton(this)}
{
    m_presets->setColumnCount(1);
    m_presets->setHorizontalHeaderLabels({tr("Preset")});
    m_presets->horizontalHeader()->setStretchLastSection(true);
    m_presets->verticalHeader()->hide();
    m_presets->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_presets->setSelectionMode(QAbstractItemView::SingleSelection);
    m_presets->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_presets->setShowGrid(false);

    auto* menu = new QMenu(m_more);
    m_rename   = menu->addAction(tr("Rename…"), this, &ThemePresetSelector::renamePreset);
    m_delete   = menu->addAction(tr("Delete"), this, &ThemePresetSelector::deletePreset);
    menu->addSeparator();
    menu->addAction(tr("Import…"), this, &ThemePresetSelector::importPreset);
    m_export = menu->addAction(tr("Export…"), this, &ThemePresetSelector::exportPreset);

    m_more->setText(tr("More"));
    m_more->setMenu(menu);

    auto* buttons = new QHBoxLayout();
    buttons->addWidget(m_save);
    buttons->addWidget(m_saveAs);
    buttons->addWidget(m_more);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_presets, 1);
    layout->addLayout(buttons);

    QObject::connect(m_presets, &QTableWidget::currentCellChanged, this, &ThemePresetSelector::loadPreset);
    QObject::connect(m_save, &QPushButton::clicked, this, &ThemePresetSelector::savePreset);
    QObject::connect(m_saveAs, &QPushButton::clicked, this, &ThemePresetSelector::newPreset);

    setCurrentTheme({}, true);
}

void ThemePresetSelector::setCurrentTheme(const FyTheme& theme, bool resetSelection)
{
    const int preferred = selectedId();
    m_current           = extractThemePart(theme, m_part);

    const QSignalBlocker blocker{m_presets};

    m_presets->setRowCount(0);

    addPreset(tr("Custom"), -2);
    addPreset(tr("System default"), -1);

    int selected = themePartMatches(m_current, {}, m_part) ? 1 : 0;
    for(const auto& preset : m_registry.items()) {
        addPreset(preset.name, preset.id);
        if((!resetSelection && preset.id == preferred)
           || (selected == 0 && themePartMatches(m_current, preset, m_part))) {
            selected = m_presets->rowCount() - 1;
        }
    }

    m_presets->setCurrentCell(selected, 0);
    m_presets->scrollToItem(m_presets->item(selected, 0));

    if(const auto preset = selectedPreset(); preset && !themePartMatches(m_current, *preset, m_part)) {
        m_presets->item(selected, 0)->setText(tr("%1 (modified)").arg(preset->name));
    }

    updateButtons();
}

int ThemePresetSelector::selectedId() const
{
    const auto* item = m_presets->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : -2;
}

int ThemePresetSelector::presetRow(int id) const
{
    for(int row{0}; row < m_presets->rowCount(); ++row) {
        if(m_presets->item(row, 0)->data(Qt::UserRole).toInt() == id) {
            return row;
        }
    }
    return -1;
}

void ThemePresetSelector::addPreset(const QString& name, int id)
{
    const int row = m_presets->rowCount();
    m_presets->insertRow(row);
    auto* item = new QTableWidgetItem(name);
    item->setData(Qt::UserRole, id);
    m_presets->setItem(row, 0, item);
}

std::optional<FyTheme> ThemePresetSelector::selectedPreset() const
{
    return m_registry.itemById(selectedId());
}

void ThemePresetSelector::updateButtons()
{
    const auto selected = selectedPreset();
    const bool editable = selected && !selected->isDefault;
    const bool valid    = m_part != ThemePart::Appearance || resolveThemeAppearance(m_current.appearance).has_value();

    m_save->setEnabled(editable && valid && !themePartMatches(m_current, *selected, m_part));
    m_saveAs->setEnabled(valid);
    m_rename->setEnabled(editable);
    m_delete->setEnabled(editable);
    m_export->setEnabled(valid && m_current.isValid());
}

void ThemePresetSelector::loadPreset()
{
    const int id = selectedId();
    if(id == -2) {
        updateButtons();
        return;
    }

    m_current = id < 0 ? FyTheme{} : selectedPreset().value_or(FyTheme{});
    Q_EMIT presetSelected(m_current);
    updateButtons();
}

void ThemePresetSelector::savePreset()
{
    const auto selected = selectedPreset();
    if(!selected || selected->isDefault) {
        return;
    }

    auto preset{m_current};
    preset.id        = selected->id;
    preset.index     = selected->index;
    preset.name      = selected->name;
    preset.isDefault = false;
    m_registry.changeItem(preset);
    setCurrentTheme(m_current);
}

void ThemePresetSelector::newPreset()
{
    bool accepted{false};
    const auto selected = selectedPreset();
    const auto name     = QInputDialog::getText(this, tr("Save Preset"), tr("Preset name"), QLineEdit::Normal,
                                                selected ? selected->name : QString{}, &accepted)
                              .trimmed();
    if(!accepted || name.isEmpty()) {
        return;
    }

    auto preset{m_current};
    preset.name      = name;
    preset.isDefault = false;
    preset           = m_registry.addItem(preset);
    setCurrentTheme(preset);

    {
        const QSignalBlocker blocker{m_presets};
        m_presets->setCurrentCell(presetRow(preset.id), 0);
        m_presets->scrollToItem(m_presets->currentItem());
    }

    updateButtons();
}

void ThemePresetSelector::renamePreset()
{
    auto selected = selectedPreset();
    if(!selected || selected->isDefault) {
        return;
    }

    bool accepted{false};
    const auto name = QInputDialog::getText(this, tr("Rename Preset"), tr("Preset name"), QLineEdit::Normal,
                                            selected->name, &accepted)
                          .trimmed();
    if(accepted && !name.isEmpty()) {
        selected->name = name;
        m_registry.changeItem(*selected);
        setCurrentTheme(m_current);
    }
}

void ThemePresetSelector::deletePreset()
{
    if(const auto selected = selectedPreset(); selected && !selected->isDefault) {
        m_registry.removeById(selected->id);
        setCurrentTheme(m_current);
    }
}

void ThemePresetSelector::importPreset()
{
    const auto path = QFileDialog::getOpenFileName(this, tr("Import Preset"), {}, tr("fooyin Theme (*.fyt)"), nullptr,
                                                   QFileDialog::DontResolveSymlinks);
    if(path.isEmpty()) {
        return;
    }

    const auto imported = FyThemeFile::read(path);
    if(!imported) {
        QMessageBox::warning(this, tr("Import Preset"), tr("Could not import preset: %1").arg(imported.error()));
        return;
    }

    const auto part = extractThemePart(*imported, m_part);
    if(!part.isValid()) {
        QMessageBox::warning(this, tr("Import Preset"), tr("This file has no settings for this preset type."));
        return;
    }

    const auto preset = m_registry.addItem(part);
    setCurrentTheme(preset);

    {
        const QSignalBlocker blocker{m_presets};
        m_presets->setCurrentCell(presetRow(preset.id), 0);
        m_presets->scrollToItem(m_presets->currentItem());
    }

    updateButtons();
    Q_EMIT presetSelected(preset);
}

void ThemePresetSelector::exportPreset()
{
    const auto selected = selectedPreset();

    auto preset{m_current};
    preset.name = selected ? selected->name : tr("Custom preset");
    auto path   = QFileDialog::getSaveFileName(this, tr("Export Preset"), preset.name + u".fyt"_s,
                                               tr("fooyin Theme (*.fyt)"), nullptr, QFileDialog::DontResolveSymlinks);
    if(path.isEmpty()) {
        return;
    }

    if(QFileInfo{path}.suffix().compare(u"fyt"_s, Qt::CaseInsensitive) != 0) {
        path += u".fyt"_s;
    }
    if(const auto result = FyThemeFile::write(preset, path); !result) {
        QMessageBox::warning(this, tr("Export Preset"), tr("Could not export preset: %1").arg(result.error()));
    }
}

class GuiColoursPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit GuiColoursPageWidget(ThemeRegistry* themeRegistry, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

    [[nodiscard]] QString validationError() const override;

protected:
    void changeEvent(QEvent* event) override;

private:
    void addColourOption(QGridLayout* layout, const QString& title, QPalette::ColorRole role,
                         QPalette::ColorGroup group = QPalette::All);
    void addFontOption(QGridLayout* layout, const QString& title, const QString& className);

    void updateButtonState() const;
    void updateDraftState(bool resetPresetSelection = false);
    void updateColourOptionState() const;
    void updateFontOptions() const;
    void editAppearance();
    [[nodiscard]] std::optional<FyTheme> selectedTheme() const;
    [[nodiscard]] FyTheme currentTheme() const;
    void loadDefaults();
    void displayTheme(const FyTheme& theme, bool resetPresetSelection = false);
    void mergeExternalTheme();

    void saveTheme();
    void newTheme();
    void importTheme();
    void exportTheme();
    void renameTheme();
    void loadTheme();
    void deleteTheme();

    ThemeRegistry* m_themeRegistry;
    SettingsManager* m_settings;

    QComboBox* m_styles;
#ifdef Q_OS_WIN
    QCheckBox* m_darkMode;
#endif
    QComboBox* m_themes;
    QPushButton* m_newButton;
    QPushButton* m_saveButton;
    QPushButton* m_themeActions;
    QAction* m_renameAction;
    QAction* m_deleteAction;
    QAction* m_exportAction;
    QLabel* m_modifiedHint;
    QTabWidget* m_themeDetails;
    QPushButton* m_editAppearance;
    std::array<ThemePresetSelector*, 3> m_presetSelectors;
    QJsonObject m_appearanceDraft;
    QLabel* m_colourHint;
    QLabel* m_appearanceStyleHint;
    QLabel* m_basicStyleHint;
    QLabel* m_advancedStyleHint;

    QString m_defaultTheme;
    std::map<PaletteKey, ColourButton*> m_colourMapping;
    std::map<QString, FontButton*> m_fontMapping;
    std::optional<FyTheme> m_loadedTheme;
    FyTheme m_themeBaseline;
    bool m_isLoaded{false};
    bool m_updatingSetting{false};
};

GuiColoursPageWidget::GuiColoursPageWidget(ThemeRegistry* themeRegistry, SettingsManager* settings)
    : m_themeRegistry{themeRegistry}
    , m_settings{settings}
    , m_styles{new QComboBox(this)}
#ifdef Q_OS_WIN
    , m_darkMode{new QCheckBox(tr("Dark mode"), this)}
#endif
    , m_themes{new QComboBox(this)}
    , m_newButton{new QPushButton(tr("Save as…"), this)}
    , m_saveButton{new QPushButton(tr("&Save"), this)}
    , m_themeActions{new QPushButton(this)}
    , m_modifiedHint{new QLabel(tr("Modified"), this)}
    , m_themeDetails{new QTabWidget(this)}
    , m_editAppearance{new QPushButton(tr("Edit appearance…"), this)}
    , m_presetSelectors{new ThemePresetSelector(ThemePart::Colours, themeRegistry, settings, this),
                        new ThemePresetSelector(ThemePart::Fonts, themeRegistry, settings, this),
                        new ThemePresetSelector(ThemePart::Appearance, themeRegistry, settings, this)}
    , m_colourHint{new HintLabel(tr("Theme colours are disabled when using this style. "
                                    "Select Fooyin or Fusion above to customise colours."),
                                 this)}
    , m_appearanceStyleHint{new HintLabel(tr("Appearance settings require the Fooyin style. "
                                             "Select Fooyin above to apply all parts of a theme."),
                                          this)}
    , m_basicStyleHint{new HintLabel(
          tr("Some styles may not fully support custom colours. Select Fooyin or Fusion above "
             "for consistent colour support."),
          this)}
    , m_advancedStyleHint{new HintLabel(m_basicStyleHint->text(), this)}
    , m_defaultTheme{tr("System default")}
{
    m_styles->addItem(tr("System default"));
    m_styles->addItem(u"Fooyin"_s);
    m_styles->addItems(QStyleFactory::keys());

    m_appearanceStyleHint->setWordWrap(true);

    m_themes->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_themes->setMinimumContentsLength(16);

    QObject::connect(m_themes, &QComboBox::currentIndexChanged, this, &GuiColoursPageWidget::loadTheme);
    QObject::connect(m_newButton, &QPushButton::clicked, this, &GuiColoursPageWidget::newTheme);
    QObject::connect(m_saveButton, &QPushButton::clicked, this, &GuiColoursPageWidget::saveTheme);

    auto* themeMenu = new QMenu(m_themeActions);
    m_renameAction  = themeMenu->addAction(tr("Rename…"), this, &GuiColoursPageWidget::renameTheme);
    m_deleteAction  = themeMenu->addAction(tr("Delete"), this, &GuiColoursPageWidget::deleteTheme);
    themeMenu->addSeparator();
    themeMenu->addAction(tr("Import…"), this, &GuiColoursPageWidget::importTheme);
    m_exportAction = themeMenu->addAction(tr("Export…"), this, &GuiColoursPageWidget::exportTheme);

    m_themeActions->setText(tr("More"));
    m_themeActions->setMenu(themeMenu);

    m_settings->subscribe<Settings::Gui::CustomTheme>(this, &GuiColoursPageWidget::mergeExternalTheme);
    m_settings->subscribe<Settings::Gui::Style>(this, [this](const QString& style) {
        const int index = m_styles->findText(style);
        m_styles->setCurrentIndex(style.isEmpty() || index < 0 ? 0 : index);
        updateColourOptionState();
    });
#ifdef Q_OS_WIN
    m_settings->subscribe<Settings::Gui::DarkMode>(m_darkMode, &QCheckBox::setChecked);
#endif

    QObject::connect(m_editAppearance, &QPushButton::clicked, this, &GuiColoursPageWidget::editAppearance);

    static constexpr std::array parts{ThemePart::Colours, ThemePart::Fonts, ThemePart::Appearance};
    for(size_t index{0}; index < parts.size(); ++index) {
        QObject::connect(m_presetSelectors[index], &ThemePresetSelector::presetSelected, this,
                         [this, part = parts[index]](const FyTheme& preset) {
                             auto theme = currentTheme();
                             applyThemePart(theme, preset, part);
                             displayTheme(theme);
                         });
    }
    auto* themesLayout = new QHBoxLayout();
    themesLayout->addWidget(m_themes, 1);
    themesLayout->addWidget(m_modifiedHint);
    themesLayout->addWidget(m_saveButton);
    themesLayout->addWidget(m_newButton);
    themesLayout->addWidget(m_themeActions);

    auto* selectorsLayout = new QGridLayout();
    selectorsLayout->addWidget(new QLabel(tr("Widget style") + u":"_s, this), 0, 0);
    selectorsLayout->addWidget(m_styles, 0, 1);
#ifdef Q_OS_WIN
    selectorsLayout->addWidget(m_darkMode, 0, 2);
#endif
    selectorsLayout->addWidget(new QLabel(tr("Theme") + u":"_s, this), 1, 0);
    selectorsLayout->addLayout(themesLayout, 1, 1, 1, 2);
    selectorsLayout->setColumnStretch(1, 1);

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

    const int baseStretchRow = baseLayout->rowCount();
    baseLayout->setRowStretch(baseStretchRow, 1);
    baseLayout->addWidget(m_basicStyleHint, baseStretchRow + 1, 0, 1, baseLayout->columnCount());

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

    ColourButton::alignLabels(m_colourMapping | std::views::values);

    const int advancedStretchRow = advancedLayout->rowCount();
    advancedLayout->setRowStretch(advancedStretchRow, 1);
    advancedLayout->addWidget(m_advancedStyleHint, advancedStretchRow + 1, 0, 1, advancedLayout->columnCount());

    auto* fonts       = new QWidget(this);
    auto* fontsLayout = new QGridLayout(fonts);

    const auto fontEntries = m_themeRegistry->fontEntries();

    std::map<QString, QString> sortedEntries;
    for(const auto& [className, entry] : fontEntries) {
        if(className.isEmpty()) {
            addFontOption(fontsLayout, entry.title, className);
        }
        else {
            sortedEntries.emplace(entry.title, className);
        }
    }
    for(const auto& [title, className] : sortedEntries) {
        addFontOption(fontsLayout, title, className);
    }
    FontButton::alignLabels(m_fontMapping | std::views::values);

    fontsLayout->setColumnStretch(1, 1);
    fontsLayout->setRowStretch(fontsLayout->rowCount(), 1);

    auto* colourTabs = new QTabWidget(this);
    colourTabs->addTab(baseColours, tr("Basic colours"));
    colourTabs->addTab(advancedColours, tr("Advanced colours"));

    auto* appearance       = new QWidget(this);
    auto* appearanceLayout = new QVBoxLayout(appearance);
    appearanceLayout->addWidget(m_editAppearance, 0, Qt::AlignLeft);
    appearanceLayout->addWidget(
        new HintLabel(tr("Appearance controls shapes, spacing, borders and animation when using the Fooyin style. "
                         "Palette roles follow the colour preset, and component typography inherits "
                         "the font preset unless explicitly overridden."),
                      appearance));
    appearanceLayout->addStretch();

    const auto addSection = [this](ThemePresetSelector* presets, QWidget* editor, const QString& title) {
        auto* section = new QSplitter(Qt::Horizontal, m_themeDetails);
        section->setChildrenCollapsible(false);
        section->addWidget(presets);
        section->addWidget(editor);
        section->setStretchFactor(0, 0);
        section->setStretchFactor(1, 1);
        section->setSizes({220, 500});

        m_themeDetails->addTab(section, title);
    };

    addSection(m_presetSelectors[0], colourTabs, tr("Colours"));
    addSection(m_presetSelectors[1], fonts, tr("Fonts"));
    addSection(m_presetSelectors[2], appearance, tr("Appearance"));

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(selectorsLayout);
    layout->addWidget(m_themeDetails, 1);
    layout->addWidget(m_colourHint);
    layout->addWidget(m_appearanceStyleHint);

    m_colourHint->setWordWrap(true);
    QObject::connect(m_styles, &QComboBox::currentIndexChanged, this, &GuiColoursPageWidget::updateColourOptionState);
    updateColourOptionState();
}

void GuiColoursPageWidget::load()
{
    const auto style     = m_settings->value<Settings::Gui::Style>();
    const int styleIndex = m_styles->findText(style);
    m_styles->setCurrentIndex(style.isEmpty() || styleIndex < 0 ? 0 : styleIndex);
#ifdef Q_OS_WIN
    m_darkMode->setChecked(m_settings->value<Settings::Gui::DarkMode>());
#endif

    {
        const QSignalBlocker blocker{m_themes};
        m_themes->clear();

        m_themes->addItem(tr("Custom"), -2);
        m_themes->addItem(m_defaultTheme, -1);

        const auto themes = m_themeRegistry->items();
        for(const auto& theme : themes) {
            if(!theme.isDefault || !theme.fonts.isEmpty() || !theme.appearance.isEmpty()) {
                m_themes->addItem(theme.name, theme.id);
            }
        }
    }

    m_themeBaseline = m_settings->value<Settings::Gui::CustomTheme>().value<FyTheme>();
    m_isLoaded      = true;
    displayTheme(m_themeBaseline, true);
    updateColourOptionState();
}

void GuiColoursPageWidget::apply()
{
    m_settings->set<Settings::Gui::Style>(m_styles->currentIndex() == 0 ? QString{} : m_styles->currentText());
#ifdef Q_OS_WIN
    m_settings->set<Settings::Gui::DarkMode>(m_darkMode->isChecked());
#endif

    const FyTheme theme = currentTheme();
    m_updatingSetting   = true;
    if(theme.isValid()) {
        m_settings->set<Settings::Gui::CustomTheme>(QVariant::fromValue(theme));
    }
    else {
        m_settings->reset<Settings::Gui::CustomTheme>();
    }
    m_updatingSetting = false;

    m_themeBaseline = m_settings->value<Settings::Gui::CustomTheme>().value<FyTheme>();
    displayTheme(m_themeBaseline);
}

void GuiColoursPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Style>();
#ifdef Q_OS_WIN
    m_settings->reset<Settings::Gui::DarkMode>();
#endif

    m_updatingSetting = true;
    m_settings->reset<Settings::Gui::CustomTheme>();
    m_updatingSetting = false;

    m_themeBaseline = m_settings->value<Settings::Gui::CustomTheme>().value<FyTheme>();
    displayTheme(m_themeBaseline, true);
}

QString GuiColoursPageWidget::validationError() const
{
    const auto resolved = resolveThemeAppearance(m_appearanceDraft);
    return resolved ? QString{} : tr("Theme appearance: %1").arg(resolved.error());
}

void GuiColoursPageWidget::changeEvent(QEvent* event)
{
    SettingsPageWidget::changeEvent(event);

    if(event->type() == QEvent::FontChange) {
        ColourButton::alignLabels(m_colourMapping | std::views::values);
        FontButton::alignLabels(m_fontMapping | std::views::values);
    }
}

void GuiColoursPageWidget::addColourOption(QGridLayout* layout, const QString& title, QPalette::ColorRole role,
                                           QPalette::ColorGroup group)
{
    auto* colour = new ColourButton(title, true, this);

    const int row = layout->rowCount();
    layout->addWidget(colour, row, 0);

    m_colourMapping[PaletteKey{.role = role, .group = group}] = colour;
    QObject::connect(colour, &ColourButton::colourUpdated, this, [this]() { updateDraftState(); });
    QObject::connect(colour, &ColourButton::toggled, this, [this]() { updateDraftState(); });
}

void GuiColoursPageWidget::addFontOption(QGridLayout* layout, const QString& title, const QString& className)
{
    auto* font = new FontButton(title, true, this);

    const int row = layout->rowCount();
    layout->addWidget(font, row, 0, 1, 2);

    m_fontMapping[className] = font;

    QObject::connect(font, &FontButton::toggled, this, [this] {
        updateFontOptions();
        updateDraftState();
    });
    QObject::connect(font, &FontButton::fontUpdated, this, [this, font]() {
        if(font->isChecked()) {
            updateFontOptions();
            updateDraftState();
        }
    });
}

void GuiColoursPageWidget::updateButtonState() const
{
    const auto selected{selectedTheme()};
    const bool editable = selected && !selected->isDefault;
    const bool valid    = validationError().isEmpty();

    const auto draft{currentTheme()};
    const auto base = selected.value_or(FyTheme{});
    const bool modified
        = draft.colours != base.colours || draft.fonts != base.fonts || draft.appearance != base.appearance;

    m_modifiedHint->setVisible(modified && m_themes->currentData().toInt() != -2);
    m_newButton->setEnabled(valid && draft.isValid());
    m_saveButton->setEnabled(editable && valid && modified);
    m_renameAction->setEnabled(editable);
    m_deleteAction->setEnabled(editable);
    m_exportAction->setEnabled(draft.isValid() && valid);
}

void GuiColoursPageWidget::updateDraftState(bool resetPresetSelection)
{
    if(!m_isLoaded) {
        return;
    }

    const auto draft{currentTheme()};
    for(auto* selector : m_presetSelectors) {
        selector->setCurrentTheme(draft, resetPresetSelection);
    }
    updateButtonState();
}

void GuiColoursPageWidget::updateColourOptionState() const
{
    const QString styleName = m_styles->currentIndex() == 0 ? m_settings->value<Settings::Gui::Internal::SystemStyle>()
                                                            : m_styles->currentText();

#ifdef Q_OS_WIN
    const bool supportsDarkMode = Gui::styleSupportsDarkMode(styleName);
    m_darkMode->setEnabled(supportsDarkMode);
    m_darkMode->setToolTip(supportsDarkMode ? QString{} : tr("Dark mode is not supported by this style."));
#endif

    const bool enabled    = Gui::styleSupportsCustomPalette(styleName);
    const QString tooltip = enabled ? QString{} : m_colourHint->text();
    m_themeDetails->setTabEnabled(0, enabled);
    m_themeDetails->setTabToolTip(0, tooltip);
    m_colourHint->setVisible(!enabled);

    const bool supportsAppearance = styleName.compare("Fooyin"_L1, Qt::CaseInsensitive) == 0;
    const auto appearanceTooltip = supportsAppearance ? QString{} : tr("Select Fooyin above to use appearance presets");
    m_themeDetails->setTabEnabled(2, supportsAppearance);
    m_themeDetails->setTabToolTip(2, appearanceTooltip);
    m_editAppearance->setEnabled(supportsAppearance);
    m_appearanceStyleHint->setVisible(!supportsAppearance);

    const bool fullColourSupport = styleName.compare("Fooyin"_L1, Qt::CaseInsensitive) == 0
                                || styleName.compare("Fusion"_L1, Qt::CaseInsensitive) == 0;
    const bool showStyleHint     = enabled && !fullColourSupport;
    m_basicStyleHint->setVisible(showStyleHint);
    m_advancedStyleHint->setVisible(showStyleHint);
}

void GuiColoursPageWidget::updateFontOptions() const
{
    const auto theme      = currentTheme();
    const auto systemFont = m_settings->value<Settings::Gui::Internal::SystemFont>().value<QFont>();

    for(const auto& [className, button] : m_fontMapping) {
        const QSignalBlocker blocker{button};
        button->setButtonFont(m_themeRegistry->resolveFont(className, theme, systemFont));
    }
}

void GuiColoursPageWidget::editAppearance()
{
    const auto theme = currentTheme();
    auto palette     = m_settings->value<Settings::Gui::Internal::SystemPalette>().value<QPalette>();
    for(const auto& [key, colour] : Utils::asRange(theme.colours)) {
        palette.setColor(key.group, key.role, colour);
    }

    const auto systemFont = m_settings->value<Settings::Gui::Internal::SystemFont>().value<QFont>();
    ThemeAppearanceDialog dialog{m_appearanceDraft, palette,
                                 m_themeRegistry->resolveFont(u"QSlider"_s, theme, systemFont), this};
    if(dialog.exec() == QDialog::Accepted) {
        m_appearanceDraft = dialog.appearance();
        updateDraftState();
    }
}

std::optional<FyTheme> GuiColoursPageWidget::selectedTheme() const
{
    return m_themeRegistry->itemById(m_themes->currentData().toInt());
}

FyTheme GuiColoursPageWidget::currentTheme() const
{
    FyTheme theme;
    if(m_loadedTheme) {
        theme = *m_loadedTheme;
        for(const auto& key : m_colourMapping | std::views::keys) {
            theme.colours.remove(key);
        }
        for(const auto& key : m_fontMapping | std::views::keys) {
            theme.fonts.remove(key);
        }
    }

    for(const auto& [key, button] : m_colourMapping) {
        if(button->isChecked()) {
            theme.colours[key] = button->colour();
        }
    }
    for(const auto& [key, button] : m_fontMapping) {
        if(button->isChecked()) {
            theme.fonts[key] = button->buttonFont();
        }
    }

    theme.appearance = m_appearanceDraft;

    return theme;
}

void GuiColoursPageWidget::loadDefaults()
{
    const auto systemPalette  = m_settings->value<Settings::Gui::Internal::SystemPalette>().value<QPalette>();
    const auto currentColours = Gui::coloursFromPalette(systemPalette);
    for(const auto& [key, colour] : Utils::asRange(currentColours)) {
        if(m_colourMapping.contains(key)) {
            auto* button = m_colourMapping.at(key);
            const QSignalBlocker blocker{button};
            button->setColour(colour);
            button->setChecked(false);
        }
    }

    const auto systemFont = m_settings->value<Settings::Gui::Internal::SystemFont>().value<QFont>();
    for(const auto& [className, button] : m_fontMapping) {
        const QSignalBlocker blocker{button};
        button->setButtonFont(m_themeRegistry->resolveFont(className, {}, systemFont));
        button->setChecked(false);
    }
}

void GuiColoursPageWidget::displayTheme(const FyTheme& theme, bool resetPresetSelection)
{
    {
        const QSignalBlocker blocker{m_themes};

        int selected = theme.isValid() ? 0 : 1;
        for(int index{2}; index < m_themes->count(); ++index) {
            const auto stored = m_themeRegistry->itemById(m_themes->itemData(index).toInt());
            if(stored && stored->id == theme.id && stored->name == theme.name) {
                selected = index;
                break;
            }
            if(selected == 0 && stored && stored->name == theme.name) {
                selected = index;
            }
        }
        m_themes->setCurrentIndex(selected);
    }

    loadDefaults();
    m_loadedTheme = theme.isValid() ? std::make_optional(theme) : std::nullopt;

    for(const auto& [key, colour] : Utils::asRange(theme.colours)) {
        if(m_colourMapping.contains(key)) {
            auto* button = m_colourMapping.at(key);
            const QSignalBlocker blocker{button};
            button->setColour(colour);
            button->setChecked(true);
        }
    }
    for(const auto& [key, font] : Utils::asRange(theme.fonts)) {
        if(m_fontMapping.contains(key)) {
            auto* button = m_fontMapping.at(key);
            const QSignalBlocker blocker{button};
            button->setButtonFont(font);
            button->setChecked(true);
        }
    }

    m_appearanceDraft = theme.appearance;
    updateFontOptions();
    updateDraftState(resetPresetSelection);
}

void GuiColoursPageWidget::mergeExternalTheme()
{
    if(!m_isLoaded || m_updatingSetting) {
        return;
    }

    const auto external  = m_settings->value<Settings::Gui::CustomTheme>().value<FyTheme>();
    const FyTheme merged = mergeTheme(m_themeBaseline, external, currentTheme());
    m_themeBaseline      = external;
    displayTheme(merged);
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
    updateDraftState();
}

void GuiColoursPageWidget::newTheme()
{
    bool accepted{false};
    const auto name
        = QInputDialog::getText(this, tr("Save Theme"), tr("Theme name"), QLineEdit::Normal, tr("New theme"), &accepted)
              .trimmed();
    if(!accepted || name.isEmpty()) {
        return;
    }

    FyTheme theme   = currentTheme();
    theme.name      = name;
    theme.id        = 0;
    theme.index     = 0;
    theme.isDefault = false;
    theme           = m_themeRegistry->addItem(theme);

    m_themes->addItem(theme.name, theme.id);
    m_themes->setCurrentIndex(m_themes->count() - 1);
}

void GuiColoursPageWidget::importTheme()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Theme"), {}, tr("fooyin Theme (*.fyt)"), nullptr,
                                                      QFileDialog::DontResolveSymlinks);
    if(path.isEmpty()) {
        return;
    }

    const auto imported = FyThemeFile::read(path);
    if(!imported) {
        QMessageBox::warning(this, tr("Import Theme"), tr("Could not import theme: %1").arg(imported.error()));
        return;
    }

    FyTheme theme{*imported};
    theme.id        = 0;
    theme.index     = 0;
    theme.isDefault = false;
    theme           = m_themeRegistry->addItem(theme);

    m_themes->addItem(theme.name, theme.id);
    m_themes->setCurrentIndex(m_themes->count() - 1);
}

void GuiColoursPageWidget::exportTheme()
{
    FyTheme theme{currentTheme()};
    if(theme.name.isEmpty()) {
        theme.name = tr("Custom theme");
    }

    QString path = QFileDialog::getSaveFileName(this, tr("Save Theme"), theme.name + u".fyt"_s,
                                                tr("fooyin Theme (*.fyt)"), nullptr, QFileDialog::DontResolveSymlinks);
    if(path.isEmpty()) {
        return;
    }
    if(QFileInfo{path}.suffix().compare(u"fyt"_s, Qt::CaseInsensitive) != 0) {
        path += u".fyt"_s;
    }

    const auto result = FyThemeFile::write(theme, path);
    if(!result) {
        QMessageBox::warning(this, tr("Export Theme"), tr("Could not export theme: %1").arg(result.error()));
    }
}

void GuiColoursPageWidget::renameTheme()
{
    auto theme{selectedTheme()};
    if(!theme || theme->isDefault) {
        return;
    }

    bool accepted{false};
    const auto name
        = QInputDialog::getText(this, tr("Rename Theme"), tr("Theme name"), QLineEdit::Normal, theme->name, &accepted)
              .trimmed();
    if(!accepted || name.isEmpty()) {
        return;
    }

    theme->name = name;
    if(m_themeRegistry->changeItem(*theme)) {
        const auto saved = m_themeRegistry->itemById(theme->id).value_or(*theme);
        m_themes->setItemText(m_themes->currentIndex(), saved.name);
        if(m_loadedTheme) {
            m_loadedTheme->name = saved.name;
        }
    }
}

void GuiColoursPageWidget::loadTheme()
{
    if(m_themes->currentData().toInt() == -2) {
        auto draft{currentTheme()};
        draft.id = 0;
        draft.name.clear();
        draft.isDefault = false;
        displayTheme(draft);
    }
    else {
        displayTheme(selectedTheme().value_or(FyTheme{}), true);
    }
}

void GuiColoursPageWidget::deleteTheme()
{
    const auto selected = selectedTheme();
    if(!selected || selected->isDefault) {
        return;
    }

    auto draft{currentTheme()};
    draft.id = 0;
    draft.name.clear();
    draft.isDefault = false;
    const int index = m_themes->currentIndex();
    m_themeRegistry->removeById(selected->id);
    {
        const QSignalBlocker blocker{m_themes};
        m_themes->removeItem(index);
    }
    displayTheme(draft);
}

GuiThemesPage::GuiThemesPage(ThemeRegistry* colourRegistry, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::InterfaceTheme);
    setName(tr("Theme"));
    setCategory({tr("Interface"), tr("Theme")});
    setWidgetCreator([colourRegistry, settings] { return new GuiColoursPageWidget(colourRegistry, settings); });
}
} // namespace Fooyin

#include "guithemespage.moc"
#include "moc_guithemespage.cpp"
