/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include <gui/editablelayout.h>

#include "internalguisettings.h"
#include "quicksetup/quicksetupdialog.h"
#include "widgets/dummy.h"
#include "widgets/menuheader.h"
#include "widgets/splitterwidget.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/layoutprovider.h>
#include <gui/widgetcontainer.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/id.h>
#include <utils/settings/settingsmanager.h>
#include <utils/widgets/overlaywidget.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>

#include <stack>

namespace {
Fooyin::FyWidget* splitterChild(QWidget* widget)
{
    if(!widget) {
        return {};
    }
    QWidget* child = widget;

    while(!qobject_cast<Fooyin::FyWidget*>(child)) {
        child = child->parentWidget();
        if(!child) {
            return {};
        }
    }
    return qobject_cast<Fooyin::FyWidget*>(child);
}
} // namespace

namespace Fooyin {
struct EditableLayout::Private
{
    EditableLayout* self;

    ActionManager* actionManager;
    SettingsManager* settings;
    WidgetProvider* widgetProvider;
    LayoutProvider* layoutProvider;

    ActionContainer* editingMenu;
    QHBoxLayout* box;
    QPointer<OverlayWidget> overlay;
    QPointer<SplitterWidget> splitter;
    bool layoutEditing{false};

    Private(EditableLayout* self_, ActionManager* actionManager_, WidgetProvider* widgetProvider_,
            LayoutProvider* layoutProvider_, SettingsManager* settings_)
        : self{self_}
        , actionManager{actionManager_}
        , settings{settings_}
        , widgetProvider{widgetProvider_}
        , layoutProvider{layoutProvider_}
        , editingMenu{actionManager->createMenu(Constants::Menus::Context::Layout)}
        , box{new QHBoxLayout(self)}
    {
        box->setContentsMargins(5, 5, 5, 5);
    }

    void setupDefault()
    {
        splitter = qobject_cast<SplitterWidget*>(widgetProvider->createWidget(QStringLiteral("SplitterVertical")));
        if(splitter) {
            splitter->setParent(self);
            splitter->finalise();
            box->addWidget(splitter);
            settings->set<Settings::Gui::LayoutEditing>(true);
        }
    }

    void changeEditingState(bool editing)
    {
        layoutEditing = editing;
        if(editing) {
            overlay = new OverlayWidget(self);

            qApp->installEventFilter(self);
        }
        else {
            qApp->removeEventFilter(self);
            overlay->deleteLater();
        }
    }

    void showOverlay(FyWidget* widget) const
    {
        overlay->setGeometry(widget->widgetGeometry());
        overlay->raise();
        overlay->show();
    }

    void hideOverlay() const
    {
        overlay->hide();
    }

    ActionContainer* createNewMenu(FyWidget* parent, const QString& title) const
    {
        const Id id   = parent->id().append(title);
        auto* newMenu = actionManager->createMenu(id);
        newMenu->menu()->setTitle(title);

        return newMenu;
    }

    void setupReplaceWidgetMenu(ActionContainer* menu, FyWidget* current) const
    {
        if(!menu->isEmpty()) {
            return;
        }

        auto* parent = qobject_cast<WidgetContainer*>(current->findParent());
        if(!parent) {
            return;
        }

        widgetProvider->setupWidgetMenu(
            menu, [parent, current](FyWidget* newWidget) { parent->replaceWidget(current, newWidget); });
    }

    void setupContextMenu(FyWidget* widget, ActionContainer* menu) const
    {
        if(!widget || !menu) {
            return;
        }

        FyWidget* currentWidget = widget;
        int level               = settings->value<Settings::Gui::Internal::EditingMenuLevels>();

        while(level > 0 && currentWidget) {
            menu->addAction(new MenuHeaderAction(currentWidget->name(), menu));
            currentWidget->layoutEditingMenu(menu);

            auto* parent = qobject_cast<WidgetContainer*>(currentWidget->findParent());
            if(parent) {
                auto* changeMenu = createNewMenu(currentWidget, tr("&Replace"));
                setupReplaceWidgetMenu(changeMenu, currentWidget);
                menu->addMenu(changeMenu);

                auto* remove = new QAction(tr("Remove"), menu);
                QObject::connect(remove, &QAction::triggered, parent,
                                 [parent, currentWidget] { parent->removeWidget(currentWidget); });
                menu->addAction(remove);
            }
            currentWidget = parent;
            --level;
        }
    }

