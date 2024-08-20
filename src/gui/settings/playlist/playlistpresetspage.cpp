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

#include "playlistpresetspage.h"

#include "internalguisettings.h"
#include "playlist/playlistpreset.h"
#include "playlist/presetregistry.h"

#include <gui/guiconstants.h>
#include <gui/widgets/customisableinput.h>
#include <utils/expandableinputbox.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace Fooyin {
class ExpandableGroupBox : public ExpandableInput
{
    Q_OBJECT

public:
    explicit ExpandableGroupBox(int rowHeight, QWidget* parent = nullptr)
        : ExpandableInput{ExpandableInput::CustomWidget, parent}
        , m_groupBox{new QGroupBox(this)}
        , m_overrideHeight{new QCheckBox(tr("Override height") + QStringLiteral(":"), this)}
        , m_rowHeight{new QSpinBox(this)}
        , m_leftScript{new QTextEdit(this)}
        , m_rightScript{new QTextEdit(this)}
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_groupBox);

        auto* groupLayout = new QGridLayout(m_groupBox);

        m_overrideHeight->setChecked(rowHeight > 0);
        m_rowHeight->setValue(rowHeight);
        m_rowHeight->setEnabled(m_overrideHeight->isChecked());

        auto* leftScript  = new QLabel(tr("Left-aligned") + QStringLiteral(":"), this);
        auto* rightScript = new QLabel(tr("Right-aligned") + QStringLiteral(":"), this);

        m_rowHeight->setMinimum(20);
        m_rowHeight->setMaximum(150);

        groupLayout->addWidget(m_overrideHeight, 0, 0);
        groupLayout->addWidget(m_rowHeight, 0, 1);
        groupLayout->addWidget(leftScript, 1, 0, 1, 3);
        groupLayout->addWidget(m_leftScript, 2, 0, 1, 3);
        groupLayout->addWidget(rightScript, 3, 0, 1, 3);
        groupLayout->addWidget(m_rightScript, 4, 0, 1, 3);

        groupLayout->setColumnStretch(2, 1);

        QObject::connect(m_overrideHeight, &QCheckBox::toggled, this,
                         [this](bool checked) { m_rowHeight->setEnabled(checked); });
    }

    void setLeftScript(const QString& script)
    {
        m_leftScript->setPlainText(script);
    }

    void setRightScript(const QString& script)
    {
        m_rightScript->setPlainText(script);
    }

    [[nodiscard]] QString leftScript() const
    {
        return m_leftScript->toPlainText();
    }

    [[nodiscard]] QString rightScript() const
    {
        return m_rightScript->toPlainText();
    }

    [[nodiscard]] int rowHeight() const
    {
        return m_overrideHeight->isChecked() ? m_rowHeight->value() : 0;
    }

    void setReadOnly(bool readOnly) override
    {
        ExpandableInput::setReadOnly(readOnly);

        m_overrideHeight->setDisabled(readOnly);
        m_rowHeight->setReadOnly(readOnly);
        m_leftScript->setReadOnly(readOnly);
        m_rightScript->setReadOnly(readOnly);
    }

private:
    QGroupBox* m_groupBox;
    QCheckBox* m_overrideHeight;
    QSpinBox* m_rowHeight;
    QTextEdit* m_leftScript;
    QTextEdit* m_rightScript;
};

void createGroupPresetInputs(const SubheaderRow& subheader, ExpandableInputBox* box, QWidget* parent)
{
    if(!subheader.isValid()) {
        return;
    }

    auto* input = new ExpandableGroupBox(subheader.rowHeight, parent);
    box->addInput(input);

    input->setLeftScript(subheader.leftText.script);
    input->setRightScript(subheader.rightText.script);
}

void updateGroupTextBlocks(const ExpandableInputList& presetInputs, SubheaderRows& textBlocks)
{
    textBlocks.clear();

    for(const auto& input : presetInputs) {
        if(auto* presetInput = qobject_cast<ExpandableGroupBox*>(input)) {
            SubheaderRow block;

            block.leftText.script  = presetInput->leftScript();
            block.rightText.script = presetInput->rightScript();
            block.rowHeight        = presetInput->rowHeight();

            textBlocks.emplace_back(block);
        }
    }
}

class PlaylistPresetsPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit PlaylistPresetsPageWidget(PresetRegistry* presetRegistry);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void newPreset();
    void renamePreset();
    void deletePreset();
    void updatePreset();
    void clonePreset();

    void selectionChanged();
    void setupPreset(const PlaylistPreset& preset);

    void clearBlocks();

    PresetRegistry* m_presetRegistry;

    QComboBox* m_presetBox;
    QTabWidget* m_presetTabs;

    QTextEdit* m_headerTitle;
    QTextEdit* m_headerSubtitle;
    QTextEdit* m_headerSideText;
    QTextEdit* m_headerInfo;
    QCheckBox* m_overrideHeaderHeight;
    QSpinBox* m_headerRowHeight;

    ExpandableInputBox* m_subHeaders;

    QTextEdit* m_trackLeftText;
    QTextEdit* m_trackRightText;
    QCheckBox* m_overrideTrackHeight;
    QSpinBox* m_trackRowHeight;

    QCheckBox* m_showCover;
    QCheckBox* m_simpleHeader;

    QPushButton* m_newPreset;
    QPushButton* m_renamePreset;
    QPushButton* m_deletePreset;
    QPushButton* m_updatePreset;
    QPushButton* m_clonePreset;
};

PlaylistPresetsPageWidget::PlaylistPresetsPageWidget(PresetRegistry* presetRegistry)
    : m_presetRegistry{presetRegistry}
    , m_presetBox{new QComboBox(this)}
    , m_presetTabs{new QTabWidget(this)}
    , m_headerTitle{new QTextEdit(this)}
    , m_headerSubtitle{new QTextEdit(this)}
    , m_headerSideText{new QTextEdit(this)}
    , m_headerInfo{new QTextEdit(this)}
    , m_overrideHeaderHeight{new QCheckBox(tr("Override height") + QStringLiteral(":"), this)}
    , m_headerRowHeight{new QSpinBox(this)}
    , m_trackLeftText{new QTextEdit(tr("Left-aligned text") + QStringLiteral(":"), this)}
    , m_trackRightText{new QTextEdit(tr("Right-aligned text") + QStringLiteral(":"), this)}
    , m_overrideTrackHeight{new QCheckBox(tr("Override height") + QStringLiteral(":"), this)}
    , m_trackRowHeight{new QSpinBox(this)}
    , m_showCover{new QCheckBox(tr("Show cover"), this)}
    , m_simpleHeader{new QCheckBox(tr("Simple header"), this)}
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

    auto* headerTitle    = new QLabel(tr("Title") + QStringLiteral(":"), this);
    auto* headerSubtitle = new QLabel(tr("Subtitle") + QStringLiteral(":"), this);
    auto* headerSide     = new QLabel(tr("Side") + QStringLiteral(":"), this);
    auto* headerInfo     = new QLabel(tr("Info") + QStringLiteral(":"), this);

    m_headerRowHeight->setMinimum(50);
    m_headerRowHeight->setMaximum(300);

    int row{0};
    headerLayout->addWidget(m_simpleHeader, row++, 0, 1, 2);
    headerLayout->addWidget(m_showCover, row++, 0, 1, 2);
    headerLayout->addWidget(m_overrideHeaderHeight, row, 0);
    headerLayout->addWidget(m_headerRowHeight, row++, 1);
    headerLayout->addWidget(headerTitle, row++, 0, 1, 5);
    headerLayout->addWidget(m_headerTitle, row++, 0, 1, 5);
    headerLayout->addWidget(headerSubtitle, row++, 0, 1, 5);
    headerLayout->addWidget(m_headerSubtitle, row++, 0, 1, 5);
    headerLayout->addWidget(headerSide, row++, 0, 1, 5);
    headerLayout->addWidget(m_headerSideText, row++, 0, 1, 5);
    headerLayout->addWidget(headerInfo, row++, 0, 1, 5);
    headerLayout->addWidget(m_headerInfo, row++, 0, 1, 5);

    headerLayout->setColumnStretch(4, 1);
    headerLayout->setRowStretch(headerLayout->rowCount(), 1);

    m_presetTabs->addTab(headerWidget, tr("Header"));

    auto* subheaderWidget = new QWidget();
    auto* subheaderLayout = new QGridLayout(subheaderWidget);

    m_subHeaders = new ExpandableInputBox(tr("Subheaders") + QStringLiteral(":"), ExpandableInput::CustomWidget, this);
    m_subHeaders->setInputWidget([](QWidget* parent) {
        const SubheaderRow subheader;
        auto* groupBox = new ExpandableGroupBox(subheader.rowHeight, parent);
        return groupBox;
    });

    subheaderLayout->addWidget(m_subHeaders, 0, 0, 1, 3);

    m_presetTabs->addTab(subheaderWidget, tr("Subheaders"));

    auto* tracksWidget = new QWidget();
    auto* trackLayout  = new QGridLayout(tracksWidget);

    subheaderLayout->setRowStretch(subheaderLayout->rowCount(), 1);

    auto* trackLeft  = new QLabel(tr("Left-aligned") + QStringLiteral(":"), this);
    auto* TrackRight = new QLabel(tr("Right-aligned") + QStringLiteral(":"), this);

    m_trackRowHeight->setMinimum(20);
    m_trackRowHeight->setMaximum(150);

    trackLayout->addWidget(m_overrideTrackHeight, 0, 0);
    trackLayout->addWidget(m_trackRowHeight, 0, 1);
    trackLayout->addWidget(trackLeft, 1, 0, 1, 3);
    trackLayout->addWidget(m_trackLeftText, 2, 0, 1, 3);
    trackLayout->addWidget(TrackRight, 3, 0, 1, 3);
    trackLayout->addWidget(m_trackRightText, 4, 0, 1, 3);

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

    QObject::connect(m_overrideHeaderHeight, &QCheckBox::toggled, m_headerRowHeight, &QSpinBox::setEnabled);
    QObject::connect(m_overrideTrackHeight, &QCheckBox::toggled, m_trackRowHeight, &QSpinBox::setEnabled);
}

