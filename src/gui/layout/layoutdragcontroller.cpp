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

#include "layoutdragcontroller.h"

#include "editablelayout_p.h"
#include "layoutcommands.h"
#include "layoutdragcard.h"
#include "layoutdragsurface.h"
#include "layoutpaneloverlay.h"
#include "splitters/fysplitter.h"
#include "splitters/splitterwidget.h"
#include "splitters/tabstackwidget.h"
#include "widgetpalette.h"
#include "widgets/dummy.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/iconloader.h>
#include <gui/layout/editablelayout.h>
#include <gui/widgetprovider.h>
#include <gui/widgets/editabletabbar.h>
#include <gui/widgets/editabletabwidget.h>
#include <gui/widgets/overlaywidget.h>
#include <gui/widgets/singletabbedwidget.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
#include <QRegion>
#include <QScopedValueRollback>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabBar>
#include <QTimerEvent>
#include <QToolButton>
#include <QUndoStack>

using namespace Qt::StringLiterals;

constexpr auto LayoutDragMime = "application/x-fooyin-layout-widget";

namespace Fooyin {
namespace {
QRect relativeGeometry(const QWidget* widget, const QWidget* root)
{
    return {widget->mapTo(root, QPoint{}), widget->size()};
}

class PaletteEdgeHint : public QWidget
{
public:
    explicit PaletteEdgeHint(QWidget* parent)
        : QWidget{parent}
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        hide();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);

        const auto position = mapFromGlobal(QCursor::pos());
        const bool nearby   = window()->isActiveWindow() && rect().adjusted(-24, -16, 0, 16).contains(position);
        auto highlight      = palette().color(QPalette::Highlight);

        QRadialGradient glow{QPointF{}, 1.0};
        highlight.setAlpha(nearby ? 180 : 110);
        glow.setColorAt(0.0, highlight);
        highlight.setAlpha(nearby ? 80 : 45);
        glow.setColorAt(0.45, highlight);
        highlight.setAlpha(0);
        glow.setColorAt(1.0, highlight);

        painter.translate(width(), height() / 2.0);
        painter.scale(width(), height() / 2.0);
        painter.fillRect(QRectF{-1.0, -1.0, 1.0, 2.0}, glow);
    }
};

class LayoutBreadcrumb : public QScrollArea
{
public:
    using QScrollArea::QScrollArea;

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter{viewport()};

        auto background = parentWidget()->palette().color(QPalette::Window);
        background.setAlpha(255);

        painter.fillRect(viewport()->rect(), background);
    }
};
} // namespace

LayoutDragController::LayoutDragController(EditableLayout* layout, QUndoStack* history, WidgetProvider* provider,
                                           SettingsManager* settings)
    : QObject{layout}
    , m_layout{layout}
    , m_history{history}
    , m_provider{provider}
    , m_settings{settings}
    , m_surface{new LayoutDragSurface(this, layout)}
    , m_sourceOverlay{new OverlayWidget(OverlayWidget::Label, layout)}
    , m_palette{new WidgetPalette(provider, settings, layout)}
    , m_paletteHint{new PaletteEdgeHint(layout)}
    , m_dragCard{new LayoutDragCard(layout)}
    , m_selectionControl{new OverlayWidget(layout)}
    , m_selectionBar{new LayoutBreadcrumb(m_selectionControl)}
    , m_selectionOutline{new LayoutPanelOverlay(layout)}
    , m_selectionGrip{new LayoutStackGrip(layout)}
    , m_paletteWidth{std::clamp(m_palette->width(), 160, 420)}
    , m_paletteExpansion{0}
    , m_paletteDocked{false}
    , m_updatingPalette{false}
    , m_paletteHovered{false}
    , m_updatePending{false}
    , m_editing{false}
    , m_updatingPanels{false}
{
    m_selectionBar->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_selectionBar->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_selectionBar->setMinimumSize(0, 0);
    m_selectionBar->setFrameShape(QFrame::NoFrame);

    auto* crumbs = new QWidget(m_selectionBar);
    auto* row    = new QHBoxLayout(crumbs);
    row->setContentsMargins(2, 2, 2, 2);
    row->setSpacing(2);

    m_selectionBar->setWidget(crumbs);
    m_selectionControl->layout()->setSizeConstraint(QLayout::SetNoConstraint);
    m_selectionControl->addWidget(m_selectionBar);

    auto* shadow = new QGraphicsDropShadowEffect(m_selectionControl);
    shadow->setBlurRadius(30);
    shadow->setColor(Qt::black);
    shadow->setOffset(1, 1);

    m_selectionControl->setGraphicsEffect(shadow);
    m_selectionControl->hide();

    m_selectionOutline->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_selectionOutline->setHovered(true);
    m_selectionOutline->hide();

    auto* gripShadow = new QGraphicsDropShadowEffect(m_selectionGrip);
    gripShadow->setBlurRadius(30);
    gripShadow->setColor(Qt::black);
    gripShadow->setOffset(1, 1);

    m_selectionGrip->setFixedSize(40, 40);
    m_selectionGrip->setGraphicsEffect(gripShadow);
    m_selectionGrip->hide();

    m_sourceOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_sourceOverlay->layout()->setSizeConstraint(QLayout::SetNoConstraint);
    m_sourceOverlay->layout()->setContentsMargins(4, 2, 4, 2);
    m_sourceOverlay->label()->setWordWrap(false);
    m_sourceOverlay->label()->setTextFormat(Qt::PlainText);
    m_sourceOverlay->label()->setContentsMargins(4, 2, 4, 2);

    m_palette->hide();

    QObject::connect(history, &QUndoStack::indexChanged, this, [this] {
        if(m_editing) {
            m_palette->refresh();
        }
    });
    QObject::connect(m_palette, &WidgetPalette::widgetDragRequested, this, &LayoutDragController::startPaletteDrag);
    QObject::connect(m_selectionGrip, &LayoutPanelOverlay::dragRequested, this, [this] { startDrag(m_selected); });

    updateSelectionStyle();
    m_settings->subscribe<Settings::Gui::ResolvedAppStyle>(this, &LayoutDragController::updateSelectionStyle);
    m_settings->subscribe<Settings::Gui::DockWidgetPalette>(this, &LayoutDragController::scheduleUpdate);
}

