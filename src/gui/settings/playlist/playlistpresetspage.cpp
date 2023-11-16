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

#include "playlist/playlistpreset.h"
#include "playlist/presetregistry.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/customisableinput.h>
#include <utils/enum.h>
#include <utils/expandableinputbox.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
void setupInputBox(const Fooyin::TextBlock& preset, Fooyin::CustomisableInput* block)
{
    block->setText(preset.text);
    block->setFont(preset.font);
    block->setColour(preset.colour);

    Fooyin::CustomisableInput::State state;
    if(preset.colourChanged) {
        state |= Fooyin::CustomisableInput::ColourChanged;
    }
    if(preset.fontChanged) {
        state |= Fooyin::CustomisableInput::FontChanged;
    }
    block->setState(state);
}

void updateTextBlock(const Fooyin::CustomisableInput* presetInput, Fooyin::TextBlock& textBlock)
{
    textBlock.text   = presetInput->text();
    textBlock.font   = presetInput->font();
    textBlock.colour = presetInput->colour();

    auto state = presetInput->state();

    textBlock.fontChanged   = state & Fooyin::CustomisableInput::FontChanged;
    textBlock.colourChanged = state & Fooyin::CustomisableInput::ColourChanged;
}

void updateTextBlocks(const Fooyin::ExpandableInputList& presetInputs, Fooyin::TextBlockList& textBlocks)
{
    textBlocks.clear();

    for(const auto& input : presetInputs) {
        if(!input->text().isEmpty()) {
            if(auto presetInput = qobject_cast<Fooyin::CustomisableInput*>(input)) {
                Fooyin::TextBlock block;
                updateTextBlock(presetInput, block);
                textBlocks.emplace_back(block);
            }
        }
    }
}

void createPresetInput(const Fooyin::TextBlock& block, Fooyin::ExpandableInputBox* box, QWidget* parent)
{
    auto* input = new Fooyin::CustomisableInput(parent);
    setupInputBox(block, input);
    box->addInput(input);
}

void createPresetInputs(const Fooyin::TextBlockList& blocks, Fooyin::ExpandableInputBox* box, QWidget* parent)
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
} // namespace

namespace Fooyin {
class ExpandableGroupBox : public ExpandableInput
{
    Q_OBJECT

public:
    explicit ExpandableGroupBox(int rowHeight, QWidget* parent = nullptr)
        : ExpandableInput{ExpandableInput::CustomWidget, parent}
        , m_groupBox{new QGroupBox(this)}
        , m_rowHeight{new QSpinBox(this)}
        , m_leftBox{new ExpandableInputBox(tr("Left-aligned text: "), ExpandableInput::CustomWidget, this)}
        , m_rightBox{new ExpandableInputBox(tr("Right-aligned text: "), ExpandableInput::CustomWidget, this)}
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_groupBox);

        auto* groupLayout = new QGridLayout(m_groupBox);

        m_rowHeight->setValue(rowHeight);

        m_leftBox->setInputWidget([](QWidget* parent) { return new CustomisableInput(parent); });
        m_rightBox->setInputWidget([](QWidget* parent) { return new CustomisableInput(parent); });

        auto* rowHeightLabel = new QLabel(tr("Row height: "), this);

        groupLayout->addWidget(rowHeightLabel, 0, 0);
        groupLayout->addWidget(m_rowHeight, 0, 1);
        groupLayout->addWidget(m_leftBox, 1, 0, 1, 3);
        groupLayout->addWidget(m_rightBox, 2, 0, 1, 3);

        groupLayout->setColumnStretch(2, 1);
    }

    void addLeftInput(const TextBlock& preset)
    {
        addInput(preset, m_leftBox);
    }

    void addRightInput(const TextBlock& preset)
    {
        addInput(preset, m_rightBox);
    }

    ExpandableInputList leftBlocks() const
    {
        return m_leftBox->blocks();
    }

    ExpandableInputList rightBlocks() const
    {
        return m_rightBox->blocks();
    }

    int rowHeight() const
    {
        return m_rowHeight->value();
    }

private:
    void addInput(const TextBlock& preset, ExpandableInputBox* box)
    {
        auto* block = new CustomisableInput(this);
        block->setText(preset.text);
        block->setFont(preset.font);
        block->setColour(preset.colour);

        CustomisableInput::State state;
        if(preset.colourChanged) {
            state |= CustomisableInput::ColourChanged;
        }
        if(preset.fontChanged) {
            state |= CustomisableInput::FontChanged;
        }
        block->setState(state);

        box->addInput(block);
    }

    QGroupBox* m_groupBox;
    QSpinBox* m_rowHeight;
    ExpandableInputBox* m_leftBox;
    ExpandableInputBox* m_rightBox;
};