    template <typename T, typename Predicate>
    T findWidgets(const Predicate& predicate) const
    {
        if(!splitter) {
            if constexpr(std::is_same_v<T, FyWidget*>) {
                return nullptr;
            }
            return {};
        }

        T widgets;

        std::stack<FyWidget*> widgetsToCheck;
        widgetsToCheck.push(splitter);

        while(!widgetsToCheck.empty()) {
            auto* current = widgetsToCheck.top();
            widgetsToCheck.pop();

            if(!current) {
                continue;
            }

            if(predicate(current)) {
                if constexpr(std::is_same_v<T, WidgetList>) {
                    widgets.push_back(current);
                }
                else {
                    return current;
                }
            }

            if(const auto* container = qobject_cast<WidgetContainer*>(current)) {
                const auto containerWidgets = container->widgets();
                for(FyWidget* containerWidget : containerWidgets) {
                    widgetsToCheck.push(containerWidget);
                }
            }
        }

        if constexpr(std::is_same_v<T, FyWidget*>) {
            return nullptr;
        }
        return widgets;
    }
};

EditableLayout::EditableLayout(ActionManager* actionManager, WidgetProvider* widgetProvider,
                               LayoutProvider* layoutProvider, SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<Private>(this, actionManager, widgetProvider, layoutProvider, settings)}
{
    setObjectName(QStringLiteral("EditableLayout"));
}

EditableLayout::~EditableLayout() = default;

void EditableLayout::initialise()
{
    if(!loadLayout()) {
        p->setupDefault();
    }

    p->settings->subscribe<Settings::Gui::LayoutEditing>(this,
                                                         [this](bool enabled) { p->changeEditingState(enabled); });

    QObject::connect(p->editingMenu->menu(), &QMenu::aboutToHide, this, [this]() { p->hideOverlay(); });
}

FyWidget* EditableLayout::findWidget(const Id& id) const
{
    return p->findWidgets<FyWidget*>([&id](FyWidget* widget) { return widget->id() == id; });
}

WidgetList EditableLayout::findWidgetsByName(const QString& name) const
{
    return p->findWidgets<WidgetList>([&name](FyWidget* widget) { return widget->name() == name; });
}

WidgetList EditableLayout::findWidgetsByFeatures(const FyWidget::Features& features) const
{
    return p->findWidgets<WidgetList>([&features](FyWidget* widget) { return widget->features() & features; });
}

bool EditableLayout::eventFilter(QObject* watched, QEvent* event)
{
    if(!p->layoutEditing || event->type() != QEvent::MouseButtonPress) {
        return QWidget::eventFilter(watched, event);
    }

    if(event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);

        if(!mouseEvent || mouseEvent->button() != Qt::RightButton || !p->editingMenu->isHidden()) {
            return QWidget::eventFilter(watched, event);
        }

        p->editingMenu->clear();

        const QPoint pos = mouseEvent->globalPosition().toPoint();
        QWidget* widget  = qApp->widgetAt(pos);
        FyWidget* child  = splitterChild(widget);

        if(!child) {
            return QWidget::eventFilter(watched, event);
        }

        if(qobject_cast<Dummy*>(child)) {
            p->setupContextMenu(child->findParent(), p->editingMenu);
        }
        else {
            p->setupContextMenu(child, p->editingMenu);
        }

        p->showOverlay(child);
        p->editingMenu->menu()->popup(pos);
    }

    event->accept();
    return true;
}

void EditableLayout::changeLayout(const Layout& layout)
{
    delete p->splitter;

    if(loadLayout(layout)) {
        p->settings->set<Settings::Gui::LayoutEditing>(p->splitter->childCount() == 0);
    }
    else {
        p->setupDefault();
    }
}

void EditableLayout::saveLayout()
{
    QJsonObject root;
    QJsonArray array;

    p->splitter->saveLayout(array);

    root[QStringLiteral("Default")] = array;

    const QByteArray json = QJsonDocument(root).toJson();

    if(const auto layout = LayoutProvider::readLayout(json)) {
        p->layoutProvider->changeLayout(layout.value());
    }
}

bool EditableLayout::loadLayout(const Layout& layout)
{
    if(layout.json.isEmpty()) {
        return false;
    }

    const auto name = layout.json.constBegin().key();
    if(auto* splitter = qobject_cast<SplitterWidget*>(p->widgetProvider->createWidget(name))) {
        p->splitter               = splitter;
        const QJsonObject options = layout.json.constBegin()->toObject();
        p->splitter->loadLayout(options);
        p->splitter->finalise();
        p->box->addWidget(p->splitter);
        return true;
    }

    return false;
}

bool EditableLayout::loadLayout()
{
    const Layout layout = p->layoutProvider->currentLayout();
    return loadLayout(layout);
}

void EditableLayout::showQuickSetup()
{
    auto* quickSetup = new QuickSetupDialog(p->layoutProvider, this);
    quickSetup->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(quickSetup, &QuickSetupDialog::layoutChanged, this, &EditableLayout::changeLayout);
    quickSetup->show();
}
} // namespace Fooyin

#include "gui/moc_editablelayout.cpp"