LayoutDragController::~LayoutDragController()
{
    qApp->removeEventFilter(this);
    m_editing = false;
    cancelDrag();

    for(auto& panel : m_panels) {
        resetStackGrip(panel);
        delete panel.grip;
        delete panel.outline;
        delete panel.emptyArea;
        delete panel.overlay;
    }
    delete m_surface;
    delete m_sourceOverlay;
    delete m_palette;
    delete m_paletteHint;
    delete m_dragCard;
    delete m_selectionControl;
    delete m_selectionGrip;
    delete m_selectionOutline;
}

void LayoutDragController::setEditing(bool editing)
{
    m_editing = editing;

    if(editing) {
        m_palettePollTimer.start(100, this);
        m_palette->refresh();
        qApp->installEventFilter(this);
        updatePanels();
    }
    else {
        m_palettePollTimer.stop();
        m_paletteHoverTimer.stop();
        m_paletteHovered = false;
        m_paletteHint->hide();
        selectWidget(nullptr);
        m_sourceOverlay->hide();
        m_dragCard->hide();
        m_tabHoverTimer.stop();
        hidePalette();
        qApp->removeEventFilter(this);
        cancelDrag();
        m_surface->hide();

        for(auto& panel : m_panels) {
            resetStackGrip(panel);
            panel.overlay->hide();
        }
    }
}

WidgetPalette* LayoutDragController::palette() const
{
    return m_palette;
}

void LayoutDragController::showPalette()
{
    if(!m_editing) {
        return;
    }

    m_palette->refresh();
    m_paletteHoverTimer.stop();
    m_palette->show();
    positionPalette();
    m_palette->raise();
    m_palette->findChild<QLineEdit*>()->setFocus();
}

bool LayoutDragController::eventFilter(QObject* watched, QEvent* event)
{
    const auto type          = event->type();
    const bool escapePressed = type == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape;

    if(m_drag
       && (escapePressed
           || (type == QEvent::MouseButtonPress && static_cast<QMouseEvent*>(event)->button() == Qt::RightButton))) {
        cancelDrag();
        if(escapePressed) {
            selectWidget(nullptr);
        }
        event->accept();
        return true;
    }

    if(m_drag && watched == m_surface) {
        if(type == QEvent::DragLeave) {
            m_surface->reset();
            m_tabHoverTimer.stop();
            m_drag->hoverTabs = nullptr;
            m_drag->hoverPage = nullptr;
            event->accept();
            return true;
        }

        if(type == QEvent::DragEnter || type == QEvent::DragMove || type == QEvent::Drop) {
            auto* drop = static_cast<QDropEvent*>(event);
            if(drop->source() != m_layout || !drop->mimeData()->hasFormat(QLatin1StringView{LayoutDragMime})) {
                drop->ignore();
                return true;
            }

            m_surface->updateTarget(m_surface->mapTo(m_layout, drop->position()), drop->modifiers());

            bool acceptDrop;
            if(type == QEvent::Drop) {
                m_drag->dropCommand = m_surface->dropCommand(m_drag->position, drop->modifiers());
                acceptDrop          = static_cast<bool>(m_drag->dropCommand);
            }
            else {
                acceptDrop = type == QEvent::DragEnter || m_surface->hasValidTarget();
            }

            if(acceptDrop) {
                drop->setDropAction(Qt::MoveAction);
                drop->accept();
            }
            else {
                drop->ignore();
            }

            return true;
        }
    }

    const auto* widget = qobject_cast<QWidget*>(watched);
    if(widget == m_palette && type == QEvent::Hide && m_palette->isHidden() && !m_updatingPalette) {
        hidePalette();
    }

    if(!m_editing) {
        return QObject::eventFilter(watched, event);
    }

    if(escapePressed && m_palette->isVisible() && !m_paletteDocked) {
        const QScopedValueRollback changing{m_updatingPalette, true};
        m_paletteHoverTimer.stop();
        m_palette->hide();
        m_layout->setFocus();
        return true;
    }

    if(type == QEvent::MouseMove && widget && widget->window() == m_layout->window()) {
        updatePaletteHover();
    }

    if(widget == m_layout->window() && type == QEvent::WindowStateChange) {
        scheduleUpdate();
    }

    if(escapePressed && m_selected && widget && widget->window() == m_layout->window()) {
        selectWidget(nullptr);
        event->accept();
        return true;
    }

    if(!m_updatingPanels && widget && (widget == m_layout || m_layout->isAncestorOf(widget)) && widget != m_surface) {
        switch(type) {
            case QEvent::Resize:
            case QEvent::Move:
            case QEvent::Show:
            case QEvent::Hide:
            case QEvent::ChildAdded:
            case QEvent::ChildRemoved:
            case QEvent::LayoutRequest:
                scheduleUpdate();
                break;
            default:
                break;
        }
    }

    return QObject::eventFilter(watched, event);
}

