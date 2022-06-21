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

#include "gui/filter/filterwidget.h"
#include "gui/widgets/dummy.h"
#include "gui/widgets/menuheader.h"
#include "gui/widgets/overlay.h"
#include "gui/widgets/spacer.h"
#include "gui/widgets/splitterwidget.h"
#include "utils/enumhelper.h"
#include "utils/settings.h"
#include "utils/widgetprovider.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>

namespace {
void addParentContext(Widget* widget, QMenu* menu)
{
    auto* parent = qobject_cast<SplitterWidget*>(widget->findParent());

    if(parent) {
        auto* remove = new QAction("Remove", menu);
        QAction::connect(remove, &QAction::triggered, widget, [=] {
            parent->removeWidget(widget);
        });
        menu->addAction(remove);

        menu->addAction(new MenuHeaderAction(parent->name(), menu));
        parent->layoutEditingMenu(menu);

        auto* parentOfParent = qobject_cast<SplitterWidget*>(parent->findParent());
        // Only splitters with a splitter parent are removable.
        if(parentOfParent) {
            auto* removeParent = new QAction("Remove", menu);
            QAction::connect(removeParent, &QAction::triggered, parent, [=] {
                parentOfParent->removeWidget(parent);
            });
            menu->addAction(removeParent);
        }
    }
}
} // namespace

EditableLayout::EditableLayout(WidgetProvider* widgetProvider, QWidget* parent)
    : QWidget(parent)
    , m_box(new QHBoxLayout(this))
    , m_settings(Settings::instance())
    , m_layoutEditing(false)
    , m_overlay(new Overlay(this))
    , m_menu(new QMenu(this))
    , m_widgetProvider(widgetProvider)
{
    setObjectName("EditableLayout");

    m_box->setContentsMargins(5, 5, 5, 5);
    setLayout(m_box);

    connect(m_menu, &QMenu::aboutToHide, this, &EditableLayout::hideOverlay);
    connect(m_settings, &Settings::layoutEditingChanged, this, [=] {
        m_layoutEditing = !m_layoutEditing;
    });

    bool loaded = loadLayout();
    if(!loaded) {
        m_splitter = m_widgetProvider->createSplitter(Qt::Vertical, this);
        m_box->addWidget(m_splitter);
    }
    qApp->installEventFilter(this);
}

void EditableLayout::changeLayout(const QByteArray& layout)
{
    delete m_splitter;
    loadLayout(layout);
}

EditableLayout::~EditableLayout() = default;

Widget* EditableLayout::splitterChild(QWidget* widget)
{
    QWidget* child = widget;

    while(!qobject_cast<Widget*>(child) || qobject_cast<Dummy*>(child)) {
        child = child->parentWidget();
    }

    if(child) {
        return qobject_cast<Widget*>(child);
    }

    return {};
}