void createGroupPresetInputs(const SubheaderRow& subheader, ExpandableInputBox* box, QWidget* parent)
{
    if(!subheader.isValid()) {
        return;
    }

    auto* input = new ExpandableGroupBox(subheader.rowHeight, parent);

    if(subheader.leftText.empty()) {
        input->addLeftInput({});
    }
    else {
        for(const auto& block : subheader.leftText) {
            input->addLeftInput(block);
        }
    }

    if(subheader.rightText.empty()) {
        input->addRightInput({});
    }
    else {
        for(const auto& block : subheader.rightText) {
            input->addRightInput(block);
        }
    }

    box->addInput(input);
}

void updateGroupTextBlocks(const ExpandableInputList& presetInputs, SubheaderRows& textBlocks)
{
    textBlocks.clear();

    for(const auto& input : presetInputs) {
        if(auto presetInput = qobject_cast<ExpandableGroupBox*>(input)) {
            SubheaderRow block;

            auto leftBlocks  = presetInput->leftBlocks();
            auto rightBlocks = presetInput->rightBlocks();

            updateTextBlocks(leftBlocks, block.leftText);
            updateTextBlocks(rightBlocks, block.rightText);
            block.rowHeight = presetInput->rowHeight();

            textBlocks.emplace_back(block);
        }
    }
}

class PlaylistPresetsPageWidget : public SettingsPageWidget
{
public:
    explicit PlaylistPresetsPageWidget(PresetRegistry* presetRegistry, SettingsManager* settings);

    void apply() override;
    void reset() override;

    void populatePresets();

    void newPreset();
    void renamePreset();
    void deletePreset();
    void updatePreset(bool force = false);
    void clonePreset();

    void selectionChanged();
    void setupPreset(const PlaylistPreset& preset);

    void clearBlocks();

private:
    PresetRegistry* m_presetRegistry;
    SettingsManager* m_settings;

    QComboBox* m_presetBox;
    QTabWidget* m_presetTabs;

    ExpandableInputBox* m_headerTitle;
    ExpandableInputBox* m_headerSubtitle;
    ExpandableInputBox* m_headerSideText;
    ExpandableInputBox* m_headerInfo;
    QSpinBox* m_headerRowHeight;

    ExpandableInputBox* m_subHeaders;

    ExpandableInputBox* m_trackLeftText;
    ExpandableInputBox* m_trackRightText;
    QSpinBox* m_trackRowHeight;

    QCheckBox* m_showCover;
    QCheckBox* m_simpleHeader;

    QPushButton* m_newPreset;
    QPushButton* m_renamePreset;
    QPushButton* m_deletePreset;
    QPushButton* m_updatePreset;
    QPushButton* m_clonePreset;
};