void LayoutDragController::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_palettePollTimer.timerId()) {
        if(hasActiveDrag()) {
            const auto modifiers = QGuiApplication::queryKeyboardModifiers() & Qt::ControlModifier;
            if(std::exchange(m_drag->polledModifiers, modifiers) != modifiers) {
                m_surface->updateTarget(m_drag->position, modifiers);
            }
        }
        updatePaletteHover();
    }
    else if(event->timerId() == m_paletteHoverTimer.timerId()) {
        m_paletteHoverTimer.stop();

        if(!m_editing || m_paletteDocked
           || (m_drag ? m_drag->creationKey.isEmpty() || m_paletteHovered
                      : QApplication::mouseButtons() != Qt::NoButton)) {
            return;
        }

        if(m_paletteHovered && !m_palette->isVisible() && QCursor::pos() != m_paletteHoverPosition) {
            updatePaletteHover();
            return;
        }

        const QScopedValueRollback changing{m_updatingPalette, true};
        m_palette->setVisible(m_paletteHovered);
        scheduleUpdate();
    }
    else if(event->timerId() == m_tabHoverTimer.timerId()) {
        m_tabHoverTimer.stop();

        if(!m_editing || !hasActiveDrag() || !m_drag->hoverTabs || !m_drag->hoverPage) {
            return;
        }

        const auto* bar = m_drag->hoverTabs->tabBar();
        const int index = m_drag->hoverTabs->indexOf(m_drag->hoverPage);
        if(index >= 0 && bar->isVisibleTo(m_layout) && bar->isTabEnabled(index)
           && bar->tabAt(bar->mapFrom(m_layout, m_drag->position).toPoint()) == index) {
            m_drag->hoverTabs->setCurrentIndex(index);
            updatePanels();
            m_surface->updateTarget(m_drag->position, QGuiApplication::keyboardModifiers());
        }
    }
    else {
        QObject::timerEvent(event);
    }
}

void LayoutDragController::scheduleUpdate()
{
    if(std::exchange(m_updatePending, true)) {
        return;
    }

    QMetaObject::invokeMethod(
        this,
        [this] {
            m_updatePending = false;
            if(m_editing) {
                updatePanels();
            }
        },
        Qt::QueuedConnection);
}

