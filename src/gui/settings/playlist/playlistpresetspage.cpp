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
#include "presetmodel.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
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
}

void updateTextBlock(PresetInput* presetInput, TextBlock& textBlock)
{
    textBlock.text   = presetInput->text();
    textBlock.font   = presetInput->font();
    textBlock.colour = presetInput->colour();

    auto state = presetInput->state();

    textBlock.fontChanged   = state & PresetInput::FontChanged;
    textBlock.colourChanged = state & PresetInput::ColourChanged;

    presetInput->resetState();
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
    input->setText(block.text);
    input->setFont(block.font);
    input->setColour(block.colour);
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

    void selectionChanged(const QItemSelection& selected);
    void setupPreset(const PlaylistPreset& preset);

    void clearBlocks();

private:
    Widgets::Playlist::PresetRegistry* m_presetRegistry;

    QTableView* m_presetList;
    PresetModel* m_model;

    PresetInput* m_headerTitle;
    PresetInput* m_headerSubtitle;
    PresetInput* m_headerSideText;
    PresetInput* m_headerInfo;
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
    , m_presetList{new QTableView(this)}
    , m_model{new PresetModel(presetRegistry, this)}
    , m_headerTitle{new PresetInput(this)}
    , m_headerSubtitle{new PresetInput(this)}
    , m_headerSideText{new PresetInput(this)}
    , m_headerInfo{new PresetInput(this)}
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
    m_presetList->setModel(m_model);

    // Hide index column
    m_presetList->hideColumn(0);

    m_presetList->verticalHeader()->hide();
    m_presetList->horizontalHeader()->hide();
    m_presetList->horizontalHeader()->setStretchLastSection(true);
    m_presetList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* mainLayout    = new QGridLayout(this);
    auto* detailsLayout = new QGridLayout();

    mainLayout->addWidget(m_presetList, 0, 0, 1, 2, Qt::AlignTop);
    mainLayout->addWidget(m_newPreset, 1, 0, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(m_clonePreset, 1, 1, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(m_updatePreset, 2, 0, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(m_deletePreset, 2, 1, 1, 1, Qt::AlignTop);

    mainLayout->addLayout(detailsLayout, 0, 2, 4, 3);

    auto* headerGroup  = new QGroupBox(tr("Header: "), this);
    auto* headerLayout = new QGridLayout(headerGroup);

    auto* headerTitle     = new QLabel(tr("Title: "), this);
    auto* headerSubtitle  = new QLabel(tr("Subtitle: "), this);
    auto* headerSideText  = new QLabel(tr("Side text: "), this);
    auto* headerInfo      = new QLabel(tr("Info: "), this);
    auto* headerRowHeight = new QLabel(tr("Row height: "), this);

    m_headerRowHeight->setMinimumWidth(120);
    m_headerRowHeight->setMaximumWidth(120);

    headerLayout->addWidget(headerRowHeight, 0, 0);
    headerLayout->addWidget(m_headerRowHeight, 1, 0);
    headerLayout->addWidget(m_simpleHeader, 0, 1);
    headerLayout->addWidget(m_showCover, 1, 1);
    headerLayout->addWidget(headerTitle, 2, 0);
    headerLayout->addWidget(m_headerTitle, 3, 0, 1, 5);
    headerLayout->addWidget(headerSubtitle, 4, 0);
    headerLayout->addWidget(m_headerSubtitle, 5, 0, 1, 5);
    headerLayout->addWidget(headerSideText, 6, 0);
    headerLayout->addWidget(m_headerSideText, 7, 0, 1, 5);
    headerLayout->addWidget(headerInfo, 8, 0);
    headerLayout->addWidget(m_headerInfo, 9, 0, 1, 5);

    headerLayout->setRowStretch(headerLayout->rowCount(), 1);
    headerLayout->setColumnStretch(0, 1);

    mainLayout->addWidget(headerGroup, 3, 0, 1, 2);

    auto* subHeaderGroup  = new QGroupBox(tr("Sub Header: "), this);
    auto* subHeaderLayout = new QGridLayout(subHeaderGroup);

    auto* subHeaderRowHeight = new QLabel(tr("Row height: "), this);

    m_subHeaderRowHeight->setMinimumWidth(120);
    m_subHeaderRowHeight->setMaximumWidth(120);

    m_subHeaderText = new PresetInputBox("Text: ", this);

    subHeaderLayout->addWidget(subHeaderRowHeight, 0, 0);
    subHeaderLayout->addWidget(m_subHeaderRowHeight, 1, 0);
    subHeaderLayout->addWidget(m_subHeaderText, 2, 0, 1, 3);

    detailsLayout->addWidget(subHeaderGroup, 1, 0);

    auto* trackGroup  = new QGroupBox(tr("Track: "), this);
    auto* trackLayout = new QGridLayout(trackGroup);

    subHeaderLayout->setRowStretch(subHeaderLayout->rowCount(), 1);

    auto* trackRowHeight = new QLabel(tr("Row height: "), this);

    m_trackRowHeight->setMinimumWidth(120);
    m_trackRowHeight->setMaximumWidth(120);

    m_trackText = new PresetInputBox("Text: ", this);

    trackLayout->addWidget(trackRowHeight, 0, 0);
    trackLayout->addWidget(m_trackRowHeight, 1, 0);
    trackLayout->addWidget(m_trackText, 2, 0, 1, 2);

    trackLayout->setRowStretch(trackLayout->rowCount(), 1);

    detailsLayout->addWidget(trackGroup, 2, 0);

    detailsLayout->setRowStretch(detailsLayout->rowCount(), 1);

    QObject::connect(m_presetList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &PlaylistPresetsPageWidget::selectionChanged);

    QObject::connect(m_newPreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::newPreset);
    QObject::connect(m_deletePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::deletePreset);
    QObject::connect(m_updatePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::updatePreset);
    QObject::connect(m_clonePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::clonePreset);

    QObject::connect(m_simpleHeader, &QPushButton::clicked, this, [this](bool checked) {
        m_showCover->setEnabled(!checked);
    });

    if(m_model->rowCount() > 0) {
        m_presetList->selectRow(0);
        m_presetList->setFocus();
    }
}

void PlaylistPresetsPageWidget::apply()
{
    updatePreset();
    m_model->processQueue();
}

void PlaylistPresetsPageWidget::newPreset()
{
    m_model->addNewPreset();

    const int row = m_model->rowCount() - 1;
    m_presetList->selectRow(row);
    m_presetList->edit(m_model->index(row, 1, {}));
}

void PlaylistPresetsPageWidget::deletePreset()
{
    const auto selection = m_presetList->selectionModel()->selectedRows();
    if(selection.empty() || m_model->rowCount() == 1) {
        return;
    }
    const auto index  = selection.constFirst();
    const auto* item  = static_cast<PresetItem*>(index.internalPointer());
    const auto preset = item->preset();

    m_model->markForRemoval(preset);
}

void PlaylistPresetsPageWidget::updatePreset()
{
    const auto selection = m_presetList->selectionModel()->selectedRows();
    if(selection.empty()) {
        return;
    }
    const auto index      = selection.constFirst();
    const auto* item      = static_cast<PresetItem*>(index.internalPointer());
    PlaylistPreset preset = item->preset();

    updateTextBlock(m_headerTitle, preset.header.title);
    updateTextBlock(m_headerSubtitle, preset.header.subtitle);
    updateTextBlock(m_headerSideText, preset.header.sideText);
    updateTextBlock(m_headerInfo, preset.header.info);

    preset.header.rowHeight = m_headerRowHeight->value();
    preset.header.simple    = m_simpleHeader->isChecked();
    preset.header.showCover = m_showCover->isEnabled() && m_showCover->isChecked();

    preset.subHeaders.rows.clear();
    for(const auto& input : m_subHeaderText->blocks()) {
        const QStringList subHeader = input->text().split("||", Qt::KeepEmptyParts);

        if(subHeader.empty()) {
            continue;
        }

        const auto savePart = [&input](const QString& text) -> TextBlock {
            TextBlock block;
            block.text   = text;
            block.font   = input->font();
            block.colour = input->colour();

            auto state = input->state();

            block.fontChanged   = state & PresetInput::FontChanged;
            block.colourChanged = state & PresetInput::ColourChanged;

            return block;
        };

        Widgets::Playlist::SubheaderRow row;
        row.title = savePart(subHeader[0]);

        if(subHeader.size() > 1) {
            row.info = savePart(subHeader[1]);
        }
        preset.subHeaders.rows.emplace_back(row);

        input->resetState();
    }

    preset.subHeaders.rowHeight = m_subHeaderRowHeight->value();

    updateTextBlocks(m_trackText->blocks(), preset.track.text);
    preset.track.rowHeight = m_trackRowHeight->value();

    if(preset != m_presetRegistry->presetByIndex(preset.index)) {
        m_model->markForChange(preset);
    }
}

void PlaylistPresetsPageWidget::clonePreset()
{
    const auto selection = m_presetList->selectionModel()->selectedRows();
    if(selection.empty()) {
        return;
    }
    const auto index = selection.constFirst();
    const auto* item = static_cast<PresetItem*>(index.internalPointer());

    if(!item) {
        return;
    }

    PlaylistPreset preset{item->preset()};
    preset.name = QString{"Copy of %1"}.arg(preset.name);

    m_model->addNewPreset(preset);

    const int row = m_model->rowCount() - 1;
    m_presetList->selectRow(row);
}

void PlaylistPresetsPageWidget::selectionChanged(const QItemSelection& selected)
{
    if(selected.empty()) {
        return;
    }
    const auto index = m_presetList->selectionModel()->selectedRows().constFirst();
    const auto* item = static_cast<PresetItem*>(index.internalPointer());
    if(!item) {
        return;
    }
    clearBlocks();
    const auto preset = item->preset();
    setupPreset(preset);
}

void PlaylistPresetsPageWidget::setupPreset(const PlaylistPreset& preset)
{
    setupInputBox(preset.header.title, m_headerTitle);
    setupInputBox(preset.header.subtitle, m_headerSubtitle);
    setupInputBox(preset.header.sideText, m_headerSideText);
    setupInputBox(preset.header.info, m_headerInfo);

    m_headerRowHeight->setValue(preset.header.rowHeight);
    m_showCover->setChecked(preset.header.showCover);
    m_simpleHeader->setChecked(preset.header.simple);

    for(const auto& row : preset.subHeaders.rows) {
        QString text{row.title.text};
        if(!row.info.text.isEmpty()) {
            text = text + "||" + row.info.text;
        }
        auto* input = new PresetInput(this);
        input->setText(text);
        input->setFont(row.title.font);
        input->setColour(row.title.colour);
        m_subHeaderText->addInput(input);
    }

    m_subHeaderRowHeight->setValue(preset.subHeaders.rowHeight);

    createPresetInputs(preset.track.text, m_trackText, this);

    m_trackRowHeight->setValue(preset.track.rowHeight);
}

void PlaylistPresetsPageWidget::clearBlocks()
{
    m_subHeaderText->clearBlocks();
    m_trackText->clearBlocks();
}

PlaylistPresetsPage::PlaylistPresetsPage(Widgets::Playlist::PresetRegistry* presetRegistry,
                                         Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::PlaylistPresets);
    setName(tr("Presets"));
    setCategory("Category.Playlist");
    setCategoryName(tr("Playlist"));
    setWidgetCreator([presetRegistry] {
        return new PlaylistPresetsPageWidget(presetRegistry);
    });
    setCategoryIconPath(Constants::Icons::Category::Playlist);
}
} // namespace Fy::Gui::Settings
