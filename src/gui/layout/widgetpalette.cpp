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

#include "widgetpalette.h"
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/iconloader.h>
#include <gui/widgetprovider.h>
#include <utils/settings/settingsmanager.h>

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <map>

using namespace Qt::StringLiterals;

namespace Fooyin {
PaletteTree::PaletteTree(QWidget* parent)
    : QTreeWidget{parent}
{
    setDragEnabled(true);
}

void PaletteTree::startDrag(Qt::DropActions /*supportedActions*/)
{
    if(const auto* item = currentItem(); item && item->flags().testFlag(Qt::ItemIsEnabled)) {
        if(const auto key = item->data(0, Qt::UserRole).toString(); !key.isEmpty()) {
            Q_EMIT widgetDragRequested(key);
        }
    }
}

WidgetPalette::WidgetPalette(WidgetProvider* provider, SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , m_provider{provider}
    , m_settings{settings}
    , m_search{new QLineEdit(this)}
    , m_entries{new PaletteTree(this)}
{
    setMinimumWidth(160);
    setAutoFillBackground(true);
    setAttribute(Qt::WA_NoMousePropagation);
    resize(280, 480);

    m_search->setPlaceholderText(tr("Search widgets…"));
    m_search->setClearButtonEnabled(true);

    auto* dockButton = new QToolButton(this);
    dockButton->setIcon(Gui::iconFromTheme(Constants::Icons::WindowPin));
    dockButton->setToolTip(tr("Dock widget palette"));
    dockButton->setCheckable(true);
    dockButton->setAutoRaise(true);
    dockButton->setChecked(m_settings->value<Settings::Gui::DockWidgetPalette>());
    m_settings->subscribe<Settings::Gui::DockWidgetPalette>(dockButton, &QToolButton::setChecked);
    QObject::connect(dockButton, &QToolButton::clicked, this,
                     [this](bool checked) { m_settings->set<Settings::Gui::DockWidgetPalette>(checked); });

    m_entries->setHeaderHidden(true);
    m_entries->setSelectionMode(QAbstractItemView::SingleSelection);

    auto* searchRow = new QHBoxLayout();
    searchRow->addWidget(m_search, 1);
    searchRow->addWidget(dockButton);

    auto* box = new QVBoxLayout(this);
    box->addLayout(searchRow);
    box->addWidget(m_entries);

    QObject::connect(m_search, &QLineEdit::textChanged, this, &WidgetPalette::refresh);
    QObject::connect(m_search, &QLineEdit::returnPressed, m_entries,
                     [this]() { m_entries->setFocus(Qt::OtherFocusReason); });
    QObject::connect(m_entries, &PaletteTree::widgetDragRequested, this, &WidgetPalette::widgetDragRequested);
}

void WidgetPalette::refresh()
{
    m_entries->clear();

    std::map<QString, QTreeWidgetItem*> categories;
    const auto query = m_search->text().trimmed();

    const auto widgets = m_provider->widgetCatalogue();
    for(const auto& widget : widgets) {
        auto category = widget.categories.join(u" / "_s);
        if(category.isEmpty()) {
            category = tr("Widgets");
        }

        if(!widget.name.contains(query, Qt::CaseInsensitive) && !category.contains(query, Qt::CaseInsensitive)) {
            continue;
        }

        auto& group = categories[category];
        if(!group) {
            group = new QTreeWidgetItem(m_entries, {category});
            group->setFlags(Qt::ItemIsEnabled);
            group->setExpanded(true);

            auto font = group->font(0);
            font.setBold(true);
            group->setFont(0, font);
        }

        auto* item = new QTreeWidgetItem(group, {widget.name});
        item->setData(0, Qt::UserRole, widget.key);
        item->setIcon(0, Gui::iconFromTheme(Constants::Icons::Add));
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
                       | (widget.available ? Qt::ItemIsEnabled : Qt::NoItemFlags));
        item->setToolTip(0, widget.available ? tr("Drag to add %1.\nHold Ctrl to replace a widget.").arg(widget.name)
                                             : tr("Instance limit reached"));
    }
}

} // namespace Fooyin

#include "moc_widgetpalette.cpp"