void LayoutDragController::updatePanels()
{
    const QScopedValueRollback updating{m_updatingPanels, true};

    m_selectionBar->setEnabled(!m_drag);
    positionPalette();

    const auto handles = m_layout->findChildren<FySplitterHandle*>();
    for(auto* handle : handles) {
        handle->raise();
    }

    auto* root         = qobject_cast<WidgetContainer*>(m_layout->root());
    const auto widgets = LayoutUtils::layoutWidgets(root);
    for(auto it = m_panels.begin(); it != m_panels.end();) {
        if(!it->widget || std::ranges::find(widgets, it->widget.data()) == widgets.end()) {
            resetStackGrip(*it);
            delete it->grip;
            delete it->outline;
            delete it->emptyArea;
            delete it->overlay;
            it = m_panels.erase(it);
        }
        else {
            ++it;
        }
    }

    for(auto* widget : widgets) {
        if(widget == root || qobject_cast<SplitterWidget*>(widget)
           || (qobject_cast<Dummy*>(widget) && qobject_cast<Dummy*>(widget)->missingName().isEmpty())) {
            continue;
        }

        const auto* parent = qobject_cast<WidgetContainer*>(widget->findParent());
        if(!parent) {
            continue;
        }

        auto it = std::ranges::find_if(m_panels, [widget](const auto& panel) { return panel.widget == widget; });
        if(it == m_panels.end()) {
            const QPointer<FyWidget> guarded{widget};
            auto* tabs     = qobject_cast<TabStackWidget*>(widget)
                               ? widget->findChild<EditableTabWidget*>(QString{}, Qt::FindDirectChildrenOnly)
                               : nullptr;
            auto* selector = qobject_cast<WidgetContainer*>(widget)
                               ? widget->findChild<SingleTabbedWidget*>(QString{}, Qt::FindDirectChildrenOnly)
                               : nullptr;

            auto* outline = new LayoutPanelOverlay(m_layout);
            outline->setAttribute(Qt::WA_TransparentForMouseEvents);

            auto* overlay = new LayoutPanelOverlay(m_layout, outline);
            overlay->setProperty("LayoutWidgetId", widget->id().name());
            overlay->setToolTip(selector ? tr("Move tabs and content")
                                         : (tabs ? tr("Move tab stack") : tr("Drag to move this panel")));

            QObject::connect(overlay, &LayoutPanelOverlay::dragRequested, this, [this, guarded] {
                if(guarded) {
                    startDrag(guarded);
                }
            });
            QObject::connect(overlay, &LayoutPanelOverlay::selectionRequested, this, [this, guarded] {
                if(guarded) {
                    selectWidget(guarded);
                }
            });

            auto* emptyArea = selector ? new QLabel(tr("Drop a widget here"), m_layout) : nullptr;
            if(emptyArea) {
                emptyArea->setAlignment(Qt::AlignCenter);
                emptyArea->setAttribute(Qt::WA_TransparentForMouseEvents);
            }

            m_panels.push_back({.widget           = widget,
                                .overlay          = overlay,
                                .tabs             = tabs,
                                .grip             = nullptr,
                                .outline          = outline,
                                .maximumBarHeight = tabs ? tabs->tabBar()->maximumHeight() : 0,
                                .selector         = selector,
                                .emptyArea        = emptyArea});
            it = std::prev(m_panels.end());
        }

        const QRect rect = relativeGeometry(widget, m_layout).intersected(m_layout->rect());
        it->overlay->setGeometry(rect);
        if(it->tabs || it->selector) {
            if(!it->grip) {
                createStackGrip(*it);
            }
            updateStackOverlay(*it, rect);
            continue;
        }

        QRegion mask{it->overlay->rect()};
        const auto widgetChildren = widget->findChildren<QWidget*>();
        for(auto* child : widgetChildren) {
            if(child->isVisibleTo(widget) && qobject_cast<QTabBar*>(child)) {
                mask -= relativeGeometry(child, m_layout).translated(-it->overlay->pos());
            }
        }

        for(const auto* handle : handles) {
            if(handle->isVisibleTo(m_layout)) {
                mask -= relativeGeometry(handle, m_layout).translated(-it->overlay->pos());
            }
        }

        it->overlay->setMask(mask);
        it->overlay->setVisible(widget->isVisibleTo(m_layout) && !rect.isEmpty() && !mask.isEmpty() && !m_drag);
        it->outline->setGeometry(rect);
        it->outline->setVisible(it->overlay->isVisible());
        it->outline->raise();
        it->overlay->raise();
    }

    updateSelection();
    if(m_palette->isVisible()) {
        m_palette->raise();
    }

    if(m_drag) {
        if(hasActiveDrag() && m_drag->creationKey.isEmpty() && m_drag->source->isVisibleTo(m_layout)) {
            QRect geometry = relativeGeometry(m_drag->source, m_layout);
            if(const auto* container = qobject_cast<WidgetContainer*>(m_drag->source->findParent())) {
                const auto slot = container->widgetGeometry(m_drag->source);
                if(slot.isValid()) {
                    geometry = {container->mapTo(m_layout, slot.topLeft()), slot.size()};
                }
            }
            geometry = geometry.intersected(m_layout->rect());

            auto* label = m_sourceOverlay->label();

            const auto metrics  = label->fontMetrics();
            const int textWidth = std::max(0, geometry.width() - 16);
            //: %1 is the name of a widget e.g. Moving Playlist
            QString text = tr("Moving %1").arg(m_drag->source->name());
            if(metrics.horizontalAdvance(text) > textWidth) {
                text = m_drag->source->name();
            }
            text = metrics.elidedText(text, Qt::ElideRight, textWidth);

            label->setText(text);
            label->setVisible(geometry.height() >= metrics.height() + 8 && !text.isEmpty());
            label->setMaximumSize(metrics.horizontalAdvance(text) + 8, metrics.height() + 4);

            m_sourceOverlay->setGeometry(geometry);
            m_sourceOverlay->show();
            m_sourceOverlay->raise();
        }
        else {
            m_sourceOverlay->hide();
        }

        m_surface->setGeometry(m_layout->rect());
        m_surface->raise();
    }
}

void LayoutDragController::startDrag(FyWidget* widget)
{
    startDrag(widget, {}, nullptr);
}

void LayoutDragController::startDrag(FyWidget* widget, const QString& creationKey,
                                     std::unique_ptr<RootContainer> staging)
{
    if(!m_editing || m_drag || !widget) {
        return;
    }

    const QPointer<LayoutDragController> guarded{this};

    m_drag.emplace();
    m_drag->source          = widget;
    m_drag->creationKey     = creationKey;
    m_drag->staging         = std::move(staging);
    m_drag->previousFocus   = QApplication::focusWidget();
    m_drag->sourceDestroyed = QObject::connect(widget, &QObject::destroyed, this, &LayoutDragController::cancelDrag);

    const bool creating = !m_drag->creationKey.isEmpty();
    auto snapshot       = !creating && widget->isVisibleTo(m_layout) ? widget->grab() : QPixmap{};

    if(!guarded) {
        return;
    }
    if(!hasActiveDrag()) {
        cancelDrag();
        return;
    }

    m_dragCard->setParent(m_layout->window());
    m_dragCard->setContent(creating ? m_provider->displayName(m_drag->creationKey) : widget->name(), creating,
                           std::move(snapshot));

    qApp->removeEventFilter(this);
    qApp->installEventFilter(this);

    m_surface->setProperty("LayoutWidgetId", widget->id().name());
    m_surface->reset();
    m_surface->setGeometry(m_layout->rect());
    m_surface->show();

    updatePanels();

    if(hasActiveDrag()) {
        m_drag->polledModifiers = QGuiApplication::queryKeyboardModifiers() & Qt::ControlModifier;
        m_surface->updateTarget(m_layout->mapFromGlobal(QCursor::pos()), QGuiApplication::keyboardModifiers());

        auto* drag = new QDrag(m_layout);
        auto* mime = new QMimeData();
        mime->setData(QLatin1String{LayoutDragMime}, widget->id().name().toUtf8());
        drag->setMimeData(mime);
        drag->setPixmap(m_dragCard->grab());
        drag->setHotSpot(QPoint{-12, -18});
        m_drag->nativeDrag = drag;

        const auto result = drag->exec(Qt::MoveAction);
        if(!guarded) {
            return;
        }
        if(hasActiveDrag() && result == Qt::MoveAction) {
            finishDrag(std::move(m_drag->dropCommand));
        }
        else {
            finishDrag();
        }
    }
}

