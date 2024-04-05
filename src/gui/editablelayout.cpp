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
#include "layoutcommands.h"
#include "quicksetup/quicksetupdialog.h"
#include "utils/actions/command.h"
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
#include <QUndoStack>

#include <stack>

namespace Fooyin {
struct EditableLayout::Private
{
    EditableLayout* self;

    ActionManager* actionManager;
    SettingsManager* settings;
    WidgetProvider* widgetProvider;
    LayoutProvider* layoutProvider;

    QPointer<QMenu> editingMenu;
    QHBoxLayout* box;
    QPointer<OverlayWidget> overlay;
    QPointer<SplitterWidget> splitter;
    bool layoutEditing{false};

    WidgetContext* editingContext;
    QJsonObject widgetClipboard;
    QUndoStack* layoutHistory;

    Private(EditableLayout* self_, ActionManager* actionManager_, WidgetProvider* widgetProvider_,
            LayoutProvider* layoutProvider_, SettingsManager* settings_)
        : self{self_}
        , actionManager{actionManager_}
        , settings{settings_}
        , widgetProvider{widgetProvider_}
        , layoutProvider{layoutProvider_}
        , box{new QHBoxLayout(self)}
        , editingContext{new WidgetContext(self, Context{"Fooyin.LayoutEditing"}, self)}
        , layoutHistory{new QUndoStack(self)}
    {
        box->setContentsMargins(5, 5, 5, 5);

        editingContext->setEnabled(false);
        editingContext->setGlobal(true);
        actionManager->addContextObject(editingContext);
        widgetProvider->setCommandStack(layoutHistory);
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
            editingContext->setEnabled(true);
            overlay = new OverlayWidget(self);
            qApp->installEventFilter(self);
        }
        else {
            editingContext->setEnabled(false);
            qApp->removeEventFilter(self);
            if(overlay) {
                overlay->deleteLater();
            }
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

    void setupMoveWidgetMenu(QMenu* menu, WidgetContainer* parent, FyWidget* current) const
    {
        if(!menu->isEmpty()) {
            return;
        }

        const int widgetIndex = parent->widgetIndex(current->id());
        const bool horizontal = parent->orientation() == Qt::Horizontal;

        auto* moveLeft = new QAction(horizontal ? tr("Left") : tr("Up"), menu);
        moveLeft->setEnabled(parent->canMoveWidget(widgetIndex, widgetIndex - 1));
        QObject::connect(moveLeft, &QAction::triggered, parent, [this, parent, widgetIndex] {
            layoutHistory->push(new MoveWidgetCommand(widgetProvider, parent, widgetIndex, widgetIndex - 1));
        });
        menu->addAction(moveLeft);

        auto* moveRight = new QAction(horizontal ? tr("Right") : tr("Down"), menu);
        moveRight->setEnabled(parent->canMoveWidget(widgetIndex, widgetIndex + 1));
        QObject::connect(moveRight, &QAction::triggered, parent, [this, parent, widgetIndex] {
            layoutHistory->push(new MoveWidgetCommand(widgetProvider, parent, widgetIndex, widgetIndex + 1));
        });
        menu->addAction(moveRight);

        auto* moveFarLeft = new QAction(horizontal ? tr("Far Left") : tr("Top"), menu);
        moveFarLeft->setEnabled(parent->canMoveWidget(widgetIndex, 0));
        QObject::connect(moveFarLeft, &QAction::triggered, parent, [this, parent, widgetIndex] {
            layoutHistory->push(new MoveWidgetCommand(widgetProvider, parent, widgetIndex, 0));
        });
        menu->addAction(moveFarLeft);

        auto* moveFarRight = new QAction(horizontal ? tr("Far Right") : tr("Bottom"), menu);
        moveFarRight->setEnabled(parent->canMoveWidget(widgetIndex, parent->widgetCount() - 1));
        QObject::connect(moveFarRight, &QAction::triggered, parent, [this, parent, widgetIndex] {
            layoutHistory->push(new MoveWidgetCommand(widgetProvider, parent, widgetIndex, parent->widgetCount() - 1));
        });
        menu->addAction(moveFarRight);
    }

    void setupContextMenu(FyWidget* widget, QMenu* menu)
    {
        if(!widget || !menu) {
            return;
        }

        FyWidget* currentWidget = widget;
        int level               = settings->value<Settings::Gui::Internal::EditingMenuLevels>();

        auto addPasteAction = [this, menu](FyWidget* containerWidget) {
            if(auto* container = qobject_cast<WidgetContainer*>(containerWidget)) {
                if(container->canAddWidget()) {
                    auto* pasteInsert = new QAction(tr("Paste (Insert)"), menu);
                    QObject::connect(pasteInsert, &QAction::triggered, container, [this, container] {
                        layoutHistory->push(new AddWidgetCommand(widgetProvider, container, widgetClipboard));
                    });
                    menu->addAction(pasteInsert);
                }
            }
        };

        while(level > 0 && currentWidget) {
            menu->addAction(new MenuHeaderAction(currentWidget->name(), menu));
            currentWidget->layoutEditingMenu(menu);

            auto* parent = qobject_cast<WidgetContainer*>(currentWidget->findParent());
            if(parent) {
                auto* moveMenu = new QMenu(tr("&Move"), menu);
                setupMoveWidgetMenu(moveMenu, parent, currentWidget);
                menu->addMenu(moveMenu);

                auto* changeMenu = new QMenu(tr("&Replace"), menu);
                widgetProvider->setupReplaceWidgetMenu(changeMenu, parent, currentWidget->id());
                menu->addMenu(changeMenu);

                auto* copy = new QAction(tr("Copy"), menu);
                copy->setEnabled(widgetProvider->canCreateWidget(currentWidget->layoutName()));
                QObject::connect(copy, &QAction::triggered, parent, [this, currentWidget] {
                    widgetClipboard = EditableLayout::saveWidget(currentWidget);
                });
                menu->addAction(copy);

                if(!widgetClipboard.isEmpty() && widgetProvider->canCreateWidget(widgetClipboard.constBegin().key())) {
                    addPasteAction(currentWidget);

                    auto* paste = new QAction(tr("Paste (Replace)"), menu);
                    QObject::connect(paste, &QAction::triggered, parent, [this, parent, currentWidget] {
                        layoutHistory->push(
                            new ReplaceWidgetCommand(widgetProvider, parent, widgetClipboard, currentWidget->id()));
                    });
                    menu->addAction(paste);
                }

                auto* remove = new QAction(tr("Remove"), menu);
                QObject::connect(remove, &QAction::triggered, parent, [this, parent, currentWidget] {
                    layoutHistory->push(new RemoveWidgetCommand(widgetProvider, parent, currentWidget->id()));
                });
                menu->addAction(remove);
            }
            else if(!widgetClipboard.isEmpty() && widgetProvider->canCreateWidget(widgetClipboard.constBegin().key())) {
                addPasteAction(currentWidget);
            }
            currentWidget = parent;
            --level;
        }
    }

    static FyWidget* findSplitterChild(QWidget* widget)
    {
        if(!widget) {
            return {};
        }
        QWidget* child = widget;

        while(!qobject_cast<FyWidget*>(child)) {
            child = child->parentWidget();
            if(!child) {
                return {};
            }
        }
        return qobject_cast<FyWidget*>(child);
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
    auto* editMenu = p->actionManager->actionContainer(Constants::Menus::Edit);

    auto* undo    = new QAction(tr("Undo"), this);
    auto* undoCmd = p->actionManager->registerAction(undo, Constants::Actions::Undo, p->editingContext->context());
    undoCmd->setDefaultShortcut(QKeySequence::Undo);
    editMenu->addAction(undoCmd);
    QObject::connect(undo, &QAction::triggered, this, [this]() { p->layoutHistory->undo(); });
    QObject::connect(p->layoutHistory, &QUndoStack::canUndoChanged, this,
                     [undo](bool canUndo) { undo->setEnabled(canUndo); });
    undo->setEnabled(p->layoutHistory->canUndo());

    auto* redo    = new QAction(tr("Redo"), this);
    auto* redoCmd = p->actionManager->registerAction(redo, Constants::Actions::Redo, p->editingContext->context());
    redoCmd->setDefaultShortcut(QKeySequence::Redo);
    editMenu->addAction(redoCmd);
    QObject::connect(redo, &QAction::triggered, this, [this]() { p->layoutHistory->redo(); });
    QObject::connect(p->layoutHistory, &QUndoStack::canRedoChanged, this,
                     [redo](bool canRedo) { redo->setEnabled(canRedo); });
    redo->setEnabled(p->layoutHistory->canRedo());

    if(!loadLayout()) {
        p->setupDefault();
    }

    p->settings->subscribe<Settings::Gui::LayoutEditing>(this,
                                                         [this](bool enabled) { p->changeEditingState(enabled); });
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

        if(!mouseEvent || mouseEvent->button() != Qt::RightButton || (p->editingMenu && !p->editingMenu->isHidden())) {
            return QWidget::eventFilter(watched, event);
        }

        p->editingMenu = new QMenu(this);
        p->editingMenu->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(p->editingMenu, &QMenu::aboutToHide, this, [this]() { p->hideOverlay(); });

        const QPoint pos = mouseEvent->globalPosition().toPoint();
        QWidget* widget  = QApplication::widgetAt(pos);
        FyWidget* child  = p->findSplitterChild(widget);

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
        p->editingMenu->popup(pos);
    }

    event->accept();
    return true;
}

void EditableLayout::changeLayout(const Layout& layout)
{
    delete p->splitter;

    if(loadLayout(layout)) {
        p->settings->set<Settings::Gui::LayoutEditing>(p->splitter->widgets().empty());
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

    p->layoutHistory->clear();

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

QJsonObject EditableLayout::saveWidget(FyWidget* widget)
{
    QJsonArray array;

    widget->saveBaseLayout(array);

    if(!array.empty() && array.constBegin()->isObject()) {
        return array.constBegin()->toObject();
    }

    return {};
}

FyWidget* EditableLayout::loadWidget(WidgetProvider* provider, const QJsonObject& layout)
{
    if(layout.empty()) {
        return nullptr;
    }

    const auto name = layout.constBegin().key();
    if(auto* widget = provider->createWidget(name)) {
        const QJsonObject options = layout.constBegin()->toObject();
        widget->loadLayout(options);
        return widget;
    }

    return nullptr;
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
