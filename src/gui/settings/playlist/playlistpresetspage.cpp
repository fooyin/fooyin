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

#include "playlistpresetspage.h"

#include "internalguisettings.h"
#include "playlist/playlistpreset.h"
#include "playlist/presetregistry.h"

#include <gui/guiconstants.h>
#include <gui/widgets/expandableinputbox.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
class ExpandableGroupBox : public ExpandableInput
{
    Q_OBJECT

public:
    explicit ExpandableGroupBox(int rowHeight, QWidget* parent = nullptr)
        : ExpandableInput{CustomWidget, parent}
        , m_groupBox{new QGroupBox(this)}
        , m_overrideHeight{new QCheckBox(tr("Override height") + u":"_s, this)}
        , m_rowHeight{new QSpinBox(this)}
        , m_grouping{new ScriptTextEdit(this)}
        , m_script{new ScriptTextEdit(this)}
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_groupBox);

        m_overrideHeight->setChecked(rowHeight > 0);
        m_rowHeight->setValue(rowHeight);
        m_rowHeight->setEnabled(m_overrideHeight->isChecked());

        auto* alignmentHelp = new QLabel(u"🛈 "_s
                                             + tr("Use <code>&lt;right&gt;</code> for right-aligned text and "
                                                  "<code>&lt;hr/&gt;</code> to draw a separator."),
                                         this);
        alignmentHelp->setWordWrap(true);

        m_grouping->setPlaceholderText(tr("Leave empty to group by the display script"));

        m_rowHeight->setMinimum(20);
        m_rowHeight->setMaximum(150);

        auto* scriptLayout = new QGridLayout();
        scriptLayout->setContentsMargins({});

        int row{0};
        scriptLayout->addWidget(new QLabel(tr("Grouping script") + u":"_s, this), row++, 0);
        scriptLayout->addWidget(m_grouping, row++, 0);
        scriptLayout->addWidget(new QLabel(tr("Display script") + u":"_s, this), row++, 0);
        scriptLayout->addWidget(m_script, row++, 0);
        scriptLayout->addWidget(alignmentHelp, row++, 0);
        scriptLayout->setColumnStretch(0, 1);

        auto* groupLayout = new QGridLayout(m_groupBox);

        row = 0;
        groupLayout->addWidget(m_overrideHeight, row, 0);
        groupLayout->addWidget(m_rowHeight, row++, 1);
        groupLayout->addLayout(scriptLayout, row++, 0, 1, 3);

        groupLayout->setColumnStretch(2, 1);

        QObject::connect(m_overrideHeight, &QCheckBox::toggled, this,
                         [this](bool checked) { m_rowHeight->setEnabled(checked); });
    }

    void setScript(const QString& script)
    {
        m_script->setText(script);
    }

    void setGrouping(const QString& script)
    {
        m_grouping->setText(script);
    }

    [[nodiscard]] QString script() const
    {
        return m_script->text();
    }

    [[nodiscard]] QString grouping() const
    {
        return m_grouping->text();
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
        m_grouping->setReadOnly(readOnly);
        m_script->setReadOnly(readOnly);
    }

private:
    QGroupBox* m_groupBox;
    QCheckBox* m_overrideHeight;
    QSpinBox* m_rowHeight;
    ScriptTextEdit* m_grouping;
    ScriptTextEdit* m_script;
};

void createGroupPresetInputs(const SubheaderRow& subheader, ExpandableInputBox* box, QWidget* parent)
{
    if(!subheader.isValid()) {
        return;
    }

    auto* input = new ExpandableGroupBox(subheader.rowHeight, parent);
    box->addInput(input);

    input->setGrouping(subheader.grouping);
    input->setScript(subheader.text.script);
}

void updateGroupTextBlocks(const ExpandableInputList& presetInputs, SubheaderRows& textBlocks)
{
    textBlocks.clear();

    for(const auto& input : presetInputs) {
        if(auto* presetInput = qobject_cast<ExpandableGroupBox*>(input)) {
            SubheaderRow block;

            block.grouping    = presetInput->grouping();
            block.text.script = presetInput->script();
            block.rowHeight   = presetInput->rowHeight();

            textBlocks.emplace_back(block);
        }
    }
}

class PlaylistPresetsPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit PlaylistPresetsPageWidget(PresetRegistry* presetRegistry, SettingsManager* settings);

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
    SettingsManager* m_settings;

    QComboBox* m_presetBox;
    QTabWidget* m_presetTabs;

    ScriptTextEdit* m_headerText;
    ScriptTextEdit* m_headerGrouping;
    QCheckBox* m_overrideHeaderHeight;
    QSpinBox* m_headerRowHeight;
    QSpinBox* m_headerArtworkPadding;
    QSpinBox* m_headerArtworkPaddingVertical;

    ExpandableInputBox* m_subHeaders;
    QCheckBox* m_alignSubheadersToImageColumns;
    QCheckBox* m_showCoverBelowEverySubheader;

    ScriptTextEdit* m_trackText;
    QCheckBox* m_overrideTrackHeight;
    QSpinBox* m_trackRowHeight;

    QCheckBox* m_showCover;

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
    , m_headerText{new ScriptTextEdit(this)}
    , m_headerGrouping{new ScriptTextEdit(this)}
    , m_overrideHeaderHeight{new QCheckBox(tr("Override height") + u":"_s, this)}
    , m_headerRowHeight{new QSpinBox(this)}
    , m_headerArtworkPadding{new QSpinBox(this)}
    , m_headerArtworkPaddingVertical{new QSpinBox(this)}
    , m_alignSubheadersToImageColumns{new QCheckBox(tr("Align subheaders to edge of image columns"), this)}
    , m_showCoverBelowEverySubheader{new QCheckBox(tr("Display covers below every subheader"), this)}
    , m_trackText{new ScriptTextEdit(this)}
    , m_overrideTrackHeight{new QCheckBox(tr("Override height") + u":"_s, this)}
    , m_trackRowHeight{new QSpinBox(this)}
    , m_showCover{new QCheckBox(tr("Show cover"), this)}
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
    mainLayout->addWidget(m_presetTabs, 2, 0, 1, 5);
    mainLayout->setRowStretch(2, 1);

    m_headerRowHeight->setMinimum(50);
    m_headerRowHeight->setMaximum(300);

    m_headerGrouping->setPlaceholderText(tr("Leave empty to group by the display script"));

    auto* scriptLayout = new QGridLayout();
    scriptLayout->setContentsMargins({});

    int row{0};
    scriptLayout->addWidget(new QLabel(tr("Grouping script") + u":"_s, this), row++, 0, 1, 2);
    scriptLayout->addWidget(m_headerGrouping, row++, 0, 1, 2);
    scriptLayout->addWidget(new QLabel(tr("Display script") + u":"_s, this), row++, 0, 1, 2);
    scriptLayout->addWidget(m_headerText, row++, 0, 1, 2);
    auto* headerHelp = new QLabel(u"🛈 "_s
                                      + tr("Use <code>&lt;right&gt;</code> for right-aligned text and "
                                           "<code>&lt;hr/&gt;</code> to draw a separator."),
                                  this);
    headerHelp->setWordWrap(true);
    scriptLayout->addWidget(headerHelp, row++, 0, 1, 2);
    scriptLayout->setRowStretch(3, 1);
    scriptLayout->setColumnStretch(0, 1);
    scriptLayout->setColumnStretch(1, 1);

    auto* headerWidget = new QWidget();
    auto* headerLayout = new QGridLayout(headerWidget);

    m_headerArtworkPadding->setRange(0, 100);
    m_headerArtworkPaddingVertical->setRange(0, 100);
    m_headerArtworkPadding->setSuffix(u" px"_s);
    m_headerArtworkPaddingVertical->setSuffix(u" px"_s);

    auto* artworkGroup  = new QGroupBox(tr("Artwork"), this);
    auto* artworkLayout = new QGridLayout(artworkGroup);
    artworkLayout->addWidget(m_showCover, 0, 0, 1, 2);
    artworkLayout->addWidget(new QLabel(tr("Left/Right") + u":"_s, this), 1, 0);
    artworkLayout->addWidget(m_headerArtworkPadding, 1, 1);
    artworkLayout->addWidget(new QLabel(tr("Top/Bottom") + u":"_s, this), 2, 0);
    artworkLayout->addWidget(m_headerArtworkPaddingVertical, 2, 1);
    artworkLayout->setColumnStretch(2, 1);

    row = 0;
    headerLayout->addWidget(artworkGroup, row++, 0, 1, 5);
    headerLayout->addWidget(m_overrideHeaderHeight, row, 0);
    headerLayout->addWidget(m_headerRowHeight, row++, 1);
    headerLayout->addLayout(scriptLayout, row++, 0, 1, 5);

    headerLayout->setColumnStretch(4, 1);
    headerLayout->setRowStretch(row - 1, 1);

    m_presetTabs->addTab(headerWidget, tr("Header"));

    auto* subheaderWidget = new QWidget();
    auto* subheaderLayout = new QGridLayout(subheaderWidget);

    m_subHeaders = new ExpandableInputBox(tr("Subheaders") + u":"_s, ExpandableInput::CustomWidget, this);
    m_subHeaders->setInputWidget([](QWidget* parent) {
        const SubheaderRow subheader;
        auto* groupBox = new ExpandableGroupBox(subheader.rowHeight, parent);
        return groupBox;
    });

    subheaderLayout->addWidget(m_alignSubheadersToImageColumns, 0, 0, 1, 3);
    subheaderLayout->addWidget(m_showCoverBelowEverySubheader, 1, 0, 1, 3);
    subheaderLayout->addWidget(m_subHeaders, 2, 0, 1, 3);
    subheaderLayout->setRowStretch(2, 1);

    m_presetTabs->addTab(subheaderWidget, tr("Subheaders"));

    auto* tracksWidget = new QWidget();
    auto* trackLayout  = new QGridLayout(tracksWidget);

    m_trackRowHeight->setMinimum(20);
    m_trackRowHeight->setMaximum(150);

    auto* alignmentHelp = new QLabel(u"🛈 "_s + tr("Use <code>&lt;right&gt;</code> for right-aligned text."), this);
    alignmentHelp->setWordWrap(true);

    row = 0;
    trackLayout->addWidget(m_overrideTrackHeight, row, 0);
    trackLayout->addWidget(m_trackRowHeight, row++, 1);
    trackLayout->addWidget(new QLabel(tr("Display script") + u":"_s, this), row++, 0);
    trackLayout->addWidget(m_trackText, row++, 0, 1, 3);
    trackLayout->addWidget(alignmentHelp, row++, 0, 1, 3);

    trackLayout->setColumnStretch(2, 1);
    trackLayout->setRowStretch(2, 1);

    m_presetTabs->addTab(tracksWidget, tr("Tracks"));

    QObject::connect(m_presetBox, &QComboBox::currentIndexChanged, this, &PlaylistPresetsPageWidget::selectionChanged);

    QObject::connect(m_newPreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::newPreset);
    QObject::connect(m_renamePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::renamePreset);
    QObject::connect(m_deletePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::deletePreset);
    QObject::connect(m_updatePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::updatePreset);
    QObject::connect(m_clonePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::clonePreset);

    QObject::connect(m_showCover, &QCheckBox::toggled, m_headerArtworkPadding, &QSpinBox::setEnabled);
    QObject::connect(m_showCover, &QCheckBox::toggled, m_headerArtworkPaddingVertical, &QSpinBox::setEnabled);
    QObject::connect(m_overrideHeaderHeight, &QCheckBox::toggled, m_headerRowHeight, &QSpinBox::setEnabled);
    QObject::connect(m_overrideTrackHeight, &QCheckBox::toggled, m_trackRowHeight, &QSpinBox::setEnabled);
}