void LayoutDragController::finishDrag()
{
    finishDrag(nullptr);
}

void LayoutDragController::finishDrag(std::unique_ptr<QUndoCommand> command)
{
    if(!m_drag) {
        return;
    }

    const auto session = std::move(*m_drag);
    QObject::disconnect(session.sourceDestroyed);

    m_drag.reset();
    m_surface->reset();
    m_surface->hide();
    m_dragCard->hide();
    m_sourceOverlay->hide();
    m_tabHoverTimer.stop();

    const QPointer<LayoutDragController> guarded{this};

    if(command) {
        m_history->push(command.release());
    }
    if(!guarded) {
        return;
    }
    if(m_editing) {
        if(!session.creationKey.isEmpty()) {
            m_palette->refresh();
        }
        scheduleUpdate();
        if(session.previousFocus && session.previousFocus->isVisible()) {
            session.previousFocus->setFocus(Qt::OtherFocusReason);
        }
    }
}

void LayoutDragController::cancelDrag()
{
    if(m_drag && m_drag->nativeDrag) {
        QDrag::cancel();
    }
    finishDrag();
}

bool LayoutDragController::hasActiveDrag() const
{
    return m_drag && m_drag->source;
}

void LayoutDragController::startPaletteDrag(const QString& key)
{
    if(!m_editing || m_drag || !m_provider->canCreateWidget(key)) {
        return;
    }

    auto staging = std::make_unique<RootContainer>(m_provider, m_settings);
    auto* widget = staging->widget();
    startDrag(widget, key, std::move(staging));
}

void LayoutDragController::positionPalette()
{
    if(m_updatingPalette || !m_editing) {
        return;
    }

    const bool dockPalette = m_settings->value<Settings::Gui::DockWidgetPalette>();
    if(!dockPalette && (m_paletteDocked || m_paletteExpansion)) {
        hidePalette();
    }

    const QScopedValueRollback changing{m_updatingPalette, true};

    auto* window = m_layout->window();
    auto* box    = qobject_cast<QHBoxLayout*>(m_layout->layout());

    const bool fullscreen = window->isMaximized() || window->isFullScreen();
    if(dockPalette && !m_paletteDocked) {
        const int expansion  = m_paletteWidth + std::max(0, box->spacing());
        const auto available = window->screen()->availableGeometry();
        const bool canExpand = m_palette->minimumSizeHint().height() <= m_layout->root()->height()
                            && window->width() + expansion <= window->maximumWidth()
                            && window->frameGeometry().width() + expansion <= available.width()
                            && window->frameGeometry().right() + expansion <= available.right();
        if(fullscreen || m_paletteExpansion || canExpand) {
            m_palette->setFixedWidth(m_paletteWidth);
            m_paletteDocked = true;
            box->addWidget(m_palette);

            if(!fullscreen && !m_paletteExpansion) {
                m_paletteExpansion = expansion;
                window->resize(window->width() + expansion, window->height());
            }

            m_palette->refresh();
            m_palette->show();
            m_paletteHoverTimer.stop();
            box->activate();
        }
    }

    if(!m_paletteDocked) {
        const auto canvas = relativeGeometry(m_layout->root(), m_layout);
        const int width   = std::min(m_paletteWidth, canvas.width());
        m_palette->setFixedWidth(width);
        m_palette->setGeometry(canvas.right() + 1 - width, canvas.top(), width, canvas.height());
    }

    updatePaletteHint();
}

void LayoutDragController::hidePalette()
{
    if(m_updatingPalette) {
        return;
    }

    const QScopedValueRollback changing{m_updatingPalette, true};

    auto* box = qobject_cast<QHBoxLayout*>(m_layout->layout());
    if(m_paletteDocked) {
        box->removeWidget(m_palette);
        m_paletteDocked = false;
    }

    m_palette->hide();

    auto* window = m_layout->window();
    if(m_paletteExpansion && !window->isMaximized() && !window->isFullScreen()) {
        window->resize(std::max(window->minimumWidth(), window->width() - m_paletteExpansion), window->height());
    }

    m_paletteExpansion = 0;
    box->activate();
    scheduleUpdate();
}

