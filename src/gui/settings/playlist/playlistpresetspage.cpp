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
        , m_rowHeight{new QSpinBox(this)}
        , m_leftScript{new QTextEdit(tr("Left-aligned text") + QStringLiteral(":"), this)}
        , m_rightScript{new QTextEdit(tr("Right-aligned text") + QStringLiteral(":"), this)}
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_groupBox);

        auto* groupLayout = new QGridLayout(m_groupBox);

        m_rowHeight->setValue(rowHeight);

        auto* rowHeightLabel = new QLabel(tr("Row height") + QStringLiteral(":"), this);

        groupLayout->addWidget(rowHeightLabel, 0, 0);
        groupLayout->addWidget(m_rowHeight, 0, 1);
        groupLayout->addWidget(m_leftScript, 1, 0, 1, 3);
        groupLayout->addWidget(m_rightScript, 2, 0, 1, 3);

        groupLayout->setColumnStretch(2, 1);
    }

    void setLeftScript(const QString& script)
    {
        m_leftScript->setPlainText(script);
    }

    void setRightScript(const QString& script)
    {
        m_rightScript->setPlainText(script);
    }

    QString leftScript() const
    {
        return m_leftScript->toPlainText();
    }

    QString rightScript() const
    {
        return m_rightScript->toPlainText();
    }

    int rowHeight() const
    {
        return m_rowHeight->value();
    }

    void setReadOnly(bool readOnly) override
    {
        ExpandableInput::setReadOnly(readOnly);

        m_rowHeight->setReadOnly(readOnly);
        m_leftScript->setReadOnly(readOnly);
        m_rightScript->setReadOnly(readOnly);
    }

private:
    QGroupBox* m_groupBox;
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
    explicit PlaylistPresetsPageWidget(SettingsManager* settings);

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

    SettingsManager* m_settings;

    PresetRegistry m_presetRegistry;

    QComboBox* m_presetBox;
    QTabWidget* m_presetTabs;

    QTextEdit* m_headerTitle;
    QTextEdit* m_headerSubtitle;
    QTextEdit* m_headerSideText;
    QTextEdit* m_headerInfo;
    QSpinBox* m_headerRowHeight;

    ExpandableInputBox* m_subHeaders;

    QTextEdit* m_trackLeftText;
    QTextEdit* m_trackRightText;
    QSpinBox* m_trackRowHeight;

    QCheckBox* m_showCover;
    QCheckBox* m_simpleHeader;

    QPushButton* m_newPreset;
    QPushButton* m_renamePreset;
    QPushButton* m_deletePreset;
    QPushButton* m_updatePreset;
    QPushButton* m_clonePreset;
};

PlaylistPresetsPageWidget::PlaylistPresetsPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_presetRegistry{settings}
    , m_presetBox{new QComboBox(this)}
    , m_presetTabs{new QTabWidget(this)}
    , m_headerTitle{new QTextEdit(tr("Title") + QStringLiteral(":"), this)}
    , m_headerSubtitle{new QTextEdit(tr("Subtitle") + QStringLiteral(":"), this)}
    , m_headerSideText{new QTextEdit(tr("Side text") + QStringLiteral(":"), this)}
    , m_headerInfo{new QTextEdit(tr("Info") + QStringLiteral(":"), this)}
    , m_headerRowHeight{new QSpinBox(this)}
    , m_trackLeftText{new QTextEdit(tr("Left-aligned text") + QStringLiteral(":"), this)}
    , m_trackRightText{new QTextEdit(tr("Right-aligned text") + QStringLiteral(":"), this)}
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

    auto* headerRowHeight = new QLabel(tr("Row height") + QStringLiteral(":"), this);

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

    auto* trackRowHeight = new QLabel(tr("Row height") + QStringLiteral(":"), this);

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
}

void PlaylistPresetsPageWidget::load()
{
    m_presetBox->clear();

    const int currentPreset = m_settings->value<Settings::Gui::Internal::PlaylistCurrentPreset>();
    const auto presets      = m_presetRegistry.items();

    for(const auto& preset : presets) {
        m_presetBox->insertItem(preset.index, preset.name, preset.id);
        if(preset.id == currentPreset) {
            m_presetBox->setCurrentIndex(preset.index);
        }
    }
}

void PlaylistPresetsPageWidget::apply()
{
    updatePreset();
}

void PlaylistPresetsPageWidget::reset()
{
    m_presetRegistry.reset();
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
        const PlaylistPreset addedPreset = m_presetRegistry.addItem(preset);
        if(addedPreset.isValid()) {
            m_presetBox->addItem(addedPreset.name, addedPreset.id);
            m_presetBox->setCurrentIndex(m_presetBox->count() - 1);
        }
    }
}

