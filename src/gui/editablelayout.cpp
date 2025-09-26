/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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

#include "editablelayout_p.h"
#include <gui/editablelayout.h>

#include "contextmenuids.h"
#include "dialog/exportlayoutdialog.h"
#include "internalguisettings.h"
#include "layoutcommands.h"
#include "quicksetup/quicksetupdialog.h"
#include "utils/actions/command.h"
#include "widgets/dummy.h"
#include "widgets/menuheader.h"

#include <gui/contextmenuutils.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/layoutprovider.h>
#include <gui/widgetprovider.h>
#include <gui/widgets/overlaywidget.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/id.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>
#include <QStyle>
#include <QUndoStack>

using namespace Qt::StringLiterals;

constexpr auto LayoutVersion = 1;

namespace Fooyin {
namespace {
FyWidget* findSplitterChild(QWidget* widget, const QPoint& pos)
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

    auto* fyWidget = qobject_cast<FyWidget*>(child);

    if(auto* container = qobject_cast<WidgetContainer*>(fyWidget)) {
        const QPoint widgetPos = container->mapFromGlobal(pos);
        if(auto* childWidget = container->widgetAtPosition(widgetPos)) {
            return childWidget;
        }
    }

    return fyWidget;
}

void preserveWidgetIds(QJsonObject& widgetObject, FyWidget* widget)
{
    if(!widget || widgetObject.empty() || !widgetObject.constBegin()->isObject()) {
        return;
    }

    const QString widgetName = widgetObject.constBegin().key();
    QJsonObject widgetData   = widgetObject.value(widgetName).toObject();
    widgetData["ID"_L1]      = widget->id().name();

    if(auto* container = qobject_cast<WidgetContainer*>(widget)) {
        QJsonArray children = widgetData.value("Widgets"_L1).toArray();
        const auto widgets  = container->widgets();

        for(qsizetype i{0}; i < children.size() && std::cmp_less(i, widgets.size()); ++i) {
            if(children.at(i).isObject()) {
                QJsonObject childObject = children.at(i).toObject();
                preserveWidgetIds(childObject, widgets.at(i));
                children[i] = childObject;
            }
        }

        widgetData["Widgets"_L1] = children;
    }

    widgetObject[widgetName] = widgetData;
}
} // namespace

RootContainer::RootContainer(WidgetProvider* provider, SettingsManager* settings, QWidget* parent)
    : WidgetContainer{provider, settings, parent}
    , m_settings{settings}
    , m_layout{new QVBoxLayout(this)}
    , m_widget{new Dummy(m_settings, this)}
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_widget);
}

void RootContainer::reset()
{
    delete m_widget;
    m_widget = new Dummy(m_settings, this);
    m_layout->addWidget(m_widget);
}

FyWidget* RootContainer::widget() const
{
    return m_widget;
}

QString RootContainer::name() const
{
    return u"Root"_s;
}

QString RootContainer::layoutName() const
{
    return name();
}

bool RootContainer::canAddWidget() const
{
    return !m_widget || qobject_cast<Dummy*>(m_widget);
}

bool RootContainer::canMoveWidget(int /*index*/, int /*newIndex*/) const
{
    return false;
}

int RootContainer::widgetIndex(const Id& id) const
{
    return m_widget && m_widget->id() == id ? 0 : -1;
}

FyWidget* RootContainer::widgetAtId(const Id& id) const
{
    return m_widget && m_widget->id() == id ? m_widget : nullptr;
}

FyWidget* RootContainer::widgetAtIndex(int index) const
{
    return index == 0 ? m_widget : nullptr;
}

int RootContainer::widgetCount() const
{
    return m_widget && !qobject_cast<Dummy*>(m_widget) ? 1 : 0;
}

WidgetList RootContainer::widgets() const
{
    if(m_widget) {
        return {m_widget};
    }

    return {};
}

int RootContainer::addWidget(FyWidget* widget)
{
    insertWidget(0, widget);
    return 0;
}

