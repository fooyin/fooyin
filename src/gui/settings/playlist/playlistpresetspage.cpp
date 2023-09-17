/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlistpresetspage.h"

#include "gui/guiconstants.h"
#include "gui/playlist/playlistpreset.h"
#include "gui/playlist/presetregistry.h"
#include "presetinputbox.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>

#include <deque>

namespace Fy::Gui::Settings {
using PresetRegistry = Widgets::Playlist::PresetRegistry;
using PlaylistPreset = Widgets::Playlist::PlaylistPreset;
using TextBlock      = Widgets::Playlist::TextBlock;
using TextBlockList  = Widgets::Playlist::TextBlockList;

void setupInputBox(const TextBlock& preset, PresetInput* block)
{
    block->setText(preset.text);
    block->setFont(preset.font);
    block->setColour(preset.colour);

    PresetInput::State state;
    if(preset.colourChanged) {
        state |= PresetInput::ColourChanged;
    }
    if(preset.fontChanged) {
        state |= PresetInput::FontChanged;
    }
    block->setState(state);
}

void updateTextBlock(PresetInput* presetInput, TextBlock& textBlock)
{
    textBlock.text   = presetInput->text();
    textBlock.font   = presetInput->font();
    textBlock.colour = presetInput->colour();

    auto state = presetInput->state();

    textBlock.fontChanged   = state & PresetInput::FontChanged;
    textBlock.colourChanged = state & PresetInput::ColourChanged;
}

void updateTextBlocks(const PresetInputList& presetInputs, TextBlockList& textBlocks)
{
    textBlocks.clear();

    for(const auto& input : presetInputs) {
        if(!input->text().isEmpty()) {
            TextBlock block;
            updateTextBlock(input, block);
            textBlocks.emplace_back(block);
        }
    }
}

void createPresetInput(const TextBlock& block, PresetInputBox* box, QWidget* parent)
{
    auto* input = new PresetInput(parent);
    setupInputBox(block, input);
    box->addInput(input);
}

void createPresetInputs(const TextBlockList& blocks, PresetInputBox* box, QWidget* parent)
{
    if(blocks.empty()) {
        createPresetInput({}, box, parent);
    }
    else {
        for(const auto& block : blocks) {
            createPresetInput(block, box, parent);
        }
    }
}

class PlaylistPresetsPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit PlaylistPresetsPageWidget(Widgets::Playlist::PresetRegistry* presetRegistry);

    void apply() override;

    void newPreset();
    void deletePreset();
    void updatePreset();
    void clonePreset();

    void selectionChanged();
    void setupPreset(const PlaylistPreset& preset);

    void clearBlocks();

private:
    Widgets::Playlist::PresetRegistry* m_presetRegistry;

    QComboBox* m_presetBox;
    QTabWidget* m_presetTabs;

    PresetInputBox* m_headerTitle;
    PresetInputBox* m_headerSubtitle;
    PresetInputBox* m_headerSideText;
    PresetInputBox* m_headerInfo;
    QSpinBox* m_headerRowHeight;

    PresetInputBox* m_subHeaderText;
    QSpinBox* m_subHeaderRowHeight;

    PresetInputBox* m_trackText;
    QSpinBox* m_trackRowHeight;

    QCheckBox* m_showCover;
    QCheckBox* m_simpleHeader;

    QPushButton* m_newPreset;
    QPushButton* m_deletePreset;
    QPushButton* m_updatePreset;
    QPushButton* m_clonePreset;
};