void PlaylistPresetsPageWidget::renamePreset()
{
    const int presetId = m_presetBox->currentData().toInt();

    auto preset = m_presetRegistry.itemById(presetId);

    bool success{false};
    const QString text = QInputDialog::getText(this, tr("Rename Preset"), tr("Preset Name") + QStringLiteral(":"),
                                               QLineEdit::Normal, preset.name, &success);

    if(success && !text.isEmpty()) {
        preset.name = text;
        if(m_presetRegistry.changeItem(preset)) {
            m_presetBox->setItemText(m_presetBox->currentIndex(), preset.name);
        }
    }
}

void PlaylistPresetsPageWidget::deletePreset()
{
    if(m_presetBox->count() <= 1) {
        return;
    }

    const int presetId = m_presetBox->currentData().toInt();

    if(m_presetRegistry.removeById(presetId)) {
        m_presetBox->removeItem(m_presetBox->currentIndex());
    }
}

void PlaylistPresetsPageWidget::updatePreset()
{
    const int presetId = m_presetBox->currentData().toInt();
    auto preset        = m_presetRegistry.itemById(presetId);

    if(preset.isDefault) {
        return;
    }

    preset.header.title.script    = m_headerTitle->toPlainText();
    preset.header.subtitle.script = m_headerSubtitle->toPlainText();
    preset.header.sideText.script = m_headerSideText->toPlainText();
    preset.header.info.script     = m_headerInfo->toPlainText();

    preset.header.rowHeight = m_headerRowHeight->value();
    preset.header.simple    = m_simpleHeader->isChecked();
    preset.header.showCover = m_showCover->isEnabled() && m_showCover->isChecked();

    updateGroupTextBlocks(m_subHeaders->blocks(), preset.subHeaders);

    preset.track.leftText.script  = m_trackLeftText->toPlainText();
    preset.track.rightText.script = m_trackRightText->toPlainText();
    preset.track.rowHeight        = m_trackRowHeight->value();

    m_presetRegistry.changeItem(preset);
}

void PlaylistPresetsPageWidget::clonePreset()
{
    const int presetId = m_presetBox->currentData().toInt();
    auto preset        = m_presetRegistry.itemById(presetId);

    PlaylistPreset clonedPreset{preset};
    clonedPreset.name                = tr("Copy of %1").arg(preset.name);
    clonedPreset.isDefault           = false;
    const PlaylistPreset addedPreset = m_presetRegistry.addItem(clonedPreset);
    if(addedPreset.isValid()) {
        m_presetBox->addItem(addedPreset.name, addedPreset.id);
        m_presetBox->setCurrentIndex(m_presetBox->count() - 1);
    }
}

void PlaylistPresetsPageWidget::selectionChanged()
{
    const int presetId = m_presetBox->currentData().toInt();
    auto preset        = m_presetRegistry.itemById(presetId);

    if(!preset.isValid()) {
        return;
    }

    clearBlocks();
    setupPreset(preset);
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

    m_headerRowHeight->setValue(preset.header.rowHeight);
    m_headerRowHeight->setReadOnly(preset.isDefault);

    m_simpleHeader->setChecked(preset.header.simple);
    m_showCover->setChecked(preset.header.showCover);
    m_showCover->setEnabled(!preset.header.simple);

    m_subHeaders->setReadOnly(preset.isDefault);

    for(const auto& subheader : preset.subHeaders) {
        createGroupPresetInputs(subheader, m_subHeaders, this);
    }

    m_headerSubtitle->setEnabled(!preset.header.simple);
    m_headerInfo->setEnabled(!preset.header.simple);

    m_trackLeftText->setReadOnly(preset.isDefault);
    m_trackRightText->setReadOnly(preset.isDefault);

    m_trackLeftText->setPlainText(preset.track.leftText.script);
    m_trackRightText->setPlainText(preset.track.rightText.script);
    m_trackRowHeight->setValue(preset.track.rowHeight);
    m_trackRowHeight->setReadOnly(preset.isDefault);
}

void PlaylistPresetsPageWidget::clearBlocks()
{
    m_subHeaders->clearBlocks();
}

PlaylistPresetsPage::PlaylistPresetsPage(SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::PlaylistPresets);
    setName(tr("Presets"));
    setCategory({tr("Playlist"), tr("Presets")});
    setWidgetCreator([settings] { return new PlaylistPresetsPageWidget(settings); });
}
} // namespace Fooyin

#include "moc_playlistpresetspage.cpp"
#include "playlistpresetspage.moc"
