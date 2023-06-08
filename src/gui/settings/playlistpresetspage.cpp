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
#include "gui/playlist/presetfwd.h"
#include "gui/playlist/presetregistry.h"

#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>
#include <utils/tablemodel.h>
#include <utils/treeitem.h>

#include <QCheckBox>
#include <QFontDialog>
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

enum ItemStatus
{
    None    = 0,
    Added   = 1,
    Removed = 2,
    Changed = 3
};

class PresetItem : public Utils::TreeItem<PresetItem>
{
public:
    explicit PresetItem(PlaylistPreset preset = {}, PresetItem* parent = nullptr);

    [[nodiscard]] ItemStatus status() const;
    void setStatus(ItemStatus status);

    [[nodiscard]] PlaylistPreset preset() const;
    void changePreset(const PlaylistPreset& preset);

private:
    PlaylistPreset m_preset;
    ItemStatus m_status{None};
};

PresetItem::PresetItem(PlaylistPreset preset, PresetItem* parent)
    : TreeItem{parent}
    , m_preset{std::move(preset)}
{ }

ItemStatus PresetItem::status() const
{
    return m_status;
}

void PresetItem::setStatus(ItemStatus status)
{
    m_status = status;
}

PlaylistPreset PresetItem::preset() const
{
    return m_preset;
}

void PresetItem::changePreset(const PlaylistPreset& preset)
{
    m_preset = preset;
}

class PresetModel : public Utils::TableModel<PresetItem>
{
public:
    explicit PresetModel(PresetRegistry* presetRegistry, QObject* parent = nullptr);

    void setupModelData();
    void addNewPreset();
    void markForRemoval(const PlaylistPreset& preset);
    void markForChange(const PlaylistPreset& preset);
    void processQueue();

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

private:
    using PresetItemList = std::vector<std::unique_ptr<PresetItem>>;

    void removePreset(int index);

    PresetRegistry* m_presetRegistry;

    PresetItemList m_nodes;
};

PresetModel::PresetModel(PresetRegistry* presetRegistry, QObject* parent)
    : TableModel{parent}
    , m_presetRegistry{presetRegistry}
{
    setupModelData();
}

void PresetModel::setupModelData()
{
    const auto& presets = m_presetRegistry->presets();

    for(const auto& [index, preset] : presets) {
        if(preset.name.isEmpty()) {
            continue;
        }
        PresetItem* child = m_nodes.emplace_back(std::make_unique<PresetItem>(preset, rootItem())).get();
        rootItem()->appendChild(child);
    }
}

void PresetModel::addNewPreset()
{
    const int index = static_cast<int>(m_nodes.size());

    PlaylistPreset preset;
    preset.index = index;

    auto* item = m_nodes.emplace_back(std::make_unique<PresetItem>(preset, rootItem())).get();

    item->setStatus(Added);

    const int row = rootItem()->childCount();
    beginInsertRows({}, row, row);
    rootItem()->appendChild(item);
    endInsertRows();
}

void PresetModel::markForRemoval(const PlaylistPreset& preset)
{
    PresetItem* item = m_nodes.at(preset.index).get();

    if(item->status() == Added) {
        beginRemoveRows({}, item->row(), item->row());
        rootItem()->removeChild(item->row());
        endRemoveRows();

        removePreset(preset.index);
    }
    else {
        item->setStatus(Removed);
        const QModelIndex index = createIndex(item->row(), 1, item);
        emit dataChanged(index, index, {Qt::FontRole});
    }
}

void PresetModel::markForChange(const PlaylistPreset& preset)
{
    PresetItem* item = m_nodes.at(preset.index).get();
    item->changePreset(preset);
    const QModelIndex index = createIndex(item->row(), 1, item);
    emit dataChanged(index, index, {Qt::DisplayRole});

    if(item->status() == None) {
        item->setStatus(Changed);
    }
}

