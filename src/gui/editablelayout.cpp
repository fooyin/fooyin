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
#include <QStyle>
#include <QUndoStack>

#include <stack>

constexpr auto LayoutVersion = 1;

namespace Fooyin {
class RootContainer : public WidgetContainer
{
    Q_OBJECT

public:
    explicit RootContainer(WidgetProvider* provider, SettingsManager* settings, QWidget* parent = nullptr)
        : WidgetContainer{provider, settings, parent}
        , m_settings{settings}
        , m_layout{new QVBoxLayout(this)}
        , m_widget{new Dummy(m_settings, this)}
    {
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->addWidget(m_widget);
    }

    void reset()
    {
        delete m_widget;
        m_widget = new Dummy(m_settings, this);
        m_layout->addWidget(m_widget);
    }

    [[nodiscard]] FyWidget* widget() const
    {
        return m_widget;
    }

    [[nodiscard]] QString name() const override
    {
        return QStringLiteral("Root");
    }

    [[nodiscard]] QString layoutName() const override
    {
        return name();
    }

    [[nodiscard]] bool canAddWidget() const override
    {
        return !m_widget || qobject_cast<Dummy*>(m_widget);
    }

    [[nodiscard]] bool canMoveWidget(int /*index*/, int /*newIndex*/) const override
    {
        return false;
    }

    [[nodiscard]] int widgetIndex(const Id& id) const override
    {
        return m_widget && m_widget->id() == id ? 0 : -1;
    }

    [[nodiscard]] FyWidget* widgetAtId(const Id& id) const override
    {
        return m_widget && m_widget->id() == id ? m_widget : nullptr;
    }

    [[nodiscard]] FyWidget* widgetAtIndex(int index) const override
    {
        return index == 0 ? m_widget : nullptr;
    }

    [[nodiscard]] int widgetCount() const override
    {
        return m_widget && !qobject_cast<Dummy*>(m_widget) ? 1 : 0;
    }

    [[nodiscard]] WidgetList widgets() const override
    {
        if(m_widget) {
            return {m_widget};
        }

        return {};
    }

    int addWidget(FyWidget* widget) override
    {
        insertWidget(0, widget);
        return 0;
    }

    void insertWidget(int index, FyWidget* widget) override
    {
        if(index != 0) {
            return;
        }

        widget->setParent(this);
        delete m_widget;
        m_widget = widget;
        m_layout->insertWidget(0, m_widget);
    }

    void removeWidget(int index) override
    {
        if(index == 0) {
            reset();
        }
    }

    void replaceWidget(int index, FyWidget* newWidget) override
    {
        insertWidget(index, newWidget);
    }

    void moveWidget(int /*index*/, int /*newIndex*/) override { }

private:
    SettingsManager* m_settings;
    QVBoxLayout* m_layout;
    QPointer<FyWidget> m_widget;
};

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
    RootContainer* root;
    bool layoutEditing{false};
    bool prevShowHandles{false};

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
        , root{new RootContainer(widgetProvider, settings, self)}
        , editingContext{new WidgetContext(self, Context{"Fooyin.LayoutEditing"}, self)}
        , layoutHistory{new QUndoStack(self)}
    {
        updateMargins();
        box->addWidget(root);

        widgetProvider->setCommandStack(layoutHistory);

        settings->subscribe<Settings::Gui::LayoutEditing>(self, [this](bool enabled) { changeEditingState(enabled); });
        settings->subscribe<Settings::Gui::Internal::EditableLayoutMargin>(self, [this]() { updateMargins(); });
    }

    void setupDefault() const
    {
        root->reset();
        settings->set<Settings::Gui::LayoutEditing>(true);
    }

    void updateMargins() const
    {
        const int margin = settings->value<Settings::Gui::Internal::EditableLayoutMargin>();
        if(margin >= 0) {
            box->setContentsMargins(margin, margin, margin, margin);
        }
        else {
            box->setContentsMargins(self->style()->pixelMetric(QStyle::PM_LayoutLeftMargin),
                                    self->style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                    self->style()->pixelMetric(QStyle::PM_LayoutRightMargin),
                                    self->style()->pixelMetric(QStyle::PM_LayoutBottomMargin));
        }
    }