bool EditableLayout::eventFilter(QObject* watched, QEvent* event)
{
    if(!m_layoutEditing) {
        return QWidget::eventFilter(watched, event);
    }

    if(event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if(mouseEvent->button() == Qt::RightButton && m_menu->isHidden()) {
            m_menu->clear();

            QWidget* widget = childAt(mouseEvent->pos());
            Widget* child = splitterChild(widget);

            if(child) {
                m_menu->addAction(new MenuHeaderAction(child->name(), m_menu));
                child->layoutEditingMenu(m_menu);
                addParentContext(child, m_menu);
            }
            if(child && !m_menu->isEmpty()) {
                showOverlay(child);
                m_menu->exec(mapToGlobal(mouseEvent->pos()));
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QRect EditableLayout::widgetGeometry(Widget* widget)
{
    const int w = widget->width();
    const int h = widget->height();

    int x = widget->x();
    int y = widget->y();

    while(widget->findParent()) {
        widget = widget->findParent();
        x += widget->x();
        y += widget->y();
    }
    return {x, y, w, h};
}

void EditableLayout::showOverlay(Widget* widget)
{
    m_overlay->setGeometry(widgetGeometry(widget));
    m_overlay->raise();
    m_overlay->show();
}

void EditableLayout::hideOverlay()
{
    m_overlay->hide();
}

// TODO: Move to splitter widget to remove recursion.
void EditableLayout::iterateSplitter(QJsonObject& splitterObject, QJsonArray& SplitterWidgetArray,
                                     SplitterWidget* splitter, bool isRoot)
{
    QJsonArray array;

    for(auto& [type, widget] : splitter->children()) {
        auto* childSplitter = qobject_cast<SplitterWidget*>(widget);
        if(childSplitter) {
            iterateSplitter(splitterObject, array, childSplitter, false);
        }
        else {
            QJsonObject widgetObject;
            if(type == Widgets::WidgetType::Filter) {
                auto* filter = qobject_cast<Library::FilterWidget*>(widget);
                QJsonObject filterObject;
                filterObject["Type"] = EnumHelper::toString(filter->type());
                widgetObject[EnumHelper::toString(type)] = filterObject;
                array.append(widgetObject);
            }
            else {
                array.append(EnumHelper::toString(type));
            }
        }
    }
    QString state = QString::fromUtf8(splitter->saveState().toBase64());

    QJsonObject children;
    children["Type"] = EnumHelper::toString(splitter->orientation());
    children["State"] = state;
    children["Children"] = array;

    if(isRoot) {
        splitterObject["Splitter"] = children;
    }
    else {
        QJsonObject object;
        object["Splitter"] = children;
        SplitterWidgetArray.append(object);
    }
}

void EditableLayout::saveLayout()
{
    QJsonObject root;
    QJsonObject object;
    QJsonArray array;

    iterateSplitter(object, array, m_splitter, true);

    root["Layout"] = object;

    QString json = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact).toBase64());

    Settings* settings = Settings::instance();
    settings->set(Settings::Setting::Layout, json);
}

// TODO: Move to splitter widget to remove recursion.
void EditableLayout::iterateInsertSplitter(const QJsonArray& array, SplitterWidget* splitter)
{
    for(const auto widget : array) {
        QJsonObject object = widget.toObject();
        if(object.contains("Splitter")) {
            QJsonObject SplitterWidgetSubObject = object["Splitter"].toObject();
            auto type = EnumHelper::fromString<Qt::Orientation>(SplitterWidgetSubObject["Type"].toString());
            auto widgetType = type == Qt::Vertical ? Widgets::WidgetType::VerticalSplitter
                                                   : Widgets::WidgetType::HorizontalSplitter;

            QJsonArray SplitterWidgetArray = SplitterWidgetSubObject["Children"].toArray();
            QByteArray SplitterWidgetState
                = QByteArray::fromBase64(SplitterWidgetSubObject["State"].toString().toUtf8());

            auto* childSplitterWidget = m_widgetProvider->createSplitter(type, this);
            splitter->addToSplitter(widgetType, childSplitterWidget);
            iterateInsertSplitter(SplitterWidgetArray, childSplitterWidget);
            childSplitterWidget->restoreState(SplitterWidgetState);
        }
        else if(object.contains("Filter")) {
            const QJsonObject filterObject = object["Filter"].toObject();
            auto filterType = EnumHelper::fromString<Filters::FilterType>(filterObject["Type"].toString());
            m_widgetProvider->createFilter(filterType, splitter);
        }
        else {
            auto type = EnumHelper::fromString<Widgets::WidgetType>(widget.toString());
            m_widgetProvider->createWidget(type, splitter);
        }
    }
}

bool EditableLayout::loadLayout(const QByteArray& layout)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(layout);
    if(!jsonDoc.isNull() && !jsonDoc.isEmpty()) {
        QJsonObject json = jsonDoc.object();
        if(json.contains("Layout") && json["Layout"].isObject()) {
            QJsonObject object = json["Layout"].toObject();
            if(object.contains("Splitter") && object["Splitter"].isObject()) {
                QJsonObject SplitterWidgetObject = object["Splitter"].toObject();

                auto type = EnumHelper::fromString<Qt::Orientation>(SplitterWidgetObject["Type"].toString());
                QJsonArray SplitterWidgetArray = SplitterWidgetObject["Children"].toArray();
                auto state = QByteArray::fromBase64(SplitterWidgetObject["State"].toString().toUtf8());

                m_splitter = m_widgetProvider->createSplitter(type, this);
                m_box->addWidget(m_splitter);

                iterateInsertSplitter(SplitterWidgetArray, m_splitter);
                m_splitter->restoreState(state);
            }
            return true;
        }
    }
    return false;
}

bool EditableLayout::loadLayout()
{
    auto layout = QByteArray::fromBase64(m_settings->value(Settings::Setting::Layout).toByteArray());
    return loadLayout(layout);
}