void LayoutDragController::updatePaletteHover()
{
    updatePaletteHint();

    if(m_editing && !m_paletteDocked && m_drag) {
        if(m_drag->creationKey.isEmpty() || !m_palette->isVisible()) {
            m_paletteHoverTimer.stop();
            return;
        }

        const bool hovered = m_palette->rect().contains(m_palette->mapFrom(m_layout, m_drag->position).toPoint());
        if(std::exchange(m_paletteHovered, hovered) != hovered) {
            m_paletteHoverTimer.stop();
        }
        if(!hovered && !m_paletteHoverTimer.isActive()) {
            m_paletteHoverTimer.start(600, this);
        }
        return;
    }

    if(!m_editing || m_paletteDocked || m_drag || QApplication::mouseButtons() != Qt::NoButton) {
        m_paletteHoverTimer.stop();
        return;
    }

    const auto position = QCursor::pos();
    const bool moved    = std::exchange(m_paletteHoverPosition, position) != position;
    const auto* window  = m_layout->window();
    const QRect windowArea{window->mapToGlobal(QPoint{}), window->size()};
    const QRect paletteArea{m_palette->mapToGlobal(QPoint{}), m_palette->size()};
    const bool overHint
        = m_paletteHint->isVisible() && m_paletteHint->rect().contains(m_paletteHint->mapFromGlobal(position));
    const bool hovered = window->isActiveWindow() && windowArea.contains(position)
                      && (overHint || position.x() >= windowArea.right() - 7
                          || (m_palette->isVisible() && paletteArea.contains(position)));
    if(hovered != m_paletteHovered) {
        m_paletteHovered = hovered;
        m_paletteHoverTimer.stop();
    }

    if(moved && hovered && !m_palette->isVisible() && m_paletteHoverTimer.isActive()) {
        m_paletteHoverTimer.start(300, this);
    }

    if(hovered != m_palette->isVisible() && !m_paletteHoverTimer.isActive()) {
        if(hovered) {
            m_palette->refresh();
        }
        m_paletteHoverTimer.start(hovered ? 300 : 600, this);
    }
}

void LayoutDragController::updatePaletteHint()
{
    const bool visible = m_editing && !m_paletteDocked && !m_palette->isVisible() && !m_drag;
    if(visible) {
        auto* window = m_layout->window();
        if(m_paletteHint->parentWidget() != window) {
            m_paletteHint->setParent(window);
        }

        const int height = window->height() * 2 / 3;
        const int width  = std::min(16, window->width());
        m_paletteHint->setGeometry(window->width() - width, (window->height() - height) / 2, width, height);

        m_paletteHint->raise();
        m_paletteHint->update();
    }
    m_paletteHint->setVisible(visible);
}

void LayoutDragController::updateTabHover(const QPointF& position)
{
    if(!m_drag) {
        return;
    }

    m_drag->position = position;

    QTabWidget* tabs{nullptr};
    QWidget* page{nullptr};
    if(hasActiveDrag()
       && (!m_palette->isVisible() || !m_palette->rect().contains(m_palette->mapFrom(m_layout, position).toPoint()))) {
        for(const auto& panel : m_panels) {
            if(!panel.tabs || !panel.widget || panel.widget == m_drag->source
               || m_drag->source->isAncestorOf(panel.widget)) {
                continue;
            }

            const auto* bar  = panel.tabs->tabBar();
            const auto point = bar->mapFrom(m_layout, position).toPoint();
            const int index  = bar->rect().contains(point) ? bar->tabAt(point) : -1;
            if(bar->isVisibleTo(m_layout) && index >= 0 && bar->isTabEnabled(index)
               && index != panel.tabs->currentIndex()) {
                tabs = panel.tabs;
                page = tabs->widget(index);
                break;
            }
        }
    }

    if(tabs == m_drag->hoverTabs && page == m_drag->hoverPage) {
        return;
    }

    m_tabHoverTimer.stop();
    m_drag->hoverTabs = tabs;
    m_drag->hoverPage = page;
    if(page) {
        m_tabHoverTimer.start(1000, this);
    }
}

void LayoutDragController::selectWidget(FyWidget* widget, bool keepPath)
{
    m_selected = widget;

    if(!keepPath) {
        m_selectionAnchor = widget;
    }

    if(!widget) {
        m_selectionAnchor = nullptr;
        m_selectionControl->hide();
        m_selectionOutline->hide();
        m_selectionGrip->hide();
    }
    else {
        updatePanels();
    }
}