void PresetModel::processQueue()
{
    for(auto& node : m_nodes) {
        PresetItem* item            = node.get();
        const ItemStatus status     = item->status();
        const PlaylistPreset preset = item->preset();

        switch(status) {
            case(Added): {
                const PlaylistPreset addedPreset = m_presetRegistry->addPreset(preset);
                if(addedPreset.isValid()) {
                    item->changePreset(addedPreset);
                    item->setStatus(None);
                }
                else {
                    qWarning() << QString{"Field %1 could not be added"}.arg(preset.name);
                }
                break;
            }
            case(Removed): {
                if(m_presetRegistry->removeByIndex(preset.index)) {
                    beginRemoveRows({}, item->row(), item->row());
                    rootItem()->removeChild(item->row());
                    endRemoveRows();
                    removePreset(preset.index);
                }
                else {
                    qWarning() << QString{"Field (%1) could not be removed"}.arg(preset.name);
                }
                break;
            }
            case(Changed): {
                if(m_presetRegistry->changePreset(preset)) {
                    item->setStatus(None);
                }
                else {
                    qWarning() << QString{"Field (%1) could not be changed"}.arg(preset.name);
                }
                break;
            }
            case(None):
                break;
        }
    }
}

Qt::ItemFlags PresetModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = TableModel::flags(index);
    flags |= Qt::ItemIsEditable;

    return flags;
}

QVariant PresetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return "Preset Index";
        case(1):
            return "Preset Name";
    }
    return {};
}

QVariant PresetModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::FontRole) {
        return {};
    }

    if(!index.isValid()) {
        return {};
    }

    auto* item = static_cast<PresetItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        QFont font;
        switch(item->status()) {
            case(Added):
                font.setItalic(true);
                break;
            case(Removed):
                font.setStrikeOut(true);
                break;
            case(Changed):
                font.setBold(true);
                break;
            case(None):
                break;
        }
        return font;
    }

    switch(index.column()) {
        case(0):
            return item->preset().index;
        case(1): {
            const QString& name = item->preset().name;
            return !name.isEmpty() ? name : "<enter name here>";
        }
    }
    return {};
}

bool PresetModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole) {
        return false;
    }

    auto* item            = static_cast<PresetItem*>(index.internalPointer());
    const int col         = index.column();
    PlaylistPreset preset = item->preset();

    switch(col) {
        case(1): {
            if(preset.name == value.toString()) {
                return false;
            }
            preset.name = value.toString();
            break;
        }
        case(0):
            break;
    }

    markForChange(preset);

    return true;
}

int PresetModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 2;
}

void PresetModel::removePreset(int index)
{
    if(index < 0 || index >= static_cast<int>(m_nodes.size())) {
        return;
    }
    m_nodes.erase(m_nodes.begin() + index);
}

class PlaylistPresetsPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit PlaylistPresetsPageWidget(Widgets::Playlist::PresetRegistry* presetRegistry);

    void apply() override;

    void newPreset();
    void deletePreset();
    void updatePreset();

    void showFontDialog(QFont& initialFont);

    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void setupPreset(const PlaylistPreset& preset);

private:
    QTableView* m_presetList;
    PresetModel* m_model;

    QLineEdit* m_headerTitle;
    QPushButton* m_headerTitleFontButton;
    QFont m_headerTitleFont;
    QLineEdit* m_headerSubtitle;
    QPushButton* m_headerSubtitleFontButton;
    QFont m_headerSubtitleFont;
    QLineEdit* m_headerSideText;
    QPushButton* m_headerSideTextFontButton;
    QFont m_headerSideTextFont;
    QPushButton* m_headerInfoFontButton;
    QFont m_headerInfoFont;
    QSpinBox* m_headerRowHeight;

    QLineEdit* m_subHeaderTitle;
    QPushButton* m_subHeaderTitleFontButton;
    QFont m_subHeaderTitleFont;
    QPushButton* m_subHeaderRightFontButton;
    QFont m_subHeaderRightFont;
    QSpinBox* m_subHeaderRowHeight;

    QLineEdit* m_trackLeft;
    QPushButton* m_trackLeftFontButton;
    QFont m_trackLeftFont;
    QLineEdit* m_trackRight;
    QPushButton* m_trackRightFontButton;
    QFont m_trackRightFont;
    QSpinBox* m_trackRowHeight;

    QCheckBox* m_showCover;
    QCheckBox* m_simpleHeader;

    QPushButton* m_newPreset;
    QPushButton* m_deletePreset;
    QPushButton* m_updatePreset;
};

