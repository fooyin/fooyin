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

#include "libraryfilterswitcher.h"

#include "filterconstants.h"

#include <core/library/libraryfilter.h>
#include <core/library/libraryfilterregistry.h>
#include <core/library/musiclibrary.h>
#include <utils/itemregistry.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QComboBox>
#include <QInputDialog>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
LibraryFilterSwitcher::LibraryFilterSwitcher(LibraryFilterRegistry* registry, MusicLibrary* library,
                                             SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_registry{registry}
    , m_library{library}
    , m_settings{settings}
    , m_presets{new QComboBox(this)}
    , m_allLibraryName{tr("All")}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_presets);

    QObject::connect(m_presets, &QComboBox::currentIndexChanged, this, [this]() { activateCurrent(); });
    m_presets->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(m_presets, &QWidget::customContextMenuRequested, this, &LibraryFilterSwitcher::showContextMenu);
    QObject::connect(m_registry, &RegistryBase::itemAdded, this, [this]() { populate(); });
    QObject::connect(m_registry, &LibraryFilterRegistry::libraryFilterChanged, this,
                     &LibraryFilterSwitcher::filterChanged);
    QObject::connect(m_registry, &RegistryBase::itemRemoved, this, [this](int id) {
        auto activeFilters = m_library->activeLibraryFilters();
        const auto active  = std::ranges::find(activeFilters, id, &LibraryFilter::id);
        if(active != activeFilters.end()) {
            activeFilters.erase(active);
            m_library->setActiveLibraryFilters(std::move(activeFilters));
        }
        populate();
    });
    QObject::connect(m_library, &MusicLibrary::activeLibraryFiltersChanged, this, [this]() { populate(); });
}

QString LibraryFilterSwitcher::name() const
{
    return tr("Saved Filter Selector");
}

QString LibraryFilterSwitcher::layoutName() const
{
    return u"SavedFilterSelector"_s;
}

void LibraryFilterSwitcher::saveLayoutData(QJsonObject& layout)
{
    layout["AllLibraryName"_L1] = m_allLibraryName;
}

void LibraryFilterSwitcher::loadLayoutData(const QJsonObject& layout)
{
    if(const QString name = layout.value("AllLibraryName"_L1).toString().trimmed(); !name.isEmpty()) {
        m_allLibraryName = name;
    }
}

void LibraryFilterSwitcher::finalise()
{
    populate();
}

void LibraryFilterSwitcher::populate()
{
    const QSignalBlocker blocker{m_presets};
    m_presets->clear();
    m_presets->addItem(m_allLibraryName, -1);

    const LibraryFilterList activeFilters = m_library->activeLibraryFilters();
    int activeIndex{0};
    for(const LibraryFilter& preset : m_registry->items()) {
        if(!preset.enabled) {
            continue;
        }
        m_presets->addItem(preset.name, preset.id);
        if(activeFilters.size() == 1 && activeFilters.front().id == preset.id) {
            activeIndex = m_presets->count() - 1;
        }
    }
    m_presets->setCurrentIndex(activeIndex);

    QStringList expressions;
    expressions.reserve(static_cast<qsizetype>(activeFilters.size()));

    for(const LibraryFilter& filter : activeFilters) {
        expressions.push_back(filter.expression);
    }

    m_presets->setToolTip(expressions.join(u" AND "_s));
}

void LibraryFilterSwitcher::filterChanged(const LibraryFilter& filter)
{
    auto activeFilters = m_library->activeLibraryFilters();
    const auto active  = std::ranges::find(activeFilters, filter.id, &LibraryFilter::id);
    if(active != activeFilters.end()) {
        if(filter.enabled) {
            *active = filter;
        }
        else {
            activeFilters.erase(active);
        }
        m_library->setActiveLibraryFilters(std::move(activeFilters));
    }

    populate();
}

void LibraryFilterSwitcher::activateCurrent()
{
    const int id = m_presets->currentData().toInt();
    if(id < 0) {
        m_library->clearActiveLibraryFilters();
        return;
    }

    if(const auto preset = m_registry->itemById(id)) {
        m_library->setActiveLibraryFilters({*preset});
    }
}

void LibraryFilterSwitcher::showContextMenu(const QPoint& pos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* rename = menu->addAction(tr("Rename 'All' filter"));
    QObject::connect(rename, &QAction::triggered, this, [this]() {
        bool ok{false};
        const QString newName = QInputDialog::getText(this, tr("Rename 'All' Filter"), tr("Name:"), QLineEdit::Normal,
                                                      m_allLibraryName, &ok);

        if(ok && !newName.trimmed().isEmpty()) {
            m_allLibraryName = newName.trimmed();
            populate();
        }
    });

    auto* manage = menu->addAction(tr("Manage library filters…"));
    QObject::connect(manage, &QAction::triggered, this,
                     [this]() { m_settings->settingsDialog()->openAtPage(Id{Constants::Page::Filters}); });

    menu->popup(m_presets->mapToGlobal(pos));
}
} // namespace Fooyin::Filters

#include "moc_libraryfilterswitcher.cpp"