void RootContainer::insertWidget(int index, FyWidget* widget)
{
    if(index != 0) {
        return;
    }

    widget->setParent(this);
    delete m_widget;
    m_widget = widget;
    m_layout->insertWidget(0, m_widget);
}

void RootContainer::removeWidget(int index)
{
    if(index == 0) {
        reset();
    }
}

void RootContainer::replaceWidget(int index, FyWidget* newWidget)
{
    insertWidget(index, newWidget);
}

void RootContainer::moveWidget(int /*index*/, int /*newIndex*/) { }

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
    , m_layoutEditing{false}
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

        m_self->saveLayout();
    }
}

void EditableLayoutPrivate::changeLayout(const FyLayout& layout) const
{
    m_root->reset();

    if(m_self->loadLayout(layout)) {
        m_layoutProvider->changeLayout(layout);
    }
    else {
        setupDefault();
    }

    if(m_root->widgetCount() == 0) {
        m_settings->set<Settings::Gui::LayoutEditing>(true);
    }
    else {
        m_settings->set<Settings::Gui::LayoutEditing>(false);
    }

    Q_EMIT m_self->layoutChanged();
}

void EditableLayoutPrivate::showOverlay(FyWidget* widget) const
{
    if(auto* container = qobject_cast<WidgetContainer*>(widget->findParent())) {
        const QRect widgetGeometry = container->widgetGeometry(widget);
        if(widgetGeometry.isValid()) {
            m_overlay->setGeometry(QRect{container->mapTo(m_self, widgetGeometry.topLeft()), widgetGeometry.size()});
            m_overlay->raise();
            m_overlay->show();
            return;
        }
    }

    m_overlay->setGeometry(QRect{widget->mapTo(m_self, QPoint{}), widget->size()});
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
    const auto addWidgetMenu = [this](QMenu* targetMenu, WidgetContainer* container, int index) {
        m_widgetProvider->setupAddWidgetMenu(m_self, targetMenu, container, index);
        return !targetMenu->isEmpty();
    };

    if(auto* container = qobject_cast<WidgetContainer*>(current)) {
        if(current != prev) {
            if(container->canAddWidget()) {
                const int previousIndex = container->widgetIndex(prev->id());

                auto* beforeMenu = new QMenu(EditableLayout::tr("Insert &before"), menu);
                if(addWidgetMenu(beforeMenu, container, previousIndex)) {
                    menu->addMenu(beforeMenu);
                }

                auto* afterMenu = new QMenu(EditableLayout::tr("Insert &after"), menu);
                if(addWidgetMenu(afterMenu, container, previousIndex + 1)) {
                    menu->addMenu(afterMenu);
                }
            }
        }
        else {
            if(parent && parent->canAddWidget()) {
                const int currentIndex = parent->widgetIndex(current->id());

                auto* beforeMenu = new QMenu(EditableLayout::tr("Insert &before"), menu);
                if(addWidgetMenu(beforeMenu, parent, currentIndex)) {
                    menu->addMenu(beforeMenu);
                }

                auto* afterMenu = new QMenu(EditableLayout::tr("Insert &after"), menu);
                if(addWidgetMenu(afterMenu, parent, currentIndex + 1)) {
                    menu->addMenu(afterMenu);
                }
            }

            if(container->canAddWidget()) {
                auto* insideMenu      = new QMenu(EditableLayout::tr("Insert &inside"), menu);
                const int insertIndex = container->widgetCount();
                if(addWidgetMenu(insideMenu, container, insertIndex)) {
                    menu->addMenu(insideMenu);
                }
            }
        }
    }
    else if(qobject_cast<Dummy*>(current)) {
        if(!parent || !parent->canAddWidget()) {
            return;
        }

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

    auto* moveLeft = new QAction(horizontal ? EditableLayout::tr("&Left") : EditableLayout::tr("&Up"), menu);
    moveLeft->setEnabled(parent->canMoveWidget(widgetIndex, widgetIndex - 1));
    QObject::connect(moveLeft, &QAction::triggered, parent, [this, parent, widgetIndex] {
        m_layoutHistory->push(new MoveWidgetCommand(m_self, m_widgetProvider, parent, widgetIndex, widgetIndex - 1));
    });
    menu->addAction(moveLeft);

    auto* moveRight = new QAction(horizontal ? EditableLayout::tr("&Right") : EditableLayout::tr("&Down"), menu);
    moveRight->setEnabled(parent->canMoveWidget(widgetIndex, widgetIndex + 1));
    QObject::connect(moveRight, &QAction::triggered, parent, [this, parent, widgetIndex] {
        m_layoutHistory->push(new MoveWidgetCommand(m_self, m_widgetProvider, parent, widgetIndex, widgetIndex + 1));
    });
    menu->addAction(moveRight);

    auto* moveFarLeft = new QAction(horizontal ? EditableLayout::tr("Far Lef&t") : EditableLayout::tr("&Top"), menu);
    moveFarLeft->setEnabled(parent->canMoveWidget(widgetIndex, 0));
    QObject::connect(moveFarLeft, &QAction::triggered, parent, [this, parent, widgetIndex] {
        m_layoutHistory->push(new MoveWidgetCommand(m_self, m_widgetProvider, parent, widgetIndex, 0));
    });
    menu->addAction(moveFarLeft);

    auto* moveFarRight
        = new QAction(horizontal ? EditableLayout::tr("Far Rig&ht") : EditableLayout::tr("&Bottom"), menu);
    moveFarRight->setEnabled(parent->canMoveWidget(widgetIndex, parent->fullWidgetCount() - 1));
    QObject::connect(moveFarRight, &QAction::triggered, parent, [this, parent, widgetIndex] {
        m_layoutHistory->push(
            new MoveWidgetCommand(m_self, m_widgetProvider, parent, widgetIndex, parent->fullWidgetCount() - 1));
    });
    menu->addAction(moveFarRight);

    return moveLeft->isEnabled() || moveRight->isEnabled() || moveFarLeft->isEnabled() || moveFarRight->isEnabled();
}

void EditableLayoutPrivate::setupUnsplitAction(QMenu* menu, WidgetContainer* parent, FyWidget* current) const
{
    if(!parent || parent == m_root || !current || qobject_cast<Dummy*>(current) || parent->widgetCount() != 1) {
        return;
    }

    auto* grandParent = qobject_cast<WidgetContainer*>(parent->findParent());
    if(!grandParent) {
        return;
    }

    auto* unsplit = new QAction(EditableLayout::tr("Remove spli&t"), menu);
    QObject::connect(unsplit, &QAction::triggered, current, [this, grandParent, parent] {
        m_layoutHistory->push(new CollapseContainerCommand(m_self, m_widgetProvider, grandParent, parent->id()));
    });
    menu->addAction(unsplit);
}

void EditableLayoutPrivate::setupPasteMenu(QMenu* menu, WidgetContainer* parent, FyWidget* prev, FyWidget* current,
                                           bool isDummy)
{
    if(m_widgetClipboard.isEmpty() || !m_widgetProvider->canCreateWidget(m_widgetClipboard.constBegin().key())) {
        return;
    }

    auto* pasteMenu = new QMenu(EditableLayout::tr("&Paste"), menu);

    if(parent) {
        auto* replace = new QAction(EditableLayout::tr("Rep&lace"), pasteMenu);
        QObject::connect(replace, &QAction::triggered, current, [this, parent, current] {
            m_layoutHistory->push(
                new ReplaceWidgetCommand(m_self, m_widgetProvider, parent, m_widgetClipboard, current->id()));
        });
        pasteMenu->addAction(replace);
    }

    if(current == prev && parent && !isDummy && parent->canAddWidget()) {
        const int currentIndex = parent->widgetIndex(current->id());

        auto* before = new QAction(EditableLayout::tr("&Before"), pasteMenu);
        QObject::connect(before, &QAction::triggered, parent, [this, parent, currentIndex] {
            m_layoutHistory->push(
                new AddWidgetCommand(m_self, m_widgetProvider, parent, m_widgetClipboard, currentIndex));
        });
        pasteMenu->addAction(before);

        auto* after = new QAction(EditableLayout::tr("&After"), pasteMenu);
        QObject::connect(after, &QAction::triggered, parent, [this, parent, currentIndex] {
            m_layoutHistory->push(
                new AddWidgetCommand(m_self, m_widgetProvider, parent, m_widgetClipboard, currentIndex + 1));
        });
        pasteMenu->addAction(after);
    }

    if(auto* container = qobject_cast<WidgetContainer*>(current)) {
        if(container->canAddWidget()) {
            const int insertIndex = current == prev ? container->widgetCount() : container->widgetIndex(prev->id()) + 1;
            auto* inside          = new QAction(EditableLayout::tr("&Inside"), pasteMenu);
            QObject::connect(inside, &QAction::triggered, container, [this, container, insertIndex] {
                m_layoutHistory->push(
                    new AddWidgetCommand(m_self, m_widgetProvider, container, m_widgetClipboard, insertIndex));
            });
            pasteMenu->addAction(inside);
        }
    }

    if(!pasteMenu->isEmpty()) {
        menu->addMenu(pasteMenu);
    }
}

void EditableLayoutPrivate::setupContextMenu(FyWidget* widget, QMenu* menu)
{
    using namespace Settings::Gui::Internal;

    if(!widget || !menu) {
        return;
    }

    FyWidget* prevWidget{widget};
    FyWidget* currentWidget{widget};
    int level = m_settings->value<EditingMenuLevels>();

    while(level > 0 && currentWidget && currentWidget != m_root) {
        const bool isDummy = qobject_cast<Dummy*>(currentWidget);

        const QString header = currentWidget == widget ? (isDummy ? u"Widget"_s : currentWidget->name())
                                                       : EditableLayout::tr("Parent: %1").arg(currentWidget->name());
        menu->addAction(new MenuHeaderAction(header, menu));

        auto* parent = qobject_cast<WidgetContainer*>(currentWidget->findParent());

        ContextMenuUtils::renderStaticContextMenu(
            menu, ContextMenuIds::LayoutEditing::DefaultItems, m_settings->value<ContextMenuLayoutEditingLayout>(),
            m_settings->value<ContextMenuLayoutEditingDisabledSections>(),
            [&](const auto& id, QMenu* targetMenu, const auto& sectionEnabled) {
                if(id == QLatin1StringView{ContextMenuIds::LayoutEditing::WidgetActions}) {
                    if(sectionEnabled(ContextMenuIds::LayoutEditing::WidgetActions)) {
                        currentWidget->layoutEditingMenu(targetMenu);
                    }
                    return;
                }
                if(id == QLatin1StringView{ContextMenuIds::LayoutEditing::Replace}) {
                    if(!isDummy && sectionEnabled(ContextMenuIds::LayoutEditing::Replace)) {
                        auto* changeMenu = new QMenu(EditableLayout::tr("R&eplace"), targetMenu);
                        m_widgetProvider->setupReplaceWidgetMenu(m_self, changeMenu, parent, currentWidget->id());
                        targetMenu->addMenu(changeMenu);
                    }
                    return;
                }
                if(id == QLatin1StringView{ContextMenuIds::LayoutEditing::Split}) {
                    if(!isDummy && parent && sectionEnabled(ContextMenuIds::LayoutEditing::Split)) {
                        auto* splitMenu = new QMenu(EditableLayout::tr("&Split"), targetMenu);
                        m_widgetProvider->setupSplitWidgetMenu(m_self, splitMenu, parent, currentWidget->id());
                        targetMenu->addMenu(splitMenu);
                    }
                    return;
                }
                if(id == QLatin1StringView{ContextMenuIds::LayoutEditing::RemoveSplit}) {
                    if(!isDummy && sectionEnabled(ContextMenuIds::LayoutEditing::RemoveSplit)) {
                        setupUnsplitAction(targetMenu, parent, currentWidget);
                    }
                    return;
                }
                if(id == QLatin1StringView{ContextMenuIds::LayoutEditing::Copy}) {
                    if(!isDummy && sectionEnabled(ContextMenuIds::LayoutEditing::Copy)) {
                        auto* copy = new QAction(EditableLayout::tr("&Copy"), targetMenu);
                        copy->setEnabled(m_widgetProvider->canCreateWidget(currentWidget->layoutName()));
                        QObject::connect(copy, &QAction::triggered, currentWidget, [this, currentWidget] {
                            m_widgetClipboard = EditableLayout::saveBaseWidget(currentWidget);
                        });
                        targetMenu->addAction(copy);
                    }
                    return;
                }
                if(id == QLatin1StringView{ContextMenuIds::LayoutEditing::Paste}) {
                    if(sectionEnabled(ContextMenuIds::LayoutEditing::Paste)) {
                        setupPasteMenu(targetMenu, parent, prevWidget, currentWidget, isDummy);
                    }
                    return;
                }
                if(id == QLatin1StringView{ContextMenuIds::LayoutEditing::Insert}) {
                    if(sectionEnabled(ContextMenuIds::LayoutEditing::Insert)) {
                        setupAddWidgetMenu(targetMenu, parent, prevWidget, currentWidget);
                    }
                    return;
                }
                if(id == QLatin1StringView{ContextMenuIds::LayoutEditing::Move}) {
                    if(parent && parent != m_root && sectionEnabled(ContextMenuIds::LayoutEditing::Move)) {
                        auto* moveMenu = new QMenu(EditableLayout::tr("&Move"), targetMenu);
                        if(setupMoveWidgetMenu(moveMenu, parent, currentWidget)) {
                            targetMenu->addMenu(moveMenu);
                        }
                    }
                    return;
                }
                if(id == QLatin1StringView{ContextMenuIds::LayoutEditing::Remove}) {
                    if(sectionEnabled(ContextMenuIds::LayoutEditing::Remove)
                       && (!isDummy || (parent && parent->widgetCount() > 1))) {
                        auto* remove = new QAction(EditableLayout::tr("&Remove"), targetMenu);
                        QObject::connect(remove, &QAction::triggered, currentWidget, [this, parent, currentWidget] {
                            m_layoutHistory->push(
                                new RemoveWidgetCommand(m_self, m_widgetProvider, parent, currentWidget->id()));
                        });
                        targetMenu->addAction(remove);
                    }
                }
            });

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
    setObjectName(u"EditableLayout"_s);
}

EditableLayout::~EditableLayout()
{
    p->m_settings->set<Settings::Gui::LayoutEditing>(false);
}

void EditableLayout::initialise()
{
    auto* editMenu = p->m_actionManager->actionContainer(Constants::Menus::Edit);

    const QStringList editCategory{tr("Edit")};

    auto* undo = new QAction(tr("&Undo"), this);
    undo->setStatusTip(tr("Undo the previous layout edit"));
    auto* undoCmd = p->m_actionManager->registerAction(undo, Constants::Actions::Undo, p->m_editingContext->context());
    undoCmd->setCategories(editCategory);
    undoCmd->setDefaultShortcut(QKeySequence::Undo);
    undoCmd->setAttribute(ProxyAction::UpdateText);
    editMenu->addAction(undoCmd);
    QObject::connect(undo, &QAction::triggered, this, [this]() { p->m_layoutHistory->undo(); });
    QObject::connect(p->m_layoutHistory, &QUndoStack::canUndoChanged, this,
                     [undo](bool canUndo) { undo->setEnabled(canUndo); });
    undo->setEnabled(p->m_layoutHistory->canUndo());

    auto* redo = new QAction(tr("&Redo"), this);
    undo->setStatusTip(tr("Redo the previous layout edit"));
    auto* redoCmd = p->m_actionManager->registerAction(redo, Constants::Actions::Redo, p->m_editingContext->context());
    redoCmd->setCategories(editCategory);
    redoCmd->setDefaultShortcut(QKeySequence::Redo);
    redoCmd->setAttribute(ProxyAction::UpdateText);
    editMenu->addAction(redoCmd);
    QObject::connect(redo, &QAction::triggered, this, [this]() { p->m_layoutHistory->redo(); });
    QObject::connect(p->m_layoutHistory, &QUndoStack::canRedoChanged, this,
                     [redo](bool canRedo) { redo->setEnabled(canRedo); });
    redo->setEnabled(p->m_layoutHistory->canRedo());

    p->changeLayout(p->m_layoutProvider->currentLayout());
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
        layoutName = u"Layout %1"_s.arg(p->m_layoutProvider->layouts().size());
    }

    root["Name"_L1]    = layoutName;
    root["Version"_L1] = LayoutVersion;
    root["Widgets"_L1] = array;

    const QByteArray json = QJsonDocument(root).toJson();

    FyLayout layout{json};

    const auto theme = p->m_settings->value<Settings::Gui::Theme>().value<FyTheme>();
    if(theme.isValid()) {
        layout.saveTheme(theme);
    }

    return layout;
}

FyWidget* EditableLayout::root() const
{
    return p->m_root;
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
        FyWidget* child  = findSplitterChild(widget, pos);

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
    p->m_layoutHistory->push(new SwitchLayoutCommand(p.get(), layout));
}

void EditableLayout::saveLayout()
{
    const auto currentLayout = p->m_layoutProvider->currentLayout();
    const QString layoutName = currentLayout.name().isEmpty() ? u"Default"_s : currentLayout.name();
    const auto layout        = saveCurrentToLayout(layoutName);
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

    layout.loadWindowSize();

    if(!json.contains("Widgets"_L1)) {
        return false;
    }

    if(!json.value("Widgets"_L1).isArray()) {
        return false;
    }

    const auto rootWidgets = json.value("Widgets"_L1).toArray();

    if(rootWidgets.empty() || !rootWidgets.cbegin()->isObject()) {
        return false;
    }

    const FyTheme theme = layout.loadTheme();
    if(theme.isValid()) {
        p->m_settings->set<Settings::Gui::Theme>(QVariant::fromValue(theme));
    }

    const auto rootObject = rootWidgets.cbegin()->toObject();
    const auto widgetKey  = rootObject.constBegin().key();
    auto* topWidget       = p->m_widgetProvider->createWidget(widgetKey);

    if(!topWidget) {
        return false;
    }

    p->m_root->addWidget(topWidget);

    const auto optionsIt = rootObject.constBegin();
    if(optionsIt->isObject()) {
        const QJsonObject options = optionsIt->toObject();
        topWidget->loadLayout(options);
    }

    topWidget->finalise();
    return true;
}

void EditableLayout::exportLayout(QWidget* parent)
{
    auto* exportDialog = new ExportLayoutDialog(this, p->m_layoutProvider, p->m_settings, parent);
    exportDialog->setAttribute(Qt::WA_DeleteOnClose);
    exportDialog->show();
}

QJsonObject EditableLayout::saveWidget(FyWidget* widget)
{
    QJsonArray array;

    widget->saveLayout(array);

    if(!array.empty() && array.constBegin()->isObject()) {
        QJsonObject widgetObject = array.constBegin()->toObject();
        preserveWidgetIds(widgetObject, widget);
        return widgetObject;
    }

    return {};
}

QJsonObject EditableLayout::saveBaseWidget(FyWidget* widget)
{
    QJsonArray array;
    LayoutCopyContext context;

    widget->saveCopyLayout(array, context);

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

#include "gui/moc_editablelayout.cpp"