PlaylistPresetsPageWidget::PlaylistPresetsPageWidget(Widgets::Playlist::PresetRegistry* presetRegistry)
    : m_presetList{new QTableView(this)}
    , m_model{new PresetModel(presetRegistry, this)}
    , m_headerTitle{new QLineEdit(this)}
    , m_headerTitleFontButton{new QPushButton(QIcon::fromTheme(Constants::Icons::Font), "", this)}
    , m_headerSubtitle{new QLineEdit(this)}
    , m_headerSubtitleFontButton{new QPushButton(QIcon::fromTheme(Constants::Icons::Font), "", this)}
    , m_headerSideText{new QLineEdit(this)}
    , m_headerSideTextFontButton{new QPushButton(QIcon::fromTheme(Constants::Icons::Font), "", this)}
    , m_headerInfoFontButton{new QPushButton(QIcon::fromTheme(Constants::Icons::Font), "", this)}
    , m_headerRowHeight{new QSpinBox(this)}
    , m_subHeaderTitle{new QLineEdit(this)}
    , m_subHeaderTitleFontButton{new QPushButton(QIcon::fromTheme(Constants::Icons::Font), "", this)}
    , m_subHeaderRightFontButton{new QPushButton(QIcon::fromTheme(Constants::Icons::Font), "", this)}
    , m_subHeaderRowHeight{new QSpinBox(this)}
    , m_trackLeft{new QLineEdit(this)}
    , m_trackLeftFontButton{new QPushButton(QIcon::fromTheme(Constants::Icons::Font), "", this)}
    , m_trackRight{new QLineEdit(this)}
    , m_trackRightFontButton{new QPushButton(QIcon::fromTheme(Constants::Icons::Font), "", this)}
    , m_trackRowHeight{new QSpinBox(this)}
    , m_showCover{new QCheckBox(tr("Show Cover"), this)}
    , m_simpleHeader{new QCheckBox(tr("Simple Header"), this)}
    , m_newPreset{new QPushButton(tr("New"), this)}
    , m_deletePreset{new QPushButton(tr("Delete"), this)}
    , m_updatePreset{new QPushButton(tr("Update"), this)}
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
    mainLayout->addWidget(m_deletePreset, 1, 1, 1, 1, Qt::AlignTop);
    mainLayout->addWidget(m_updatePreset, 2, 0, 1, 1, Qt::AlignTop);

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

    m_headerTitleFontButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_headerSubtitleFontButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_headerSideTextFontButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_headerInfoFontButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    headerLayout->addWidget(headerRowHeight, 0, 0);
    headerLayout->addWidget(m_headerRowHeight, 1, 0);
    headerLayout->addWidget(m_simpleHeader, 0, 1);
    headerLayout->addWidget(m_showCover, 1, 1);
    headerLayout->addWidget(headerTitle, 2, 0);
    headerLayout->addWidget(m_headerTitle, 3, 0, 1, 2);
    headerLayout->addWidget(m_headerTitleFontButton, 3, 3);
    headerLayout->addWidget(headerSubtitle, 4, 0);
    headerLayout->addWidget(m_headerSubtitle, 5, 0, 1, 2);
    headerLayout->addWidget(m_headerSubtitleFontButton, 5, 3);
    headerLayout->addWidget(headerSideText, 6, 0);
    headerLayout->addWidget(m_headerSideText, 7, 0, 1, 2);
    headerLayout->addWidget(m_headerSideTextFontButton, 7, 3);
    headerLayout->addWidget(headerInfo, 8, 0);
    headerLayout->addWidget(m_headerInfoFontButton, 9, 0);

    headerLayout->setRowStretch(headerLayout->rowCount(), 1);
    headerLayout->setColumnStretch(0, 1);

    mainLayout->addWidget(headerGroup, 3, 0, 1, 2);

    auto* subHeaderGroup  = new QGroupBox(tr("Sub Header: "), this);
    auto* subHeaderLayout = new QGridLayout(subHeaderGroup);

    auto* subHeaderTitle     = new QLabel(tr("Title: "), this);
    auto* subHeaderRight     = new QLabel(tr("Duration: "), this);
    auto* subHeaderRowHeight = new QLabel(tr("Row height: "), this);

    m_subHeaderRowHeight->setMinimumWidth(120);
    m_subHeaderRowHeight->setMaximumWidth(120);

    m_subHeaderTitleFontButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_subHeaderRightFontButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    subHeaderLayout->addWidget(subHeaderRowHeight, 0, 0);
    subHeaderLayout->addWidget(m_subHeaderRowHeight, 1, 0);
    subHeaderLayout->addWidget(subHeaderTitle, 2, 0);
    subHeaderLayout->addWidget(m_subHeaderTitle, 3, 0);
    subHeaderLayout->addWidget(m_subHeaderTitleFontButton, 3, 1);
    subHeaderLayout->addWidget(subHeaderRight, 4, 0);
    subHeaderLayout->addWidget(m_subHeaderRightFontButton, 5, 0);

    detailsLayout->addWidget(subHeaderGroup, 1, 0);

    auto* trackGroup  = new QGroupBox(tr("Track: "), this);
    auto* trackLayout = new QGridLayout(trackGroup);

    subHeaderLayout->setRowStretch(subHeaderLayout->rowCount(), 1);

    auto* trackLeft      = new QLabel(tr("Left: "), this);
    auto* trackRight     = new QLabel(tr("Right: "), this);
    auto* trackRowHeight = new QLabel(tr("Row height: "), this);

    m_trackRowHeight->setMinimumWidth(120);
    m_trackRowHeight->setMaximumWidth(120);

    m_trackLeftFontButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_trackRightFontButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    trackLayout->addWidget(trackRowHeight, 0, 0);
    trackLayout->addWidget(m_trackRowHeight, 1, 0);
    trackLayout->addWidget(trackLeft, 2, 0);
    trackLayout->addWidget(m_trackLeft, 3, 0);
    trackLayout->addWidget(m_trackLeftFontButton, 3, 1);
    trackLayout->addWidget(trackRight, 4, 0);
    trackLayout->addWidget(m_trackRight, 5, 0);
    trackLayout->addWidget(m_trackRightFontButton, 5, 1);

    trackLayout->setRowStretch(trackLayout->rowCount(), 1);

    detailsLayout->addWidget(trackGroup, 2, 0);

    detailsLayout->setRowStretch(detailsLayout->rowCount(), 1);

    QObject::connect(m_presetList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &PlaylistPresetsPageWidget::selectionChanged);

    QObject::connect(m_newPreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::newPreset);
    QObject::connect(m_deletePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::deletePreset);
    QObject::connect(m_updatePreset, &QPushButton::clicked, this, &PlaylistPresetsPageWidget::updatePreset);

    QObject::connect(m_simpleHeader, &QPushButton::clicked, this, [this](bool checked) {
        m_showCover->setEnabled(!checked);
    });

    QObject::connect(m_headerTitleFontButton, &QPushButton::clicked, this, [this]() {
        showFontDialog(m_headerTitleFont);
    });
    QObject::connect(m_headerSubtitleFontButton, &QPushButton::clicked, this, [this]() {
        showFontDialog(m_headerSubtitleFont);
    });
    QObject::connect(m_headerSideTextFontButton, &QPushButton::clicked, this, [this]() {
        showFontDialog(m_headerSideTextFont);
    });
    QObject::connect(m_headerInfoFontButton, &QPushButton::clicked, this, [this]() {
        showFontDialog(m_headerInfoFont);
    });
    QObject::connect(m_subHeaderTitleFontButton, &QPushButton::clicked, this, [this]() {
        showFontDialog(m_subHeaderTitleFont);
    });
    QObject::connect(m_subHeaderRightFontButton, &QPushButton::clicked, this, [this]() {
        showFontDialog(m_subHeaderRightFont);
    });
    QObject::connect(m_trackLeftFontButton, &QPushButton::clicked, this, [this]() {
        showFontDialog(m_trackLeftFont);
    });
    QObject::connect(m_trackRightFontButton, &QPushButton::clicked, this, [this]() {
        showFontDialog(m_trackRightFont);
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
    if(selection.empty() || selection.size() == 1) {
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

    preset.header.title        = m_headerTitle->text();
    preset.header.titleFont    = m_headerTitleFont;
    preset.header.subtitle     = m_headerSubtitle->text();
    preset.header.subtitleFont = m_headerSubtitleFont;
    preset.header.sideText     = m_headerSideText->text();
    preset.header.sideTextFont = m_headerSideTextFont;
    preset.header.infoFont     = m_headerInfoFont;
    preset.header.rowHeight    = m_headerRowHeight->value();
    preset.header.simple       = m_simpleHeader->isChecked();
    preset.header.showCover    = m_showCover->isEnabled() && m_showCover->isChecked();

    preset.subHeader.title     = m_subHeaderTitle->text();
    preset.subHeader.titleFont = m_subHeaderTitleFont;
    preset.subHeader.rightFont = m_subHeaderRightFont;
    preset.subHeader.rowHeight = m_subHeaderRowHeight->value();

    preset.track.leftText      = m_trackLeft->text();
    preset.track.leftTextFont  = m_trackLeftFont;
    preset.track.rightText     = m_trackRight->text();
    preset.track.rightTextFont = m_trackRightFont;
    preset.track.rowHeight     = m_trackRowHeight->value();

    m_model->markForChange(preset);
}

void PlaylistPresetsPageWidget::showFontDialog(QFont& initialFont)
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, initialFont, this);
    if(ok) {
        initialFont = font;
    }
}