void LayoutDragController::updateSelection()
{
    if(!m_selected || !m_layout->root()->isAncestorOf(m_selected)) {
        selectWidget(nullptr);
        return;
    }

    if(!m_selectionAnchor || !m_layout->root()->isAncestorOf(m_selectionAnchor)) {
        m_selectionAnchor = m_selected;
    }

    std::vector<QPointer<FyWidget>> path;
    for(auto* widget = m_selectionAnchor.data(); widget && widget != m_layout->root(); widget = widget->findParent()) {
        const auto* parent = qobject_cast<WidgetContainer*>(widget->findParent());
        if(parent && parent->widgetIndex(widget->id()) >= 0) {
            path.emplace_back(widget);
        }
    }
    std::ranges::reverse(path);

    if(std::ranges::find(path, m_selected) == path.end()) {
        selectWidget(nullptr);
        return;
    }

    auto* crumbs = m_selectionBar->widget();
    auto* row    = crumbs->layout();
    if(path != m_selectionPath) {
        m_selectionPath = path;

        while(const auto* item = row->takeAt(0)) {
            delete item->widget();
            delete item;
        }

        for(const auto& widget : path) {
            if(row->count()) {
                row->addWidget(new QLabel(u"›"_s, crumbs));
            }
            auto* button = new QToolButton(crumbs);
            button->setProperty("LayoutWidgetId", widget->id().name());
            button->setCheckable(true);
            QObject::connect(button, &QToolButton::clicked, this, [this, widget] {
                if(widget) {
                    selectWidget(widget, true);
                }
            });
            row->addWidget(button);
        }
    }

    QToolButton* selectedButton{nullptr};
    const auto buttons = crumbs->findChildren<QToolButton*>(QString{}, Qt::FindDirectChildrenOnly);
    for(size_t index{0}; index < path.size(); ++index) {
        auto* button = buttons.at(static_cast<qsizetype>(index));

        const auto& widget = path[index];
        QString name       = widget->name();
        if(const auto* splitter = qobject_cast<SplitterWidget*>(widget)) {
            name = splitter->orientation() == Qt::Horizontal ? tr("Horizontal split") : tr("Vertical split");
        }

        button->setText(name);
        button->setToolTip(tr("Select %1").arg(name));
        button->setChecked(widget == m_selected);

        if(button->isChecked()) {
            selectedButton = button;
        }
    }

    const QRect rect = relativeGeometry(m_selected, m_layout).intersected(m_layout->rect());
    QRect available  = relativeGeometry(m_layout->root(), m_layout);
    if(m_palette->isVisible() && !m_paletteDocked) {
        available.setRight(std::min(available.right(), m_palette->x() - 4));
    }

    const bool visible
        = m_editing && !m_drag && m_selected->isVisibleTo(m_layout) && !rect.isEmpty() && !available.isEmpty();
    m_selectionOutline->setGeometry(rect);
    m_selectionOutline->setVisible(visible);
    m_selectionOutline->raise();

    const bool splitterSelected = qobject_cast<SplitterWidget*>(m_selected);
    m_selectionGrip->setVisible(visible && splitterSelected);
    m_selectionGrip->setProperty("LayoutWidgetId", m_selected->id().name());
    m_selectionGrip->setToolTip(tr("Move %1").arg(m_selected->name()));

    QPoint gripPosition{std::clamp(rect.right() - m_selectionGrip->width() - 3, available.left(),
                                   std::max(available.left(), available.right() + 1 - m_selectionGrip->width())),
                        std::clamp(rect.top() + 4, available.top(),
                                   std::max(available.top(), available.bottom() + 1 - m_selectionGrip->height()))};
    row->activate();
    crumbs->setFixedSize(crumbs->sizeHint());

    const auto margins          = m_selectionControl->layout()->contentsMargins();
    const int horizontalMargins = margins.left() + margins.right();
    const int verticalMargins   = margins.top() + margins.bottom();
    const int width             = std::min(crumbs->width(), std::max(0, available.width() - 16 - horizontalMargins));
    const int height
        = crumbs->height() + (width < crumbs->width() ? m_selectionBar->horizontalScrollBar()->sizeHint().height() : 0);

    m_selectionBar->setFixedSize(width, std::max(0, std::min(height, available.height() - 16 - verticalMargins)));

    const QSize size{m_selectionBar->width() + horizontalMargins, m_selectionBar->height() + verticalMargins};
    m_selectionControl->setGeometry(available.left() + ((available.width() - size.width()) / 2),
                                    available.top() + std::min(8, std::max(0, available.height() - size.height())),
                                    size.width(), size.height());
    m_selectionControl->setVisible(visible && width > 0 && m_selectionBar->height() > 0);
    m_selectionControl->raise();

    if(m_selectionGrip->isVisible()
       && QRect{gripPosition, m_selectionGrip->size()}.intersects(m_selectionControl->geometry())) {
        gripPosition.setY(std::min(m_selectionControl->geometry().bottom() + 8,
                                   std::max(available.top(), available.bottom() + 1 - m_selectionGrip->height())));
    }

    m_selectionGrip->move(gripPosition);
    m_selectionGrip->raise();

    if(selectedButton && visible) {
        m_selectionBar->ensureWidgetVisible(selectedButton);
    }
}

void LayoutDragController::updateSelectionStyle()
{
    auto highlight = QApplication::palette().color(QPalette::Highlight);
    highlight.setAlpha(80);
    m_selectionControl->setColour(highlight);
    m_selectionGrip->setBackgroundColour(highlight);

    m_selectionBar->setStyleSheet(u"QToolButton:checked { background-color: palette(highlight); "
                                  "color: palette(highlighted-text); }"_s);
    m_selectionBar->viewport()->update();
}

void LayoutDragController::resetStackGrip(PanelOverlay& panel)
{
    if(panel.tabs) {
        if(panel.tabs->cornerWidget(Qt::TopRightCorner) == panel.grip) {
            panel.tabs->setCornerWidget(nullptr, Qt::TopRightCorner);
        }
        panel.tabs->tabBar()->setMaximumHeight(panel.maximumBarHeight);
    }
    if(panel.selector && panel.selector->cornerWidget(Qt::TopRightCorner) == panel.grip) {
        // SingleTabbedWidget owns and deletes its corner widget
        panel.selector->setCornerWidget(nullptr, Qt::TopRightCorner);
    }
    if(panel.grip) {
        panel.grip->hide();
    }
    if(panel.emptyArea) {
        panel.emptyArea->hide();
    }
    if(panel.outline) {
        panel.outline->hide();
        panel.outline->setHovered(false);
    }
}

void LayoutDragController::createStackGrip(PanelOverlay& panel)
{
    const QPointer<FyWidget> guarded{panel.widget};

    auto* host = panel.tabs ? static_cast<QWidget*>(panel.tabs) : panel.selector;
    auto* grip = new LayoutStackGrip(host, panel.outline);
    QObject::connect(grip, &LayoutPanelOverlay::dragRequested, this, [this, guarded] {
        if(guarded) {
            startDrag(guarded);
        }
    });
    QObject::connect(grip, &LayoutPanelOverlay::selectionRequested, this, [this, guarded] {
        if(guarded) {
            selectWidget(guarded);
        }
    });

    const QString description = panel.selector ? tr("Move tabs and content") : tr("Move tab stack");
    grip->setProperty("LayoutWidgetId", panel.widget->id().name());
    grip->setToolTip(description);
    panel.grip = grip;
}

