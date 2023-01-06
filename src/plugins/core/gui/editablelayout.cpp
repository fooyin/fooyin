/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "editablelayout.h"

#include "core/gui/widgets/dummy.h"
#include "core/gui/widgets/menuheader.h"
#include "core/gui/widgets/overlay.h"
#include "core/gui/widgets/splitterwidget.h"
#include "core/settings/settings.h"
#include "core/widgets/widgetprovider.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>
#include <pluginsystem/pluginmanager.h>
#include <utils/enumhelper.h>

namespace {
void addParentContext(FyWidget* widget, QMenu* menu)
{
    auto* parent = qobject_cast<SplitterWidget*>(widget->findParent());

    if(parent) {
        auto* remove = new QAction("Remove", menu);
        QAction::connect(remove, &QAction::triggered, parent, [parent, widget] {
            parent->removeWidget(widget);
        });
        menu->addAction(remove);

        menu->addAction(new MenuHeaderAction(parent->name(), menu));
        parent->layoutEditingMenu(menu);

        auto* parentOfParent = qobject_cast<SplitterWidget*>(parent->findParent());
        // Only splitters with a splitter parent are removable.
        if(parentOfParent) {
            auto* removeParent = new QAction("Remove", menu);
            QAction::connect(removeParent, &QAction::triggered, parentOfParent, [parentOfParent, parent] {
                parentOfParent->removeWidget(parent);
            });
            menu->addAction(removeParent);
        }
    }
}
} // namespace

struct EditableLayout::Private
{
    QHBoxLayout* box;
    Settings* settings;
    bool layoutEditing{false};
    Overlay* overlay;
    SplitterWidget* splitter;
    QMenu* menu;
    WidgetProvider* widgetProvider;

    explicit Private(QWidget* parent)
        : box{new QHBoxLayout(parent)}
        , settings{PluginSystem::object<Settings>()}
        , overlay{new Overlay(parent)}
        , menu{new QMenu(parent)}
        , widgetProvider{PluginSystem::object<WidgetProvider>()}
    { }
};

EditableLayout::EditableLayout(QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this)}
{
    setObjectName("EditableLayout");

    p->box->setContentsMargins(5, 5, 5, 5);

    connect(p->menu, &QMenu::aboutToHide, this, &EditableLayout::hideOverlay);
    connect(p->settings, &Settings::layoutEditingChanged, this, [this](bool enabled) {
        p->layoutEditing = enabled;
    });

    bool loaded = loadLayout();
    if(!loaded) {
        p->splitter = WidgetProvider::createSplitter(Qt::Vertical, this);
        p->box->addWidget(p->splitter);
    }
    if(!p->splitter->hasChildren()) {
        p->settings->set(Settings::Setting::LayoutEditing, true);
    }
    qApp->installEventFilter(this);
}

void EditableLayout::changeLayout(const QByteArray& layout)
{
    // Delete all current widgets
    // TODO: Look into caching previous layout widgets
    p->splitter->deleteLater();
    bool success = loadLayout(layout);
    if(success && p->splitter->hasChildren()) {
        p->settings->set(Settings::Setting::LayoutEditing, false);
    }
    else {
        p->settings->set(Settings::Setting::LayoutEditing, true);
    }
}

EditableLayout::~EditableLayout() = default;

FyWidget* EditableLayout::splitterChild(QWidget* widget)
{
    QWidget* child = widget;

    while(!qobject_cast<FyWidget*>(child) || qobject_cast<Dummy*>(child)) {
        child = child->parentWidget();
    }

    if(child) {
        return qobject_cast<FyWidget*>(child);
    }

    return {};
}

bool EditableLayout::eventFilter(QObject* watched, QEvent* event)
{
    if(!p->layoutEditing) {
        return QWidget::eventFilter(watched, event);
    }

    if(event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if(mouseEvent->button() == Qt::RightButton && p->menu->isHidden()) {
            p->menu->clear();

            QWidget* widget = childAt(mouseEvent->pos());
            FyWidget* child = splitterChild(widget);

            if(child) {
                p->menu->addAction(new MenuHeaderAction(child->objectName(), p->menu));
                child->layoutEditingMenu(p->menu);
                addParentContext(child, p->menu);
            }
            if(child && !p->menu->isEmpty()) {
                showOverlay(child);
                p->menu->exec(mapToGlobal(mouseEvent->pos()));
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QRect EditableLayout::widgetGeometry(FyWidget* widget)
{
    const int w = widget->width();
    const int h = widget->height();

    int x = widget->x();
    int y = widget->y();

    while((widget = widget->findParent())) {
        x += widget->x();
        y += widget->y();
    }
    return {x, y, w, h};
}

void EditableLayout::showOverlay(FyWidget* widget)
{
    p->overlay->setGeometry(widgetGeometry(widget));
    p->overlay->raise();
    p->overlay->show();
}

void EditableLayout::hideOverlay()
{
    p->overlay->hide();
}

void EditableLayout::saveLayout()
{
    QJsonObject root;
    QJsonObject object;
    QJsonArray array;

    p->splitter->saveSplitter(object, array);

    root["Layout"] = object;

    QString json = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact).toBase64());

    p->settings->set(Settings::Setting::Layout, json);
}

bool EditableLayout::loadLayout(const QByteArray& layout)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(layout);
    if(!jsonDoc.isNull() && !jsonDoc.isEmpty()) {
        QJsonObject json = jsonDoc.object();
        if(json.contains("Layout") && json["Layout"].isObject()) {
            QJsonObject object = json["Layout"].toObject();
            if(object.contains("Splitter") && object["Splitter"].isObject()) {
                QJsonObject splitterObject = object["Splitter"].toObject();

                auto type = EnumHelper::fromString<Qt::Orientation>(splitterObject["Type"].toString());
                QJsonArray splitterChildren = splitterObject["Children"].toArray();
                auto state = QByteArray::fromBase64(splitterObject["State"].toString().toUtf8());

                p->splitter = WidgetProvider::createSplitter(type, this);
                p->box->addWidget(p->splitter);

                p->splitter->loadSplitter(splitterChildren, p->splitter);
                p->splitter->restoreState(state);
            }
            return true;
        }
    }
    return false;
}

bool EditableLayout::loadLayout()
{
    auto layout = QByteArray::fromBase64(p->settings->value(Settings::Setting::Layout).toByteArray());
    return loadLayout(layout);
}