PlaylistPresetsPageWidget::PlaylistPresetsPageWidget(PresetRegistry* presetRegistry, SettingsManager* settings)
    : m_presetRegistry{presetRegistry}
    , m_settings{settings}
    , m_presetBox{new QComboBox(this)}
    , m_presetTabs{new QTabWidget(this)}
    , m_headerRowHeight{new QSpinBox(this)}
    , m_trackRowHeight{new QSpinBox(this)}
    , m_showCover{new QCheckBox(tr("Show Cover"), this)}
    , m_simpleHeader{new QCheckBox(tr("Simple Header"), this)}
    , m_newPreset{new QPushButton(tr("New"), this)}
    , m_renamePreset{new QPushButton(tr("Rename"), this)}
    , m_deletePreset{new QPushButton(tr("Delete"), this)}
    , m_updatePreset{new QPushButton(tr("Update"), this)}
    , m_clonePreset{new QPushButton(tr("Clone"), this)}
{
    auto* mainLayout = new QGridLayout(this);

    mainLayout->addWidget(m_presetBox, 0, 0, 1, 5, Qt::AlignTop);
    mainLayout->addWidget(m_newPreset, 1, 0, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(m_renamePreset, 1, 1, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(m_clonePreset, 1, 2, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(m_updatePreset, 1, 3, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(m_deletePreset, 1, 4, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(m_presetTabs, 2, 0, 2, 5, Qt::AlignTop);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    auto* headerWidget = new QWidget();
    auto* headerLayout = new QGridLayout(headerWidget);

    const auto inputAttributes = ExpandableInput::CustomWidget;

    m_headerTitle    = new ExpandableInputBox(tr("Title: "), inputAttributes, this);
    m_headerSubtitle = new ExpandableInputBox(tr("Subtitle: "), inputAttributes, this);
    m_headerSideText = new ExpandableInputBox(tr("Side text: "), inputAttributes, this);
    m_headerInfo     = new ExpandableInputBox(tr("Info: "), inputAttributes, this);

    m_headerTitle->setInputWidget([](QWidget* parent) { return new CustomisableInput(parent); });
    m_headerSideText->setInputWidget([](QWidget* parent) { return new CustomisableInput(parent); });
    m_headerSideText->setInputWidget([](QWidget* parent) { return new CustomisableInput(parent); });
    m_headerInfo->setInputWidget([](QWidget* parent) { return new CustomisableInput(parent); });

    auto* headerRowHeight = new QLabel(tr("Row height: "), this);

    headerLayout->addWidget(headerRowHeight, 0, 0);
    headerLayout->addWidget(m_headerRowHeight, 0, 1);
    headerLayout->addWidget(m_simpleHeader, 1, 0, 1, 2);
    headerLayout->addWidget(m_showCover, 2, 0, 1, 2);
    headerLayout->addWidget(m_headerTitle, 3, 0, 1, 5);
    headerLayout->addWidget(m_headerSubtitle, 4, 0, 1, 5);
    headerLayout->addWidget(m_headerSideText, 5, 0, 1, 5);
    headerLayout->addWidget(m_headerInfo, 6, 0, 1, 5);

    headerLayout->setColumnStretch(4, 1);
    headerLayout->setRowStretch(headerLayout->rowCount(), 1);

    m_presetTabs->addTab(headerWidget, tr("Header"));

    auto* subheaderWidget = new QWidget();
    auto* subheaderLayout = new QGridLayout(subheaderWidget);

    m_subHeaders = new ExpandableInputBox(tr("Subheaders: "), inputAttributes, this);
    m_subHeaders->setInputWidget([](QWidget* parent) {
        SubheaderRow subheader;
        auto* groupBox = new ExpandableGroupBox(subheader.rowHeight, parent);
        groupBox->addLeftInput({});
        groupBox->addRightInput({});
        return groupBox;
    });

    subheaderLayout->addWidget(m_subHeaders, 0, 0, 1, 3);

    m_presetTabs->addTab(subheaderWidget, tr("Subheaders"));

    auto* tracksWidget = new QWidget();
    auto* trackLayout  = new QGridLayout(tracksWidget);

    subheaderLayout->setRowStretch(subheaderLayout->rowCount(), 1);

    auto* trackRowHeight = new QLabel(tr("Row height: "), this);

    m_trackLeftText = new ExpandableInputBox(tr("Left-aligned text: "), inputAttributes, this);
    m_trackLeftText->setInputWidget([](QWidget* parent) { return new CustomisableInput(parent); });
    m_trackRightText = new ExpandableInputBox(tr("Right-aligned text: "), inputAttributes, this);
    m_trackRightText->setInputWidget([](QWidget* parent) { return new CustomisableInput(parent); });

    trackLayout->addWidget(trackRowHeight, 0, 0);
    trackLayout->addWidget(m_trackRowHeight, 0, 1);
    trackLayout->addWidget(m_trackLeftText, 1, 0, 1, 3);
    trackLayout->addWidget(m_trackRightText, 2, 0, 1, 3);

    trackLayout->setColumnStretch(2, 1);
    trackLayout->setRowStretch(trackLayout->rowCount(), 1);

    m_presetTabs->addTab(tracksWidget, tr("Tracks"));

    QObject::connect(m_presetBox, &QComboBox::currentIndexChanged, this, &PlaylistPresetsPageWidget::selectionChanged);

    QObject::connect(m_newPreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::newPreset);
    QObject::connect(m_renamePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::renamePreset);
    QObject::connect(m_deletePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::deletePreset);
    QObject::connect(m_updatePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::updatePreset);
    QObject::connect(m_clonePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::clonePreset);

    QObject::connect(m_simpleHeader, &QPushButton::clicked, this, [this](bool checked) {
        m_showCover->setEnabled(!checked);
        m_headerSubtitle->setEnabled(!checked);
        m_headerInfo->setEnabled(!checked);
    });

    populatePresets();
}

void PlaylistPresetsPageWidget::apply()
{
    updatePreset();
}

void PlaylistPresetsPageWidget::reset()
{
    m_settings->reset<Settings::Gui::PlaylistPresets>();

    m_presetRegistry->loadItems();
    populatePresets();

    updatePreset(true);
}

void PlaylistPresetsPageWidget::populatePresets()
{
    m_presetBox->clear();

    const auto& presets = m_presetRegistry->items();

    for(const auto& [index, preset] : presets) {
        m_presetBox->insertItem(index, preset.name, QVariant::fromValue(preset));
    }
}

void PlaylistPresetsPageWidget::newPreset()
{
    PlaylistPreset preset;
    preset.name = tr("New preset");

    bool success{false};
    const QString text
        = QInputDialog::getText(this, tr("Add Preset"), tr("Preset Name:"), QLineEdit::Normal, preset.name, &success);

    if(success && !text.isEmpty()) {
        preset.name                      = text;
        const PlaylistPreset addedPreset = m_presetRegistry->addItem(preset);
        if(addedPreset.isValid()) {
            m_presetBox->addItem(addedPreset.name, QVariant::fromValue(addedPreset));
            m_presetBox->setCurrentIndex(m_presetBox->count() - 1);
        }
    }
}

void PlaylistPresetsPageWidget::renamePreset()
{
    auto preset = m_presetBox->currentData().value<PlaylistPreset>();

    bool success{false};
    const QString text = QInputDialog::getText(this, tr("Rename Preset"), tr("Preset Name:"), QLineEdit::Normal,
                                               preset.name, &success);

    if(success && !text.isEmpty()) {
        preset.name = text;
        if(m_presetRegistry->changeItem(preset)) {
            m_presetBox->setItemText(m_presetBox->currentIndex(), preset.name);
            m_presetBox->setItemData(m_presetBox->currentIndex(), QVariant::fromValue(preset));
        }
    }
}

void PlaylistPresetsPageWidget::deletePreset()
{
    if(m_presetBox->count() <= 1) {
        return;
    }

    const auto preset = m_presetBox->currentData().value<PlaylistPreset>();

    if(m_presetRegistry->removeByIndex(preset.index)) {
        m_presetBox->removeItem(preset.index);
    }
}

void PlaylistPresetsPageWidget::updatePreset(bool force)
{
    auto preset = m_presetBox->currentData().value<PlaylistPreset>();

    updateTextBlocks(m_headerTitle->blocks(), preset.header.title);
    updateTextBlocks(m_headerSubtitle->blocks(), preset.header.subtitle);
    updateTextBlocks(m_headerSideText->blocks(), preset.header.sideText);
    updateTextBlocks(m_headerInfo->blocks(), preset.header.info);

    preset.header.rowHeight = m_headerRowHeight->value();
    preset.header.simple    = m_simpleHeader->isChecked();
    preset.header.showCover = m_showCover->isEnabled() && m_showCover->isChecked();

    updateGroupTextBlocks(m_subHeaders->blocks(), preset.subHeaders);

    updateTextBlocks(m_trackLeftText->blocks(), preset.track.leftText);
    updateTextBlocks(m_trackRightText->blocks(), preset.track.rightText);
    preset.track.rowHeight = m_trackRowHeight->value();

    if(force || preset != m_presetRegistry->itemByIndex(preset.index)) {
        m_presetRegistry->changeItem(preset);
    }
}

void PlaylistPresetsPageWidget::clonePreset()
{
    const auto preset = m_presetBox->currentData().value<PlaylistPreset>();

    PlaylistPreset clonedPreset{preset};
    clonedPreset.name                = tr("Copy of ") + preset.name;
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

    for(const auto& subheader : preset.subHeaders) {
        createGroupPresetInputs(subheader, m_subHeaders, this);
    }

    m_headerSubtitle->setEnabled(!preset.header.simple);
    m_headerInfo->setEnabled(!preset.header.simple);

    createPresetInputs(preset.track.leftText, m_trackLeftText, this);
    createPresetInputs(preset.track.rightText, m_trackRightText, this);
    m_trackRowHeight->setValue(preset.track.rowHeight);
}

void PlaylistPresetsPageWidget::clearBlocks()
{
    m_headerTitle->clearBlocks();
    m_headerSubtitle->clearBlocks();
    m_headerSideText->clearBlocks();
    m_headerInfo->clearBlocks();
    m_subHeaders->clearBlocks();
    m_trackLeftText->clearBlocks();
    m_trackRightText->clearBlocks();
}

PlaylistPresetsPage::PlaylistPresetsPage(PresetRegistry* presetRegistry, SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::PlaylistPresets);
    setName(tr("Presets"));
    setCategory({tr("Playlist"), tr("Presets")});
    setWidgetCreator([presetRegistry, settings] { return new PlaylistPresetsPageWidget(presetRegistry, settings); });
}
} // namespace Fooyin

#include "playlistpresetspage.moc"