void PlaylistPresetsPageWidget::load()
{
    m_presetBox->clear();

    const auto presets = m_presetRegistry->items();
    for(const auto& preset : presets) {
        m_presetBox->insertItem(preset.index, preset.name, preset.id);
    }
}

void PlaylistPresetsPageWidget::apply()
{
    updatePreset();
}

void PlaylistPresetsPageWidget::reset()
{
    m_presetRegistry->reset();
}

void PlaylistPresetsPageWidget::newPreset()
{
    PlaylistPreset preset;
    preset.name = tr("New preset");

    bool success{false};
    const QString text = QInputDialog::getText(this, tr("Add Preset"), tr("Preset Name") + QStringLiteral(":"),
                                               QLineEdit::Normal, preset.name, &success);

    if(success && !text.isEmpty()) {
        preset.name                      = text;
        const PlaylistPreset addedPreset = m_presetRegistry->addItem(preset);
        if(addedPreset.isValid()) {
            m_presetBox->addItem(addedPreset.name, addedPreset.id);
            m_presetBox->setCurrentIndex(m_presetBox->count() - 1);
        }
    }
}

void PlaylistPresetsPageWidget::renamePreset()
{
    const int presetId = m_presetBox->currentData().toInt();

    if(const auto regPreset = m_presetRegistry->itemById(presetId)) {
        auto preset = regPreset.value();

        bool success{false};
        const QString text = QInputDialog::getText(this, tr("Rename Preset"), tr("Preset Name") + QStringLiteral(":"),
                                                   QLineEdit::Normal, preset.name, &success);

        if(success && !text.isEmpty()) {
            preset.name = text;
            if(m_presetRegistry->changeItem(preset)) {
                m_presetBox->setItemText(m_presetBox->currentIndex(), preset.name);
            }
        }
    }
}

void PlaylistPresetsPageWidget::deletePreset()
{
    if(m_presetBox->count() <= 1) {
        return;
    }

    const int presetId = m_presetBox->currentData().toInt();

    if(m_presetRegistry->removeById(presetId)) {
        m_presetBox->removeItem(m_presetBox->currentIndex());
    }
}

void PlaylistPresetsPageWidget::updatePreset()
{
    const int presetId   = m_presetBox->currentData().toInt();
    const auto regPreset = m_presetRegistry->itemById(presetId);
    if(!regPreset) {
        return;
    }

    auto preset = regPreset.value();

    if(preset.isDefault) {
        return;
    }

    preset.header.title.script    = m_headerTitle->toPlainText();
    preset.header.subtitle.script = m_headerSubtitle->toPlainText();
    preset.header.sideText.script = m_headerSideText->toPlainText();
    preset.header.info.script     = m_headerInfo->toPlainText();

    preset.header.rowHeight = m_overrideHeaderHeight->isChecked() ? m_headerRowHeight->value() : 0;
    preset.header.simple    = m_simpleHeader->isChecked();
    preset.header.showCover = m_showCover->isEnabled() && m_showCover->isChecked();

    updateGroupTextBlocks(m_subHeaders->blocks(), preset.subHeaders);

    preset.track.leftText.script  = m_trackLeftText->toPlainText();
    preset.track.rightText.script = m_trackRightText->toPlainText();
    preset.track.rowHeight        = m_overrideTrackHeight->isChecked() ? m_trackRowHeight->value() : 0;

    m_presetRegistry->changeItem(preset);
}