    void changeEditingState(bool editing)
    {
        if(std::exchange(layoutEditing, editing) == editing) {
            return;
        }

        if(editing) {
            prevShowHandles = settings->value<Settings::Gui::Internal::SplitterHandles>();
            settings->set<Settings::Gui::Internal::SplitterHandles>(true);

            actionManager->overrideContext(editingContext, true);
            overlay = new OverlayWidget(self);
            qApp->installEventFilter(self);
        }
        else {
            actionManager->overrideContext(editingContext, false);
            qApp->removeEventFilter(self);
            if(overlay) {
                overlay->deleteLater();
            }

            settings->set<Settings::Gui::Internal::SplitterHandles>(prevShowHandles);
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

    void setupAddWidgetMenu(QMenu* menu, WidgetContainer* parent, FyWidget* prev, FyWidget* current) const
    {
        if(auto* container = qobject_cast<WidgetContainer*>(current)) {
            auto* addMenu = new QMenu(tr("&Insert"), menu);
            addMenu->setEnabled(container->canAddWidget());
            const int insertIndex = current == prev ? container->widgetCount() : container->widgetIndex(prev->id()) + 1;
            widgetProvider->setupAddWidgetMenu(self, addMenu, container, insertIndex);
            menu->addMenu(addMenu);
        }
        else if(qobject_cast<Dummy*>(current)) {
            auto* addMenu = new QMenu(tr("&Insert"), menu);
            addMenu->setEnabled(parent->canAddWidget());
            widgetProvider->setupReplaceWidgetMenu(self, addMenu, parent, current->id());
            menu->addMenu(addMenu);
        }
    }

    bool setupMoveWidgetMenu(QMenu* menu, WidgetContainer* parent, FyWidget* current) const
    {
        if(!menu->isEmpty()) {
            return false;
        }

        const int widgetIndex = parent->widgetIndex(current->id());
        const bool horizontal = parent->orientation() == Qt::Horizontal;

        auto* moveLeft = new QAction(horizontal ? tr("Left") : tr("Up"), menu);
        moveLeft->setEnabled(parent->canMoveWidget(widgetIndex, widgetIndex - 1));
        QObject::connect(moveLeft, &QAction::triggered, parent, [this, parent, widgetIndex] {
            layoutHistory->push(new MoveWidgetCommand(self, widgetProvider, parent, widgetIndex, widgetIndex - 1));
        });
        menu->addAction(moveLeft);

        auto* moveRight = new QAction(horizontal ? tr("Right") : tr("Down"), menu);
        moveRight->setEnabled(parent->canMoveWidget(widgetIndex, widgetIndex + 1));
        QObject::connect(moveRight, &QAction::triggered, parent, [this, parent, widgetIndex] {
            layoutHistory->push(new MoveWidgetCommand(self, widgetProvider, parent, widgetIndex, widgetIndex + 1));
        });
        menu->addAction(moveRight);

        auto* moveFarLeft = new QAction(horizontal ? tr("Far Left") : tr("Top"), menu);
        moveFarLeft->setEnabled(parent->canMoveWidget(widgetIndex, 0));
        QObject::connect(moveFarLeft, &QAction::triggered, parent, [this, parent, widgetIndex] {
            layoutHistory->push(new MoveWidgetCommand(self, widgetProvider, parent, widgetIndex, 0));
        });
        menu->addAction(moveFarLeft);

        auto* moveFarRight = new QAction(horizontal ? tr("Far Right") : tr("Bottom"), menu);
        moveFarRight->setEnabled(parent->canMoveWidget(widgetIndex, parent->fullWidgetCount() - 1));
        QObject::connect(moveFarRight, &QAction::triggered, parent, [this, parent, widgetIndex] {
            layoutHistory->push(
                new MoveWidgetCommand(self, widgetProvider, parent, widgetIndex, parent->fullWidgetCount() - 1));
        });
        menu->addAction(moveFarRight);

        return moveLeft->isEnabled() || moveRight->isEnabled() || moveFarLeft->isEnabled() || moveFarRight->isEnabled();
    }

    void setupPasteAction(QMenu* menu, FyWidget* prev, FyWidget* current)
    {
        if(auto* container = qobject_cast<WidgetContainer*>(current)) {
            if(container->canAddWidget()) {
                auto* pasteInsert = new QAction(tr("Paste (Insert)"), menu);
                const int insertIndex
                    = current == prev ? container->widgetCount() : container->widgetIndex(prev->id()) + 1;
                QObject::connect(pasteInsert, &QAction::triggered, container, [this, container, insertIndex] {
                    layoutHistory->push(
                        new AddWidgetCommand(self, widgetProvider, container, widgetClipboard, insertIndex));
                });
                menu->addAction(pasteInsert);
            }
        }
    }

    void setupContextMenu(FyWidget* widget, QMenu* menu)
    {
        if(!widget || !menu) {
            return;
        }

        FyWidget* prevWidget    = widget;
        FyWidget* currentWidget = widget;
        int level               = settings->value<Settings::Gui::Internal::EditingMenuLevels>();

        while(level > 0 && currentWidget && currentWidget != root) {
            const bool isDummy = qobject_cast<Dummy*>(currentWidget);

            menu->addAction(new MenuHeaderAction(isDummy ? QStringLiteral("Widget") : currentWidget->name(), menu));

            currentWidget->layoutEditingMenu(menu);

            auto* parent = qobject_cast<WidgetContainer*>(currentWidget->findParent());

            setupAddWidgetMenu(menu, parent, prevWidget, currentWidget);

            if(!isDummy) {
                auto* changeMenu = new QMenu(tr("&Replace"), menu);
                widgetProvider->setupReplaceWidgetMenu(self, changeMenu, parent, currentWidget->id());
                menu->addMenu(changeMenu);

                if(parent) {
                    auto* splitMenu = new QMenu(tr("&Split"), menu);
                    widgetProvider->setupSplitWidgetMenu(self, splitMenu, parent, currentWidget->id());
                    menu->addMenu(splitMenu);
                }

                auto* copy = new QAction(tr("Copy"), menu);
                copy->setEnabled(widgetProvider->canCreateWidget(currentWidget->layoutName()));
                QObject::connect(copy, &QAction::triggered, currentWidget, [this, currentWidget] {
                    widgetClipboard = EditableLayout::saveBaseWidget(currentWidget);
                });
                menu->addAction(copy);
            }

            if(!widgetClipboard.isEmpty() && widgetProvider->canCreateWidget(widgetClipboard.constBegin().key())) {
                if(parent && !isDummy) {
                    setupPasteAction(menu, prevWidget, currentWidget);
                }

                auto* paste = new QAction(tr("Paste (Replace)"), menu);
                QObject::connect(paste, &QAction::triggered, currentWidget, [this, parent, currentWidget] {
                    layoutHistory->push(
                        new ReplaceWidgetCommand(self, widgetProvider, parent, widgetClipboard, currentWidget->id()));
                });
                menu->addAction(paste);
            }

            if(parent && parent != root) {
                auto* moveMenu = new QMenu(tr("&Move"), menu);
                moveMenu->setEnabled(setupMoveWidgetMenu(moveMenu, parent, currentWidget));
                menu->addMenu(moveMenu);
            }

            if(!isDummy || parent->widgetCount() > 1) {
                auto* remove = new QAction(tr("Remove"), menu);
                QObject::connect(remove, &QAction::triggered, currentWidget, [this, parent, currentWidget] {
                    layoutHistory->push(new RemoveWidgetCommand(self, widgetProvider, parent, currentWidget->id()));
                });
                menu->addAction(remove);
            }

            prevWidget    = currentWidget;
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

    WidgetList findAllWidgets() const
    {
        if(!root) {
            return {};
        }

        WidgetList widgets;

        std::stack<FyWidget*> widgetsToCheck;
        widgetsToCheck.push(root);

        while(!widgetsToCheck.empty()) {
            auto* current = widgetsToCheck.top();
            widgetsToCheck.pop();

            if(!current) {
                continue;
            }

            widgets.push_back(current);

            if(const auto* container = qobject_cast<WidgetContainer*>(current)) {
                const auto containerWidgets = container->widgets();
                for(FyWidget* containerWidget : containerWidgets) {
                    widgetsToCheck.push(containerWidget);
                }
            }
        }

        return widgets;
    }

    template <typename T, typename Predicate>
    T findWidgets(const Predicate& predicate) const
    {
        if(!root) {
            if constexpr(std::is_same_v<T, FyWidget*>) {
                return nullptr;
            }
            return {};
        }

        T widgets;

        std::stack<FyWidget*> widgetsToCheck;
        widgetsToCheck.push(root);

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

EditableLayout::~EditableLayout()
{
    p->settings->set<Settings::Gui::LayoutEditing>(false);
}

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

    changeLayout(p->layoutProvider->currentLayout());
}

std::optional<Layout> EditableLayout::saveCurrentToLayout(const QString& name)
{
    QJsonObject root;
    QJsonArray array;

    if(p->root->widget()) {
        p->root->widget()->saveLayout(array);
    }
    else {
        array.append(QJsonObject{});
    }

    QString layoutName{name};
    if(layoutName.isEmpty()) {
        layoutName = QStringLiteral("Layout %1").arg(p->layoutProvider->layouts().size());
    }

    root[QStringLiteral("Name")]    = layoutName;
    root[QStringLiteral("Version")] = LayoutVersion;
    root[QStringLiteral("Widgets")] = array;

    const QByteArray json = QJsonDocument(root).toJson();

    return LayoutProvider::readLayout(json);
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

        p->setupContextMenu(child, p->editingMenu);
        p->showOverlay(child);
        p->editingMenu->popup(pos);
    }

    event->accept();
    return true;
}

void EditableLayout::changeLayout(const Layout& layout)
{
    p->root->reset();

    if(!loadLayout(layout)) {
        p->setupDefault();
    }

    if(p->root->widgetCount() == 0) {
        p->settings->set<Settings::Gui::LayoutEditing>(true);
    }
    else {
        p->settings->set<Settings::Gui::LayoutEditing>(false);
    }
}

void EditableLayout::saveLayout()
{
    if(const auto layout = saveCurrentToLayout(QStringLiteral("Default"))) {
        p->layoutProvider->changeLayout(layout.value());
    }
}

bool EditableLayout::loadLayout(const Layout& layout)
{
    if(layout.json.isEmpty()) {
        return false;
    }

    p->layoutHistory->clear();

    if(!layout.json.contains(QStringLiteral("Widgets"))) {
        return false;
    }

    if(!layout.json.value(QStringLiteral("Widgets")).isArray()) {
        return false;
    }

    const auto rootWidgets = layout.json.value(QStringLiteral("Widgets")).toArray();

    if(rootWidgets.empty() || !rootWidgets.cbegin()->isObject()) {
        return false;
    }

    const auto rootObject = rootWidgets.cbegin()->toObject();
    const auto widgetKey  = rootObject.constBegin().key();
    auto* topWidget       = p->widgetProvider->createWidget(widgetKey);

    if(!topWidget) {
        return false;
    }

    p->root->addWidget(topWidget);

    if(rootObject.constBegin()->isObject()) {
        const QJsonObject options = rootObject.constBegin()->toObject();
        topWidget->loadLayout(options);
    }

    topWidget->finalise();
    return true;
}

QJsonObject EditableLayout::saveWidget(FyWidget* widget)
{
    QJsonArray array;

    widget->saveLayout(array);

    if(!array.empty() && array.constBegin()->isObject()) {
        return array.constBegin()->toObject();
    }

    return {};
}

QJsonObject EditableLayout::saveBaseWidget(FyWidget* widget)
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

#include "editablelayout.moc"
#include "gui/moc_editablelayout.cpp"
