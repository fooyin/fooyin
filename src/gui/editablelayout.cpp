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

namespace {
Fooyin::FyWidget* findSplitterChild(QWidget* widget)
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

class EditableLayoutPrivate
{
public:
    EditableLayoutPrivate(EditableLayout* self, ActionManager* actionManager, WidgetProvider* widgetProvider,
                          LayoutProvider* layoutProvider, SettingsManager* settings);

    void setupDefault() const;
    void updateMargins() const;
    void changeEditingState(bool editing);

    void showOverlay(FyWidget* widget) const;
    void hideOverlay() const;

    void setupAddWidgetMenu(QMenu* menu, WidgetContainer* parent, FyWidget* prev, FyWidget* current) const;
    bool setupMoveWidgetMenu(QMenu* menu, WidgetContainer* parent, FyWidget* current) const;
    void setupPasteAction(QMenu* menu, FyWidget* prev, FyWidget* current);
    void setupContextMenu(FyWidget* widget, QMenu* menu);

    [[nodiscard]] WidgetList findAllWidgets() const;

    template <typename T, typename Predicate>
    [[nodiscard]] T findWidgets(const Predicate& predicate) const
    {
        if(!m_root) {
            if constexpr(std::is_same_v<T, FyWidget*>) {
                return nullptr;
            }
            return {};
        }

        T widgets;

        std::stack<FyWidget*> widgetsToCheck;
        widgetsToCheck.push(m_root);

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

    EditableLayout* m_self;

    ActionManager* m_actionManager;
    SettingsManager* m_settings;
    WidgetProvider* m_widgetProvider;
    LayoutProvider* m_layoutProvider;

    QPointer<QMenu> m_editingMenu;
    QHBoxLayout* m_box;
    QPointer<OverlayWidget> m_overlay;
    RootContainer* m_root;
    bool m_layoutEditing{false};
    bool m_prevShowHandles{false};

    WidgetContext* m_editingContext;
    QJsonObject m_widgetClipboard;
    QUndoStack* m_layoutHistory;
};

EditableLayoutPrivate::EditableLayoutPrivate(EditableLayout* self, ActionManager* actionManager,
                                             WidgetProvider* widgetProvider, LayoutProvider* layoutProvider,
                                             SettingsManager* settings)
    : m_self{self}
    , m_actionManager{actionManager}
    , m_settings{settings}
    , m_widgetProvider{widgetProvider}
    , m_layoutProvider{layoutProvider}
    , m_box{new QHBoxLayout(m_self)}
    , m_root{new RootContainer(m_widgetProvider, m_settings, m_self)}
    , m_editingContext{new WidgetContext(m_self, Context{"Fooyin.LayoutEditing"}, m_self)}
    , m_layoutHistory{new QUndoStack(m_self)}
{
    updateMargins();
    m_box->addWidget(m_root);

    m_widgetProvider->setCommandStack(m_layoutHistory);

    m_settings->subscribe<Settings::Gui::LayoutEditing>(m_self, [this](bool enabled) { changeEditingState(enabled); });
    m_settings->subscribe<Settings::Gui::Internal::EditableLayoutMargin>(m_self, [this]() { updateMargins(); });
}

void EditableLayoutPrivate::setupDefault() const
{
    m_root->reset();
    m_settings->set<Settings::Gui::LayoutEditing>(true);
}

void EditableLayoutPrivate::updateMargins() const
{
    const int margin = m_settings->value<Settings::Gui::Internal::EditableLayoutMargin>();
    if(margin >= 0) {
        m_box->setContentsMargins(margin, margin, margin, margin);
    }
    else {
        m_box->setContentsMargins(m_self->style()->pixelMetric(QStyle::PM_LayoutLeftMargin),
                                  m_self->style()->pixelMetric(QStyle::PM_LayoutTopMargin),
                                  m_self->style()->pixelMetric(QStyle::PM_LayoutRightMargin),
                                  m_self->style()->pixelMetric(QStyle::PM_LayoutBottomMargin));
    }
}

void EditableLayoutPrivate::changeEditingState(bool editing)
{
    if(std::exchange(m_layoutEditing, editing) == editing) {
        return;
    }

    if(editing) {
        m_prevShowHandles = m_settings->value<Settings::Gui::Internal::SplitterHandles>();
        m_settings->set<Settings::Gui::Internal::SplitterHandles>(true);

        m_actionManager->overrideContext(m_editingContext, true);
        m_overlay = new OverlayWidget(m_self);
        qApp->installEventFilter(m_self);
    }
    else {
        m_actionManager->overrideContext(m_editingContext, false);
        qApp->removeEventFilter(m_self);
        if(m_overlay) {
            m_overlay->deleteLater();
        }

        m_settings->set<Settings::Gui::Internal::SplitterHandles>(m_prevShowHandles);
        m_self->saveLayout();
    }
}

void EditableLayoutPrivate::showOverlay(FyWidget* widget) const
{
    m_overlay->setGeometry(widget->widgetGeometry());
    m_overlay->raise();
    m_overlay->show();
}

void EditableLayoutPrivate::hideOverlay() const
{
    m_overlay->hide();
}

void EditableLayoutPrivate::setupAddWidgetMenu(QMenu* menu, WidgetContainer* parent, FyWidget* prev,
                                               FyWidget* current) const
{
    if(auto* container = qobject_cast<WidgetContainer*>(current)) {
        auto* addMenu = new QMenu(EditableLayout::tr("&Insert"), menu);
        addMenu->setEnabled(container->canAddWidget());
        const int insertIndex = current == prev ? container->widgetCount() : container->widgetIndex(prev->id()) + 1;
        m_widgetProvider->setupAddWidgetMenu(m_self, addMenu, container, insertIndex);
        menu->addMenu(addMenu);
    }
    else if(qobject_cast<Dummy*>(current)) {
        auto* addMenu = new QMenu(EditableLayout::tr("&Insert"), menu);
        addMenu->setEnabled(parent->canAddWidget());
        m_widgetProvider->setupReplaceWidgetMenu(m_self, addMenu, parent, current->id());
        menu->addMenu(addMenu);
    }
}

bool EditableLayoutPrivate::setupMoveWidgetMenu(QMenu* menu, WidgetContainer* parent, FyWidget* current) const
{
    if(!menu->isEmpty()) {
        return false;
    }

    const int widgetIndex = parent->widgetIndex(current->id());
    const bool horizontal = parent->orientation() == Qt::Horizontal;

    auto* moveLeft = new QAction(horizontal ? EditableLayout::tr("Left") : EditableLayout::tr("Up"), menu);
    moveLeft->setEnabled(parent->canMoveWidget(widgetIndex, widgetIndex - 1));
    QObject::connect(moveLeft, &QAction::triggered, parent, [this, parent, widgetIndex] {
        m_layoutHistory->push(new MoveWidgetCommand(m_self, m_widgetProvider, parent, widgetIndex, widgetIndex - 1));
    });
    menu->addAction(moveLeft);

    auto* moveRight = new QAction(horizontal ? EditableLayout::tr("Right") : EditableLayout::tr("Down"), menu);
    moveRight->setEnabled(parent->canMoveWidget(widgetIndex, widgetIndex + 1));
    QObject::connect(moveRight, &QAction::triggered, parent, [this, parent, widgetIndex] {
        m_layoutHistory->push(new MoveWidgetCommand(m_self, m_widgetProvider, parent, widgetIndex, widgetIndex + 1));
    });
    menu->addAction(moveRight);

    auto* moveFarLeft = new QAction(horizontal ? EditableLayout::tr("Far Left") : EditableLayout::tr("Top"), menu);
    moveFarLeft->setEnabled(parent->canMoveWidget(widgetIndex, 0));
    QObject::connect(moveFarLeft, &QAction::triggered, parent, [this, parent, widgetIndex] {
        m_layoutHistory->push(new MoveWidgetCommand(m_self, m_widgetProvider, parent, widgetIndex, 0));
    });
    menu->addAction(moveFarLeft);

    auto* moveFarRight = new QAction(horizontal ? EditableLayout::tr("Far Right") : EditableLayout::tr("Bottom"), menu);
    moveFarRight->setEnabled(parent->canMoveWidget(widgetIndex, parent->fullWidgetCount() - 1));
    QObject::connect(moveFarRight, &QAction::triggered, parent, [this, parent, widgetIndex] {
        m_layoutHistory->push(
            new MoveWidgetCommand(m_self, m_widgetProvider, parent, widgetIndex, parent->fullWidgetCount() - 1));
    });
    menu->addAction(moveFarRight);

    return moveLeft->isEnabled() || moveRight->isEnabled() || moveFarLeft->isEnabled() || moveFarRight->isEnabled();
}

void EditableLayoutPrivate::setupPasteAction(QMenu* menu, FyWidget* prev, FyWidget* current)
{
    if(auto* container = qobject_cast<WidgetContainer*>(current)) {
        if(container->canAddWidget()) {
            auto* pasteInsert     = new QAction(EditableLayout::tr("Paste (Insert)"), menu);
            const int insertIndex = current == prev ? container->widgetCount() : container->widgetIndex(prev->id()) + 1;
            QObject::connect(pasteInsert, &QAction::triggered, container, [this, container, insertIndex] {
                m_layoutHistory->push(
                    new AddWidgetCommand(m_self, m_widgetProvider, container, m_widgetClipboard, insertIndex));
            });
            menu->addAction(pasteInsert);
        }
    }
}

void EditableLayoutPrivate::setupContextMenu(FyWidget* widget, QMenu* menu)
{
    if(!widget || !menu) {
        return;
    }

    FyWidget* prevWidget    = widget;
    FyWidget* currentWidget = widget;
    int level               = m_settings->value<Settings::Gui::Internal::EditingMenuLevels>();

    while(level > 0 && currentWidget && currentWidget != m_root) {
        const bool isDummy = qobject_cast<Dummy*>(currentWidget);

        menu->addAction(new MenuHeaderAction(isDummy ? QStringLiteral("Widget") : currentWidget->name(), menu));

        currentWidget->layoutEditingMenu(menu);

        auto* parent = qobject_cast<WidgetContainer*>(currentWidget->findParent());

        setupAddWidgetMenu(menu, parent, prevWidget, currentWidget);

        if(!isDummy) {
            auto* changeMenu = new QMenu(EditableLayout::tr("&Replace"), menu);
            m_widgetProvider->setupReplaceWidgetMenu(m_self, changeMenu, parent, currentWidget->id());
            menu->addMenu(changeMenu);

            if(parent) {
                auto* splitMenu = new QMenu(EditableLayout::tr("&Split"), menu);
                m_widgetProvider->setupSplitWidgetMenu(m_self, splitMenu, parent, currentWidget->id());
                menu->addMenu(splitMenu);
            }

            auto* copy = new QAction(EditableLayout::tr("Copy"), menu);
            copy->setEnabled(m_widgetProvider->canCreateWidget(currentWidget->layoutName()));
            QObject::connect(copy, &QAction::triggered, currentWidget, [this, currentWidget] {
                m_widgetClipboard = EditableLayout::saveBaseWidget(currentWidget);
            });
            menu->addAction(copy);
        }

        if(!m_widgetClipboard.isEmpty() && m_widgetProvider->canCreateWidget(m_widgetClipboard.constBegin().key())) {
            if(parent && !isDummy) {
                setupPasteAction(menu, prevWidget, currentWidget);
            }

            auto* paste = new QAction(EditableLayout::tr("Paste (Replace)"), menu);
            QObject::connect(paste, &QAction::triggered, currentWidget, [this, parent, currentWidget] {
                m_layoutHistory->push(
                    new ReplaceWidgetCommand(m_self, m_widgetProvider, parent, m_widgetClipboard, currentWidget->id()));
            });
            menu->addAction(paste);
        }

        if(parent && parent != m_root) {
            auto* moveMenu = new QMenu(EditableLayout::tr("&Move"), menu);
            moveMenu->setEnabled(setupMoveWidgetMenu(moveMenu, parent, currentWidget));
            menu->addMenu(moveMenu);
        }

        if(!isDummy || parent->widgetCount() > 1) {
            auto* remove = new QAction(EditableLayout::tr("Remove"), menu);
            QObject::connect(remove, &QAction::triggered, currentWidget, [this, parent, currentWidget] {
                m_layoutHistory->push(new RemoveWidgetCommand(m_self, m_widgetProvider, parent, currentWidget->id()));
            });
            menu->addAction(remove);
        }

        if(isDummy) {
            // Don't show parent menus
            return;
        }

        prevWidget    = currentWidget;
        currentWidget = parent;
        --level;
    }
}

WidgetList EditableLayoutPrivate::findAllWidgets() const
{
    if(!m_root) {
        return {};
    }

    WidgetList widgets;

    std::stack<FyWidget*> widgetsToCheck;
    widgetsToCheck.push(m_root);

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

EditableLayout::EditableLayout(ActionManager* actionManager, WidgetProvider* widgetProvider,
                               LayoutProvider* layoutProvider, SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<EditableLayoutPrivate>(this, actionManager, widgetProvider, layoutProvider, settings)}
{
    setObjectName(QStringLiteral("EditableLayout"));
}

EditableLayout::~EditableLayout()
{
    p->m_settings->set<Settings::Gui::LayoutEditing>(false);
}

void EditableLayout::initialise()
{
    auto* editMenu = p->m_actionManager->actionContainer(Constants::Menus::Edit);

    auto* undo    = new QAction(tr("Undo"), this);
    auto* undoCmd = p->m_actionManager->registerAction(undo, Constants::Actions::Undo, p->m_editingContext->context());
    undoCmd->setDefaultShortcut(QKeySequence::Undo);
    editMenu->addAction(undoCmd);
    QObject::connect(undo, &QAction::triggered, this, [this]() { p->m_layoutHistory->undo(); });
    QObject::connect(p->m_layoutHistory, &QUndoStack::canUndoChanged, this,
                     [undo](bool canUndo) { undo->setEnabled(canUndo); });
    undo->setEnabled(p->m_layoutHistory->canUndo());

    auto* redo    = new QAction(tr("Redo"), this);
    auto* redoCmd = p->m_actionManager->registerAction(redo, Constants::Actions::Redo, p->m_editingContext->context());
    redoCmd->setDefaultShortcut(QKeySequence::Redo);
    editMenu->addAction(redoCmd);
    QObject::connect(redo, &QAction::triggered, this, [this]() { p->m_layoutHistory->redo(); });
    QObject::connect(p->m_layoutHistory, &QUndoStack::canRedoChanged, this,
                     [redo](bool canRedo) { redo->setEnabled(canRedo); });
    redo->setEnabled(p->m_layoutHistory->canRedo());

    changeLayout(p->m_layoutProvider->currentLayout());
}

FyLayout EditableLayout::saveCurrentToLayout(const QString& name)
{
    QJsonObject root;
    QJsonArray array;

    if(p->m_root->widget()) {
        p->m_root->widget()->saveLayout(array);
    }
    else {
        array.append(QJsonObject{});
    }

    QString layoutName{name};
    if(layoutName.isEmpty()) {
        layoutName = QStringLiteral("Layout %1").arg(p->m_layoutProvider->layouts().size());
    }

    root[QStringLiteral("Name")]    = layoutName;
    root[QStringLiteral("Version")] = LayoutVersion;
    root[QStringLiteral("Widgets")] = array;

    const QByteArray json = QJsonDocument(root).toJson();

    return FyLayout{json};
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
    if(!p->m_layoutEditing || event->type() != QEvent::MouseButtonPress) {
        return QWidget::eventFilter(watched, event);
    }

    if(event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);

        if(!mouseEvent || mouseEvent->button() != Qt::RightButton
           || (p->m_editingMenu && !p->m_editingMenu->isHidden())) {
            return QWidget::eventFilter(watched, event);
        }

        p->m_editingMenu = new QMenu(this);
        p->m_editingMenu->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(p->m_editingMenu, &QMenu::aboutToHide, this, [this]() { p->hideOverlay(); });

        const QPoint pos = mouseEvent->globalPosition().toPoint();
        QWidget* widget  = childAt(mapFromGlobal(pos));
        FyWidget* child  = findSplitterChild(widget);

        if(!child) {
            return QWidget::eventFilter(watched, event);
        }

        p->setupContextMenu(child, p->m_editingMenu);
        p->showOverlay(child);
        p->m_editingMenu->popup(pos);
    }

    event->accept();
    return true;
}

void EditableLayout::changeLayout(const FyLayout& layout)
{
    p->m_root->reset();

    if(!loadLayout(layout)) {
        p->setupDefault();
    }

    if(p->m_root->widgetCount() == 0) {
        p->m_settings->set<Settings::Gui::LayoutEditing>(true);
    }
    else {
        p->m_settings->set<Settings::Gui::LayoutEditing>(false);
    }
}

void EditableLayout::saveLayout()
{
    const auto layout = saveCurrentToLayout(QStringLiteral("Default"));
    if(layout.isValid()) {
        p->m_layoutProvider->changeLayout(layout);
        p->m_layoutProvider->saveCurrentLayout();
    }
}

bool EditableLayout::loadLayout(const FyLayout& layout)
{
    if(!layout.isValid()) {
        return false;
    }

    const auto json = layout.json();

    p->m_layoutHistory->clear();
    layout.loadWindowSize();

    if(!json.contains(QStringLiteral("Widgets"))) {
        return false;
    }

    if(!json.value(QStringLiteral("Widgets")).isArray()) {
        return false;
    }

    const auto rootWidgets = json.value(QStringLiteral("Widgets")).toArray();

    if(rootWidgets.empty() || !rootWidgets.cbegin()->isObject()) {
        return false;
    }

    const auto rootObject = rootWidgets.cbegin()->toObject();
    const auto widgetKey  = rootObject.constBegin().key();
    auto* topWidget       = p->m_widgetProvider->createWidget(widgetKey);

    if(!topWidget) {
        return false;
    }

    p->m_root->addWidget(topWidget);

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
    auto* quickSetup = new QuickSetupDialog(p->m_layoutProvider, this);
    quickSetup->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(quickSetup, &QuickSetupDialog::layoutChanged, this, &EditableLayout::changeLayout);
    quickSetup->show();
}
} // namespace Fooyin

#include "editablelayout.moc"
#include "gui/moc_editablelayout.cpp"