void PlaylistPresetsPageWidget::clonePreset()
{
    const int presetId   = m_presetBox->currentData().toInt();
    const auto regPreset = m_presetRegistry->itemById(presetId);
    if(!regPreset) {
        return;
    }

    PlaylistPreset clonedPreset{regPreset.value()};
    clonedPreset.name                = tr("Copy of %1").arg(clonedPreset.name);
    clonedPreset.isDefault           = false;
    const PlaylistPreset addedPreset = m_presetRegistry->addItem(clonedPreset);
    if(addedPreset.isValid()) {
        m_presetBox->addItem(addedPreset.name, addedPreset.id);
        m_presetBox->setCurrentIndex(m_presetBox->count() - 1);
    }
}

void PlaylistPresetsPageWidget::selectionChanged()
{
    const int presetId   = m_presetBox->currentData().toInt();
    const auto regPreset = m_presetRegistry->itemById(presetId);
    if(!regPreset) {
        return;
    }

    clearBlocks();
    setupPreset(regPreset.value());
}

void PlaylistPresetsPageWidget::setupPreset(const PlaylistPreset& preset)
{
    m_renamePreset->setDisabled(preset.isDefault);
    m_deletePreset->setDisabled(preset.isDefault);
    m_updatePreset->setDisabled(preset.isDefault);

    m_headerTitle->setReadOnly(preset.isDefault);
    m_headerSubtitle->setReadOnly(preset.isDefault);
    m_headerSideText->setReadOnly(preset.isDefault);
    m_headerInfo->setReadOnly(preset.isDefault);

    m_headerTitle->setPlainText(preset.header.title.script);
    m_headerSubtitle->setPlainText(preset.header.subtitle.script);
    m_headerSideText->setPlainText(preset.header.sideText.script);
    m_headerInfo->setPlainText(preset.header.info.script);

    m_simpleHeader->setChecked(preset.header.simple);
    m_simpleHeader->setDisabled(preset.isDefault);
    m_showCover->setChecked(preset.header.showCover);
    m_showCover->setDisabled(preset.isDefault || preset.header.simple);

    m_overrideHeaderHeight->setChecked(preset.header.rowHeight > 0);
    m_overrideHeaderHeight->setEnabled(!preset.isDefault);
    m_headerRowHeight->setValue(preset.header.rowHeight);
    m_headerRowHeight->setReadOnly(preset.isDefault);
    m_headerRowHeight->setEnabled(m_overrideHeaderHeight->isChecked());

    m_subHeaders->setReadOnly(preset.isDefault);

    for(const auto& subheader : preset.subHeaders) {
        createGroupPresetInputs(subheader, m_subHeaders, this);
    }

    m_headerSubtitle->setDisabled(preset.header.simple);
    m_headerInfo->setDisabled(preset.header.simple);

    m_trackLeftText->setReadOnly(preset.isDefault);
    m_trackRightText->setReadOnly(preset.isDefault);

    m_trackLeftText->setPlainText(preset.track.leftText.script);
    m_trackRightText->setPlainText(preset.track.rightText.script);

    m_overrideTrackHeight->setChecked(preset.track.rowHeight > 0);
    m_overrideTrackHeight->setEnabled(!preset.isDefault);
    m_trackRowHeight->setValue(preset.track.rowHeight);
    m_trackRowHeight->setReadOnly(preset.isDefault);
    m_trackRowHeight->setEnabled(m_overrideTrackHeight->isChecked());
}

void PlaylistPresetsPageWidget::clearBlocks()
{
    m_subHeaders->clearBlocks();
}

PlaylistPresetsPage::PlaylistPresetsPage(PresetRegistry* presetRegistry, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::PlaylistPresets);
    setName(tr("Presets"));
    setCategory({tr("Playlist"), tr("Presets")});
    setWidgetCreator([presetRegistry] { return new PlaylistPresetsPageWidget(presetRegistry); });
}
} // namespace Fooyin

#include "moc_playlistpresetspage.cpp"
#include "playlistpresetspage.moc"
