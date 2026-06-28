/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "projectmpresetdialog.h"

#include <QDir>
#include <QLineEdit>
#include <QVBoxLayout>

constexpr auto IndexRole = Qt::UserRole;
constexpr auto PathRole  = Qt::UserRole + 1;

namespace Fooyin::ProjectM {

PresetDialog::PresetDialog(const std::vector<ProjectMPreset>& presets, int currentIndex, const QString& currentPath,
                           QWidget* parent)
    : QDialog{parent}
    , m_filterEdit{new QLineEdit(this)}
    , m_presetList{new QListWidget(this)}
    , m_buttons{new QDialogButtonBox(QDialogButtonBox::Close, this)}
    , m_lastSelectedIndex{-1}
{
    setWindowTitle(tr("Select projectM Preset"));
    resize(560, 480);

    auto* layout = new QVBoxLayout(this);

    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setPlaceholderText(tr("Filter presets"));

    for(const auto& preset : presets) {
        auto* item
            = new QListWidgetItem(preset.relativePath.isEmpty() ? preset.name : preset.relativePath, m_presetList);
        item->setData(IndexRole, preset.index);
        item->setData(PathRole, preset.path);
        if(!preset.failureMessage.isEmpty()) {
            markPresetFailed(item, preset.failureMessage);
        }
        if((!preset.path.isEmpty() && preset.path == currentPath)
           || (preset.path.isEmpty() && preset.index == currentIndex)) {
            item->setSelected(true);
            m_presetList->setCurrentItem(item);
        }
    }

    m_presetList->setAlternatingRowColors(true);
    m_presetList->setSelectionMode(QAbstractItemView::SingleSelection);

    layout->addWidget(m_filterEdit);
    layout->addWidget(m_presetList, 1);
    layout->addWidget(m_buttons);

    QObject::connect(m_filterEdit, &QLineEdit::textChanged, this, &PresetDialog::refilter);
    QObject::connect(m_presetList, &QListWidget::currentItemChanged, this, &PresetDialog::emitSelectionChanged);
    QObject::connect(m_presetList, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    QObject::connect(m_presetList, &QListWidget::itemActivated, this, &QDialog::accept);
    QObject::connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if(auto* currentItem = m_presetList->currentItem()) {
        m_presetList->scrollToItem(currentItem, QAbstractItemView::PositionAtCenter);
    }
}

int PresetDialog::selectedPresetIndex() const
{
    if(auto* item = m_presetList->currentItem(); item && !item->isHidden()) {
        return item->data(IndexRole).toInt();
    }
    return -1;
}

QString PresetDialog::selectedPresetPath() const
{
    if(auto* item = m_presetList->currentItem(); item && !item->isHidden()) {
        return item->data(PathRole).toString();
    }
    return {};
}

void PresetDialog::markPresetFailed(int index, const QString& path, const QString& message)
{
    for(int row{0}; row < m_presetList->count(); ++row) {
        auto* item             = m_presetList->item(row);
        const QString itemPath = item->data(PathRole).toString();
        if((!path.isEmpty() && QDir::cleanPath(itemPath) == QDir::cleanPath(path))
           || (path.isEmpty() && item->data(IndexRole).toInt() == index)) {
            markPresetFailed(item, message);
            return;
        }
    }
}

void PresetDialog::markPresetFailed(QListWidgetItem* item, const QString& message)
{
    if(!item) {
        return;
    }

    item->setToolTip(message);
    item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
}

void PresetDialog::refilter()
{
    const QString filter = m_filterEdit->text().trimmed();
    QListWidgetItem* firstVisible{nullptr};

    for(int row{0}; row < m_presetList->count(); ++row) {
        auto* item       = m_presetList->item(row);
        const bool match = filter.isEmpty() || item->text().contains(filter, Qt::CaseInsensitive);
        item->setHidden(!match);
        if(match && !firstVisible) {
            firstVisible = item;
        }
    }

    if(!m_presetList->currentItem() || m_presetList->currentItem()->isHidden()) {
        m_presetList->setCurrentItem(firstVisible);
    }
}

void PresetDialog::emitSelectionChanged()
{
    const QString path = selectedPresetPath();
    if(!path.isEmpty()) {
        if(path == m_lastSelectedPath) {
            return;
        }
        m_lastSelectedPath = path;
        Q_EMIT presetPathSelected(path);
        return;
    }

    const int index = selectedPresetIndex();
    if(index >= 0 && index != m_lastSelectedIndex) {
        m_lastSelectedIndex = index;
        Q_EMIT presetIndexSelected(index);
    }
}
} // namespace Fooyin::ProjectM