void PlaylistPresetsPageWidget::load()
{
    const QSignalBlocker blocker{m_presetBox};

    m_presetBox->clear();

    const auto presets = m_presetRegistry->items();
    for(const auto& preset : presets) {
        m_presetBox->insertItem(preset.index, preset.name, preset.id);
    }

    const int currentPresetId = m_settings->fileValue(Settings::Gui::Internal::PlaylistCurrentPreset).toInt();
    const int currentIndex    = m_presetBox->findData(currentPresetId);
    if(currentIndex >= 0) {
        m_presetBox->setCurrentIndex(currentIndex);
    }

    selectionChanged();
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
    const QString text = QInputDialog::getText(this, tr("Add Preset"), tr("Preset Name") + u":"_s, QLineEdit::Normal,
                                               preset.name, &success);

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
        const QString text = QInputDialog::getText(this, tr("Rename Preset"), tr("Preset Name") + u":"_s,
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

    preset.header.grouping    = m_headerGrouping->text();
    preset.header.text.script = m_headerText->text();

    preset.header.rowHeight              = m_overrideHeaderHeight->isChecked() ? m_headerRowHeight->value() : 0;
    preset.header.showCover              = m_showCover->isChecked();
    preset.header.artworkPadding         = m_headerArtworkPadding->value();
    preset.header.artworkPaddingVertical = m_headerArtworkPaddingVertical->value();

    updateGroupTextBlocks(m_subHeaders->blocks(), preset.subHeaders);
    preset.insetSubheadersToImageColumns = m_alignSubheadersToImageColumns->isChecked();
    preset.showCoverBelowEverySubheader  = m_showCoverBelowEverySubheader->isChecked();

    preset.track.text.script = m_trackText->text();
    preset.track.rowHeight   = m_overrideTrackHeight->isChecked() ? m_trackRowHeight->value() : 0;

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
    //: %1 refers to the name of a playlist preset.
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
    m_deletePreset->setDisabled(preset.isDefault);

    m_headerGrouping->setText(preset.header.grouping);
    m_headerText->setText(preset.header.text.script);

    m_showCover->setChecked(preset.header.showCover);
    m_headerArtworkPadding->setValue(preset.header.artworkPadding);
    m_headerArtworkPaddingVertical->setValue(preset.header.artworkPaddingVertical);
    m_headerArtworkPadding->setEnabled(preset.header.showCover);
    m_headerArtworkPaddingVertical->setEnabled(preset.header.showCover);

    m_overrideHeaderHeight->setChecked(preset.header.rowHeight > 0);
    m_headerRowHeight->setValue(preset.header.rowHeight);
    m_headerRowHeight->setEnabled(m_overrideHeaderHeight->isChecked());

    m_alignSubheadersToImageColumns->setChecked(preset.insetSubheadersToImageColumns);
    m_showCoverBelowEverySubheader->setChecked(preset.showCoverBelowEverySubheader);

    for(const auto& subheader : preset.subHeaders) {
        createGroupPresetInputs(subheader, m_subHeaders, this);
    }

    m_trackText->setText(preset.track.text.script);

    m_overrideTrackHeight->setChecked(preset.track.rowHeight > 0);
    m_trackRowHeight->setValue(preset.track.rowHeight);
    m_trackRowHeight->setEnabled(m_overrideTrackHeight->isChecked());
}

void PlaylistPresetsPageWidget::clearBlocks()
{
    m_subHeaders->clearBlocks();
}
} // namespace

PlaylistPresetsPage::PlaylistPresetsPage(PresetRegistry* presetRegistry, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::PlaylistPresets);
    setName(tr("Presets"));
    setCategory({tr("Playlist"), tr("Presets")});
    setWidgetCreator([presetRegistry, settings] { return new PlaylistPresetsPageWidget(presetRegistry, settings); });
}
} // namespace Fooyin

#include "moc_playlistpresetspage.cpp"
#include "playlistpresetspage.moc"
