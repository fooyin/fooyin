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

#include <gui/widgetprovider.h>

#include "layout/layoutcommands.h"

#include <gui/fywidget.h>
#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <gui/widgetcontainer.h>

#include <QAction>
#include <QLoggingCategory>
#include <QMenu>
#include <QUndoStack>

Q_LOGGING_CATEGORY(WIDGET_PROV, "fy.widgetprovider")

using namespace Qt::StringLiterals;

namespace {
struct FactoryWidget
{
    QString key;
    QString name;
    std::function<Fooyin::FyWidget*()> instantiator;
    std::function<bool()> isVisibleWhen;
    QStringList subMenus;
    bool isHidden{false};
    bool canSplit{false};
    bool supportsToolbar{false};
    Qt::ToolBarAreas toolbarAreas{Qt::TopToolBarArea | Qt::BottomToolBarArea};
    QJsonObject toolbarDefaults;
    int limit{0};
    int count{0};
};

std::vector<FactoryWidget> sortBySubMenu(const std::map<QString, FactoryWidget>& widgets)
{
    std::vector<FactoryWidget> sortedWidgets;
    sortedWidgets.reserve(widgets.size());
    for(const auto& [_, widget] : widgets) {
        sortedWidgets.push_back(widget);
    }

    std::ranges::sort(sortedWidgets, [](const auto& a, const auto& b) {
        const QString aFullPath = a.subMenus.join(u'.') + a.name;
        const QString bFullPath = b.subMenus.join(u'.') + b.name;
        return QString::localeAwareCompare(aFullPath, bFullPath) < 0;
    });

    return sortedWidgets;
}
} // namespace

namespace Fooyin {
class WidgetProviderPrivate
{
public:
    bool canCreateWidget(const QString& key)
    {
        if(!m_widgets.contains(key)) {
            return false;
        }

        const auto& widget = m_widgets.at(key);

        return widget.limit == 0 || widget.count < widget.limit;
    }

    template <typename Func>
    void setupWidgetMenu(QMenu* menu, Func&& func, bool splitOnly = false, bool toolbarOnly = false)
    {
        menu->clear();

        std::map<QString, QMenu*> menuCache;

        const auto widgets = sortBySubMenu(m_widgets);

        for(const auto& widget : widgets) {
            if(widget.isHidden || (widget.isVisibleWhen && !widget.isVisibleWhen())) {
                continue;
            }

            if(splitOnly && !widget.canSplit) {
                continue;
            }

            if(toolbarOnly && !widget.supportsToolbar) {
                continue;
            }

            auto* parentMenu = menu;
            if(!splitOnly) {
                for(const auto& subMenu : widget.subMenus) {
                    if(!menuCache.contains(subMenu)) {
                        auto* childMenu = new QMenu(subMenu, menu);
                        menuCache.emplace(subMenu, childMenu);
                        parentMenu->addMenu(childMenu);
                    }
                    parentMenu = menuCache.at(subMenu);
                }
            }

            auto* addWidgetAction = new QAction(widget.name, parentMenu);
            if(toolbarOnly) {
                Gui::setThemeIcon(addWidgetAction, Constants::Icons::Add);
            }
            addWidgetAction->setEnabled(canCreateWidget(widget.key));
            QObject::connect(addWidgetAction, &QAction::triggered, menu, [func, widget] { func(widget.key); });
            parentMenu->addAction(addWidgetAction);
        }
    }

    struct Instance
    {
        QString key;
        bool retained{false};
    };

