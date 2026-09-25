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

#include <gui/guiconstants.h>
#include <gui/iconloader.h>

#include <QDir>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

constexpr auto IndexRole      = Qt::UserRole;
constexpr auto PathRole       = Qt::UserRole + 1;
constexpr auto IdentifierRole = Qt::UserRole + 2;
constexpr auto FavouriteRole  = Qt::UserRole + 3;

namespace Fooyin::ProjectM {
namespace {
bool identifiersEqual(const QString& lhs, const QString& rhs)
{
#ifdef Q_OS_WIN
    return lhs.compare(rhs, Qt::CaseInsensitive) == 0;
#else
    return lhs == rhs;
#endif
}
} // namespace

PresetDialog::PresetDialog(const std::vector<ProjectMPreset>& presets, int currentIndex, const QString& currentPath,
                           QWidget* parent)
    : QDialog{parent}
    , m_filterEdit{new QLineEdit(this)}
    , m_favouritesOnlyButton{new QToolButton(this)}
    , m_presetList{new QListWidget(this)}
    , m_hoveredFavouriteItem{nullptr}
    , m_hoveredFavouriteState{false}
    , m_buttons{new QDialogButtonBox(QDialogButtonBox::Close, this)}
    , m_lastSelectedIndex{-1}
{
    setWindowTitle(tr("Select projectM Preset"));
    resize(560, 480);

    auto* layout       = new QVBoxLayout(this);
    auto* filterLayout = new QHBoxLayout();

    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setPlaceholderText(tr("Filter presets"));

    m_favouritesOnlyButton->setCheckable(true);
    m_favouritesOnlyButton->setIcon(Gui::iconFromTheme(Constants::Icons::Favorite));
    m_favouritesOnlyButton->setText(tr("Favourites only"));
    m_favouritesOnlyButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    for(const auto& preset : presets) {
        auto* item
            = new QListWidgetItem(preset.relativePath.isEmpty() ? preset.name : preset.relativePath, m_presetList);
        item->setData(IndexRole, preset.index);
        item->setData(PathRole, preset.path);
        item->setData(IdentifierRole, preset.relativePath.isEmpty() ? preset.path : preset.relativePath);

        setFavourite(item, preset.favourite);

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
    m_presetList->setMouseTracking(true);
    m_presetList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_presetList->viewport()->installEventFilter(this);

    filterLayout->addWidget(m_filterEdit, 1);
    filterLayout->addWidget(m_favouritesOnlyButton);
    layout->addLayout(filterLayout);
    layout->addWidget(m_presetList, 1);
    layout->addWidget(m_buttons);

    QObject::connect(m_filterEdit, &QLineEdit::textChanged, this, &PresetDialog::refilter);
    QObject::connect(m_favouritesOnlyButton, &QToolButton::toggled, this, &PresetDialog::refilter);
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

void PresetDialog::updateFavourite(const QString& identifier, bool favourite)
{
    for(int row{0}; row < m_presetList->count(); ++row) {
        auto* item = m_presetList->item(row);
        if(identifiersEqual(item->data(IdentifierRole).toString(), identifier)) {
            setFavourite(item, favourite);
            refilter();
            return;
        }
    }
}

bool PresetDialog::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == m_presetList->viewport() && event->type() == QEvent::MouseMove) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        auto* item             = m_presetList->itemAt(mouseEvent->position().toPoint());
        if(item && favouriteIconRect(item).contains(mouseEvent->position().toPoint())) {
            setHoveredFavouriteItem(item);
            m_presetList->viewport()->setCursor(Qt::PointingHandCursor);
        }
        else {
            setHoveredFavouriteItem(nullptr);
            m_presetList->viewport()->unsetCursor();
        }
    }
    else if(watched == m_presetList->viewport() && event->type() == QEvent::Leave) {
        setHoveredFavouriteItem(nullptr);
        m_presetList->viewport()->unsetCursor();
    }

    if(watched == m_presetList->viewport()
       && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease
           || event->type() == QEvent::MouseButtonDblClick)) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if(mouseEvent->button() == Qt::LeftButton) {
            auto* item = m_presetList->itemAt(mouseEvent->position().toPoint());
            if(item && favouriteIconRect(item).contains(mouseEvent->position().toPoint())) {
                if(event->type() == QEvent::MouseButtonRelease) {
                    toggleFavourite(item);
                }
                return true;
            }
        }
    }

    return QDialog::eventFilter(watched, event);
}

void PresetDialog::markPresetFailed(QListWidgetItem* item, const QString& message)
{
    if(!item) {
        return;
    }

    item->setToolTip(message);
    item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
}

void PresetDialog::setFavourite(QListWidgetItem* item, bool favourite)
{
    if(!item) {
        return;
    }

    item->setData(FavouriteRole, favourite);
    updateFavouriteIcon(item);
}

void PresetDialog::updateFavouriteIcon(QListWidgetItem* item)
{
    if(!item) {
        return;
    }

    bool favourite = item->data(FavouriteRole).toBool();
    if(item == m_hoveredFavouriteItem) {
        favourite = m_hoveredFavouriteState;
    }
    item->setIcon(Gui::iconFromTheme(favourite ? Constants::Icons::Favorite : Constants::Icons::FavoriteOff));
}

void PresetDialog::setHoveredFavouriteItem(QListWidgetItem* item)
{
    if(item == m_hoveredFavouriteItem) {
        return;
    }

    auto* previous = std::exchange(m_hoveredFavouriteItem, nullptr);
    updateFavouriteIcon(previous);

    m_hoveredFavouriteItem = item;
    if(m_hoveredFavouriteItem) {
        m_hoveredFavouriteState = !m_hoveredFavouriteItem->data(FavouriteRole).toBool();
    }
    updateFavouriteIcon(m_hoveredFavouriteItem);
}

void PresetDialog::toggleFavourite(QListWidgetItem* item)
{
    if(!item || item->isHidden()) {
        return;
    }

    const bool favourite = !item->data(FavouriteRole).toBool();
    setFavourite(item, favourite);
    Q_EMIT favouriteChanged(item->data(IdentifierRole).toString(), favourite);
    refilter();
}

QRect PresetDialog::favouriteIconRect(const QListWidgetItem* item) const
{
    if(!item) {
        return {};
    }

    QRect rect = m_presetList->visualItemRect(item);

    const int iconSize = m_presetList->iconSize().isValid()
                           ? m_presetList->iconSize().width()
                           : m_presetList->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, m_presetList);
    const int hitWidth = iconSize + (m_presetList->style()->pixelMetric(QStyle::PM_FocusFrameHMargin) * 2) + 4;
    if(m_presetList->layoutDirection() == Qt::RightToLeft) {
        rect.setLeft(rect.right() - hitWidth + 1);
    }
    else {
        rect.setWidth(hitWidth);
    }

    return rect;
}

void PresetDialog::refilter()
{
    const QString filter      = m_filterEdit->text().trimmed();
    const bool favouritesOnly = m_favouritesOnlyButton->isChecked();
    QListWidgetItem* firstVisible{nullptr};

    for(int row{0}; row < m_presetList->count(); ++row) {
        auto* item       = m_presetList->item(row);
        const bool match = (!favouritesOnly || item->data(FavouriteRole).toBool())
                        && (filter.isEmpty() || item->text().contains(filter, Qt::CaseInsensitive));
        item->setHidden(!match);
        if(match && !firstVisible) {
            firstVisible = item;
        }
    }

    if(m_hoveredFavouriteItem && m_hoveredFavouriteItem->isHidden()) {
        setHoveredFavouriteItem(nullptr);
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