void LayoutDragController::updateStackOverlay(PanelOverlay& panel, const QRect& rect)
{
    auto* tabs     = panel.tabs.data();
    auto* selector = panel.selector.data();
    auto* bar      = tabs ? tabs->tabBar() : selector->tabBar();

    const bool vertical = tabs && (tabs->tabPosition() == QTabWidget::West || tabs->tabPosition() == QTabWidget::East);
    panel.grip->setOrientation(vertical ? Qt::Vertical : Qt::Horizontal);
    if(vertical) {
        panel.grip->setFixedSize(std::max(1, bar->sizeHint().width()), 32);
    }
    else {
        panel.grip->setFixedWidth(32);
        panel.grip->setMinimumHeight(0);
        panel.grip->setMaximumHeight(32);
    }

    if(vertical) {
        if(tabs->cornerWidget(Qt::TopRightCorner) == panel.grip) {
            tabs->setCornerWidget(nullptr, Qt::TopRightCorner);
        }
        // QTabWidget only reserves corner widgets for horizontal tab bars
        bar->setMaximumHeight(std::min(panel.maximumBarHeight, std::max(0, tabs->height() - panel.grip->height())));
        panel.grip->move(bar->x() + ((bar->width() - panel.grip->width()) / 2), tabs->height() - panel.grip->height());
    }
    else if(tabs) {
        bar->setMaximumHeight(panel.maximumBarHeight);
        if(tabs->cornerWidget(Qt::TopRightCorner) != panel.grip) {
            tabs->setCornerWidget(panel.grip, Qt::TopRightCorner);
        }
    }
    else if(selector->cornerWidget(Qt::TopRightCorner) != panel.grip) {
        selector->setCornerWidget(panel.grip, Qt::TopRightCorner);
    }

    const bool visible = panel.widget->isVisibleTo(m_layout) && !rect.isEmpty();
    panel.grip->setVisible(visible);
    panel.outline->setGeometry(rect);
    panel.outline->setVisible(visible && !m_drag);
    panel.outline->raise();

    const auto* host = tabs ? static_cast<QWidget*>(tabs) : selector;
    QRect strip      = relativeGeometry(bar, m_layout).translated(-panel.overlay->pos());
    if(vertical) {
        strip.setTop(host->mapTo(m_layout, QPoint{}).y() - panel.overlay->y());
        strip.setHeight(host->height());
    }
    else {
        strip.setLeft(host->mapTo(m_layout, QPoint{}).x() - panel.overlay->x());
        strip.setWidth(host->width());
    }

    QRegion mask{strip.intersected(panel.overlay->rect())};
    for(int i{0}; i < bar->count(); ++i) {
        const QRect tab = bar->tabRect(i);
        mask -= QRect{bar->mapTo(m_layout, tab.topLeft()) - panel.overlay->pos(), tab.size()};
    }

    const auto children = bar->findChildren<QWidget*>();
    for(const auto* child : children) {
        if(child->isVisibleTo(bar)) {
            mask -= relativeGeometry(child, m_layout).translated(-panel.overlay->pos());
        }
    }

    if(!vertical) {
        // The native corner widget can be positioned after this update
        QRect corner = strip;
        corner.setWidth(panel.grip->width());
        if(host->layoutDirection() != Qt::RightToLeft) {
            corner.moveRight(strip.right());
        }
        mask -= corner;
    }

    const auto* leftCorner = tabs ? tabs->cornerWidget(Qt::TopLeftCorner) : selector->cornerWidget(Qt::TopLeftCorner);
    if(leftCorner && leftCorner->isVisibleTo(host)) {
        mask -= relativeGeometry(leftCorner, m_layout).translated(-panel.overlay->pos());
    }

    mask -= relativeGeometry(panel.grip, m_layout).translated(-panel.overlay->pos());

    const auto handles = m_layout->findChildren<FySplitterHandle*>();
    for(const auto* handle : handles) {
        if(handle->isVisibleTo(m_layout)) {
            mask -= relativeGeometry(handle, m_layout).translated(-panel.overlay->pos());
        }
    }

    panel.overlay->setMask(mask);
    panel.overlay->setVisible(visible && !mask.isEmpty() && !m_drag);
    panel.overlay->raise();

    if(panel.emptyArea) {
        const bool bottom = selector->tabPosition() == SingleTabbedWidget::TabPosition::Bottom;
        QRect emptyRect   = panel.overlay->rect();

        if(bottom) {
            emptyRect.setBottom(strip.top() - 1);
        }
        else {
            emptyRect.setTop(strip.bottom() + 1);
        }

        emptyRect.translate(panel.overlay->pos());
        panel.emptyArea->setGeometry(emptyRect);
        const auto* container = qobject_cast<WidgetContainer*>(panel.widget);
        panel.emptyArea->setVisible(visible && container && container->widgets().empty() && !m_drag);
        panel.emptyArea->raise();
    }
}
} // namespace Fooyin

#include "moc_layoutdragcontroller.cpp"