    std::map<FyWidget*, Instance> m_instances;
    QUndoStack* m_layoutCommands{nullptr};
    std::map<QString, FactoryWidget> m_widgets;
};

WidgetProvider::WidgetProvider()
    : p{std::make_unique<WidgetProviderPrivate>()}
{ }

WidgetProvider::~WidgetProvider() = default;

void WidgetProvider::setCommandStack(QUndoStack* layoutCommands)
{
    p->m_layoutCommands = layoutCommands;
}

std::vector<WidgetCatalogueEntry> WidgetProvider::widgetCatalogue() const
{
    std::vector<WidgetCatalogueEntry> entries;

    for(const auto& widget : sortBySubMenu(p->m_widgets)) {
        if(widget.isHidden || (widget.isVisibleWhen && !widget.isVisibleWhen())) {
            continue;
        }
        entries.push_back({.key        = widget.key,
                           .name       = widget.name,
                           .categories = widget.subMenus,
                           .available  = widget.instantiator && p->canCreateWidget(widget.key)});
    }

    return entries;
}

bool WidgetProvider::registerWidget(const QString& key, std::function<FyWidget*()> instantiator,
                                    const QString& displayName, WidgetRegistrationOptions options)
{
    if(p->m_widgets.contains(key)) {
        qCWarning(WIDGET_PROV) << "Subclass already registered";
        return false;
    }

    FactoryWidget fw;
    fw.key             = key;
    fw.name            = displayName.isEmpty() ? key : displayName;
    fw.instantiator    = std::move(instantiator);
    fw.supportsToolbar = options.supportsToolbar;
    fw.toolbarAreas    = options.toolbarAreas;

    p->m_widgets.emplace(key, fw);
    return true;
}

void WidgetProvider::setSubMenus(const QString& key, const QStringList& subMenus)
{
    if(!p->m_widgets.contains(key)) {
        qCWarning(WIDGET_PROV) << "Subclass not registered";
        return;
    }

    p->m_widgets.at(key).subMenus = subMenus;
}

void WidgetProvider::setLimit(const QString& key, int limit)
{
    if(!p->m_widgets.contains(key)) {
        qCWarning(WIDGET_PROV) << "Subclass not registered";
        return;
    }

    p->m_widgets.at(key).limit = limit;
}

void WidgetProvider::setCanSplit(const QString& key, bool canSplit)
{
    if(!p->m_widgets.contains(key)) {
        qCWarning(WIDGET_PROV) << "Subclass not registered";
        return;
    }

    p->m_widgets.at(key).canSplit = canSplit;
}

void WidgetProvider::setIsHidden(const QString& key, bool hidden)
{
    if(!p->m_widgets.contains(key)) {
        qCWarning(WIDGET_PROV) << "Subclass not registered";
        return;
    }

    p->m_widgets.at(key).isHidden = hidden;
}

void WidgetProvider::setIsVisibleWhen(const QString& key, std::function<bool()> predicate)
{
    if(!p->m_widgets.contains(key)) {
        qCWarning(WIDGET_PROV) << "Subclass not registered";
        return;
    }

    p->m_widgets.at(key).isVisibleWhen = std::move(predicate);
}

void WidgetProvider::setToolbarDefaults(const QString& key, const QJsonObject& layout)
{
    if(!p->m_widgets.contains(key)) {
        qCWarning(WIDGET_PROV) << "Subclass not registered";
        return;
    }

    p->m_widgets.at(key).toolbarDefaults = layout;
}

bool WidgetProvider::widgetExists(const QString& key) const
{
    return p->m_widgets.contains(key);
}

QString WidgetProvider::displayName(const QString& key) const
{
    if(!p->m_widgets.contains(key)) {
        return key;
    }
    return p->m_widgets.at(key).name;
}

bool WidgetProvider::canCreateWidget(const QString& key) const
{
    return p->canCreateWidget(key);
}

bool WidgetProvider::supportsToolbar(const QString& key) const
{
    return p->m_widgets.contains(key) && p->m_widgets.at(key).supportsToolbar;
}

Qt::ToolBarAreas WidgetProvider::toolbarAreas(const QString& key) const
{
    if(!p->m_widgets.contains(key)) {
        return {};
    }
    return p->m_widgets.at(key).toolbarAreas;
}

QJsonObject WidgetProvider::toolbarDefaults(const QString& key) const
{
    if(!p->m_widgets.contains(key)) {
        return {};
    }
    return p->m_widgets.at(key).toolbarDefaults;
}

FyWidget* WidgetProvider::createWidget(const QString& key)
{
    if(!p->m_widgets.contains(key)) {
        return nullptr;
    }

    auto& widget = p->m_widgets.at(key);

    if(!widget.instantiator || !p->canCreateWidget(key)) {
        return nullptr;
    }

    widget.count++;

    auto* newWidget = widget.instantiator();
    if(!newWidget) {
        --widget.count;
        return nullptr;
    }

    p->m_instances.emplace(newWidget, WidgetProviderPrivate::Instance{.key = key});
    QObject::connect(newWidget, &QObject::destroyed, newWidget, [this, newWidget] {
        const auto instance = p->m_instances.find(newWidget);
        if(instance != p->m_instances.end()) {
            if(!instance->second.retained && p->m_widgets.contains(instance->second.key)) {
                --p->m_widgets.at(instance->second.key).count;
            }
            p->m_instances.erase(instance);
        }
    });

    return newWidget;
}

void WidgetProvider::setWidgetRetained(FyWidget* widget, bool retained)
{
    if(!widget) {
        return;
    }

    for(auto& [candidate, instance] : p->m_instances) {
        if((candidate == widget || widget->isAncestorOf(candidate)) && instance.retained != retained) {
            instance.retained = retained;
            p->m_widgets.at(instance.key).count += retained ? -1 : 1;
        }
    }
}

void WidgetProvider::setupAddWidgetMenu(EditableLayout* layout, QMenu* menu, WidgetContainer* container, int index)
{
    if(!p->m_layoutCommands) {
        return;
    }

    p->setupWidgetMenu(menu, [this, layout, container, index](const QString& key) {
        p->m_layoutCommands->push(new AddWidgetCommand(layout, this, container, key, index));
    });
}

void WidgetProvider::setupReplaceWidgetMenu(EditableLayout* layout, QMenu* menu, WidgetContainer* container,
                                            const Id& widgetId)
{
    if(!p->m_layoutCommands || !container) {
        return;
    }

    p->setupWidgetMenu(menu, [this, layout, container, widgetId](const QString& key) {
        p->m_layoutCommands->push(new ReplaceWidgetCommand(layout, this, container, key, widgetId));
    });
}

void WidgetProvider::setupSplitWidgetMenu(EditableLayout* layout, QMenu* menu, WidgetContainer* container,
                                          const Id& widgetId)
{
    if(!p->m_layoutCommands) {
        return;
    }

    p->setupWidgetMenu(
        menu,
        [this, layout, container, widgetId](const QString& key) {
            p->m_layoutCommands->push(new SplitWidgetCommand(layout, this, container, key, widgetId));
        },
        true);
}

void WidgetProvider::setupToolbarWidgetMenu(QMenu* menu, const std::function<void(const QString&)>& addWidget)
{
    p->setupWidgetMenu(menu, addWidget, false, true);
}
} // namespace Fooyin