PlaylistPresetsPageWidget::PlaylistPresetsPageWidget(Widgets::Playlist::PresetRegistry* presetRegistry)
    : m_presetRegistry{presetRegistry}
    , m_presetBox{new QComboBox(this)}
    , m_presetTabs{new QTabWidget(this)}
    , m_headerRowHeight{new QSpinBox(this)}
    , m_subHeaderRowHeight{new QSpinBox(this)}
    , m_trackRowHeight{new QSpinBox(this)}
    , m_showCover{new QCheckBox(tr("Show Cover"), this)}
    , m_simpleHeader{new QCheckBox(tr("Simple Header"), this)}
    , m_newPreset{new QPushButton(tr("New"), this)}
    , m_deletePreset{new QPushButton(tr("Delete"), this)}
    , m_updatePreset{new QPushButton(tr("Update"), this)}
    , m_clonePreset{new QPushButton(tr("Clone"), this)}
{
    auto* mainLayout = new QGridLayout(this);

    mainLayout->addWidget(m_presetBox, 0, 0, 1, 4, Qt::AlignTop);
    mainLayout->addWidget(m_newPreset, 1, 0, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(m_clonePreset, 1, 1, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(m_updatePreset, 1, 2, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(m_deletePreset, 1, 3, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(m_presetTabs, 2, 0, 2, 4, Qt::AlignTop);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    auto* headerWidget = new QWidget();
    auto* headerLayout = new QGridLayout(headerWidget);

    m_headerTitle         = new PresetInputBox(tr("Title: "), this);
    m_headerSubtitle      = new PresetInputBox(tr("Subtitle: "), this);
    m_headerSideText      = new PresetInputBox(tr("Side text: "), this);
    m_headerInfo          = new PresetInputBox(tr("Info: "), this);
    auto* headerRowHeight = new QLabel(tr("Row height: "), this);

    m_headerRowHeight->setMinimumWidth(120);
    m_headerRowHeight->setMaximumWidth(120);

    headerLayout->addWidget(headerRowHeight, 0, 0);
    headerLayout->addWidget(m_headerRowHeight, 1, 0);
    headerLayout->addWidget(m_simpleHeader, 1, 1);
    headerLayout->addWidget(m_showCover, 1, 2);
    headerLayout->addWidget(m_headerTitle, 2, 0, 1, 5);
    headerLayout->addWidget(m_headerSubtitle, 3, 0, 1, 5);
    headerLayout->addWidget(m_headerSideText, 4, 0, 1, 5);
    headerLayout->addWidget(m_headerInfo, 5, 0, 1, 5);

    headerLayout->setRowStretch(headerLayout->rowCount(), 1);
    headerLayout->setColumnStretch(0, 1);

    m_presetTabs->addTab(headerWidget, tr("Header"));

    auto* subheaderWidget = new QWidget();
    auto* subheaderLayout = new QGridLayout(subheaderWidget);

    auto* subHeaderRowHeight = new QLabel(tr("Row height: "), this);

    m_subHeaderRowHeight->setMinimumWidth(120);
    m_subHeaderRowHeight->setMaximumWidth(120);

    m_subHeaderText = new PresetInputBox("Text: ", this);

    subheaderLayout->addWidget(subHeaderRowHeight, 0, 0);
    subheaderLayout->addWidget(m_subHeaderRowHeight, 1, 0);
    subheaderLayout->addWidget(m_subHeaderText, 2, 0, 1, 3);

    m_presetTabs->addTab(subheaderWidget, tr("Subheaders"));

    auto* tracksWidget = new QWidget();
    auto* trackLayout  = new QGridLayout(tracksWidget);

    subheaderLayout->setRowStretch(subheaderLayout->rowCount(), 1);

    auto* trackRowHeight = new QLabel(tr("Row height: "), this);

    m_trackRowHeight->setMinimumWidth(120);
    m_trackRowHeight->setMaximumWidth(120);

    m_trackText = new PresetInputBox("Text: ", this);

    trackLayout->addWidget(trackRowHeight, 0, 0);
    trackLayout->addWidget(m_trackRowHeight, 1, 0);
    trackLayout->addWidget(m_trackText, 2, 0, 1, 2);

    trackLayout->setRowStretch(trackLayout->rowCount(), 1);

    m_presetTabs->addTab(tracksWidget, tr("Tracks"));

    QObject::connect(m_presetBox, &QComboBox::currentIndexChanged, this, &PlaylistPresetsPageWidget::selectionChanged);

    QObject::connect(m_newPreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::newPreset);
    QObject::connect(m_deletePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::deletePreset);
    QObject::connect(m_updatePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::updatePreset);
    QObject::connect(m_clonePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::clonePreset);

    QObject::connect(m_simpleHeader, &QPushButton::clicked, this, [this](bool checked) {
        m_showCover->setEnabled(!checked);
        m_headerSubtitle->setEnabled(!checked);
        m_headerInfo->setEnabled(!checked);
    });

    const auto& presets = m_presetRegistry->items();

    for(const auto& [index, preset] : presets) {
        m_presetBox->addItem(preset.name, QVariant::fromValue(preset));
    }
}

void PlaylistPresetsPageWidget::apply()
{
    updatePreset();
}

void PlaylistPresetsPageWidget::newPreset()
{
    PlaylistPreset preset;
    preset.name                      = "New preset";
    const PlaylistPreset addedPreset = m_presetRegistry->addItem(preset);
    if(addedPreset.isValid()) {
        m_presetBox->addItem(addedPreset.name, QVariant::fromValue(addedPreset));
        m_presetBox->setCurrentIndex(m_presetBox->count() - 1);
    }
}

void PlaylistPresetsPageWidget::deletePreset()
{
    const auto preset = m_presetBox->currentData().value<PlaylistPreset>();

    if(m_presetRegistry->removeByIndex(preset.index)) {
        m_presetBox->removeItem(preset.index);
    }
}

void PlaylistPresetsPageWidget::updatePreset()
{
    auto preset = m_presetBox->currentData().value<PlaylistPreset>();

    updateTextBlocks(m_headerTitle->blocks(), preset.header.title);
    updateTextBlocks(m_headerSubtitle->blocks(), preset.header.subtitle);
    updateTextBlocks(m_headerSideText->blocks(), preset.header.sideText);
    updateTextBlocks(m_headerInfo->blocks(), preset.header.info);

    preset.header.rowHeight = m_headerRowHeight->value();
    preset.header.simple    = m_simpleHeader->isChecked();
    preset.header.showCover = m_showCover->isEnabled() && m_showCover->isChecked();

    updateTextBlocks(m_subHeaderText->blocks(), preset.subHeader.text);
    preset.subHeader.rowHeight = m_subHeaderRowHeight->value();

    updateTextBlocks(m_trackText->blocks(), preset.track.text);
    preset.track.rowHeight = m_trackRowHeight->value();

    if(preset != m_presetRegistry->itemByIndex(preset.index)) {
        m_presetRegistry->changeItem(preset);
    }
}

void PlaylistPresetsPageWidget::clonePreset()
{
    const auto preset = m_presetBox->currentData().value<PlaylistPreset>();

    PlaylistPreset clonedPreset{preset};
    clonedPreset.name                = QString{"Copy of %1"}.arg(preset.name);
    const PlaylistPreset addedPreset = m_presetRegistry->addItem(clonedPreset);
    if(addedPreset.isValid()) {
        m_presetBox->addItem(addedPreset.name, QVariant::fromValue(addedPreset));
        m_presetBox->setCurrentIndex(m_presetBox->count() - 1);
    }
}

void PlaylistPresetsPageWidget::selectionChanged()
{
    const auto preset = m_presetBox->currentData().value<PlaylistPreset>();
    if(!preset.isValid()) {
        return;
    }
    clearBlocks();
    setupPreset(preset);
}

void PlaylistPresetsPageWidget::setupPreset(const PlaylistPreset& preset)
{
    createPresetInputs(preset.header.title, m_headerTitle, this);
    createPresetInputs(preset.header.subtitle, m_headerSubtitle, this);
    createPresetInputs(preset.header.sideText, m_headerSideText, this);
    createPresetInputs(preset.header.info, m_headerInfo, this);

    m_headerRowHeight->setValue(preset.header.rowHeight);

    m_simpleHeader->setChecked(preset.header.simple);
    m_showCover->setChecked(preset.header.showCover);
    m_showCover->setEnabled(!preset.header.simple);

    createPresetInputs(preset.subHeader.text, m_subHeaderText, this);
    m_subHeaderRowHeight->setValue(preset.subHeader.rowHeight);

    m_headerSubtitle->setEnabled(!preset.header.simple);
    m_headerInfo->setEnabled(!preset.header.simple);

    createPresetInputs(preset.track.text, m_trackText, this);
    m_trackRowHeight->setValue(preset.track.rowHeight);
}

void PlaylistPresetsPageWidget::clearBlocks()
{
    m_headerTitle->clearBlocks();
    m_headerSubtitle->clearBlocks();
    m_headerSideText->clearBlocks();
    m_headerInfo->clearBlocks();
    m_subHeaderText->clearBlocks();
    m_trackText->clearBlocks();
}

PlaylistPresetsPage::PlaylistPresetsPage(Widgets::Playlist::PresetRegistry* presetRegistry,
                                         Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::PlaylistPresets);
    setName(tr("Presets"));
    setCategory({"Playlist", "Presets"});
    setWidgetCreator([presetRegistry] {
        return new PlaylistPresetsPageWidget(presetRegistry);
    });
}
} // namespace Fy::Gui::Settings