void PlaylistPresetsPageWidget::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    Q_UNUSED(deselected)
    if(selected.empty()) {
        return;
    }
    const auto index  = m_presetList->selectionModel()->selectedRows().constFirst();
    const auto* item  = static_cast<PresetItem*>(index.internalPointer());
    const auto preset = item->preset();

    setupPreset(preset);
}

void PlaylistPresetsPageWidget::setupPreset(const PlaylistPreset& preset)
{
    m_headerTitle->setText(preset.header.title);
    m_headerTitleFont = preset.header.titleFont;
    m_headerSubtitle->setText(preset.header.subtitle);
    m_headerSubtitleFont = preset.header.subtitleFont;
    m_headerSideText->setText(preset.header.sideText);
    m_headerSideTextFont = preset.header.sideTextFont;
    m_headerInfoFont     = preset.header.infoFont;
    m_headerRowHeight->setValue(preset.header.rowHeight);
    m_showCover->setChecked(preset.header.showCover);
    m_simpleHeader->setChecked(preset.header.simple);

    m_subHeaderTitle->setText(preset.subHeader.title);
    m_subHeaderTitleFont = preset.subHeader.titleFont;
    m_subHeaderRightFont = preset.subHeader.rightFont;
    m_subHeaderRowHeight->setValue(preset.subHeader.rowHeight);

    m_trackLeft->setText(preset.track.leftText);
    m_trackLeftFont = preset.track.leftTextFont;
    m_trackRight->setText(preset.track.rightText);
    m_trackRightFont = preset.track.rightTextFont;
    m_trackRowHeight->setValue(preset.track.rowHeight);
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
