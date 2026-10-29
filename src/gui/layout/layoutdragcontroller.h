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

#pragma once

#include <gui/fywidget.h>
#include <gui/widgetcontainer.h>

#include <QBasicTimer>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QRect>

#include <optional>

class QDrag;
class QLabel;
class QScrollArea;
class QTabWidget;
class QToolButton;
class QUndoCommand;
class QUndoStack;

namespace Fooyin {
class EditableLayout;
class LayoutDragCard;
class LayoutDragSurface;
class LayoutPanelOverlay;
class LayoutStackGrip;
class OverlayWidget;
class RootContainer;
class SettingsManager;
class SingleTabbedWidget;
class WidgetPalette;
class WidgetProvider;

class LayoutDragController : public QObject
{
    Q_OBJECT

public:
    LayoutDragController(EditableLayout* layout, QUndoStack* history, WidgetProvider* provider,
                         SettingsManager* settings);
    ~LayoutDragController() override;

    void setEditing(bool editing);

    [[nodiscard]] WidgetPalette* palette() const;
    void showPalette();

    void clearSelection(FyWidget* contextWidget);

    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    friend class LayoutDragSurface;

    void scheduleUpdate();
    void updatePanels();
    void startDrag(FyWidget* widget);
    void startDrag(FyWidget* widget, const QString& creationKey, std::unique_ptr<RootContainer> staging);
    void finishDrag();
    void finishDrag(std::unique_ptr<QUndoCommand> command);
    void cancelDrag();
    [[nodiscard]] bool hasActiveDrag() const;
    void startPaletteDrag(const QString& key);
    void positionPalette();
    void hidePalette();
    void updatePaletteHover();
    void updatePaletteHint();
    void updateTabHover(const QPointF& position);
    void selectWidget(FyWidget* widget, bool keepPath = false);
    void updateSelection();
    void updateSelectionStyle();

    struct PanelOverlay
    {
        QPointer<FyWidget> widget;
        LayoutPanelOverlay* overlay;
        QPointer<QTabWidget> tabs;
        QPointer<LayoutStackGrip> grip;
        LayoutPanelOverlay* outline{nullptr};
        int maximumBarHeight{0};
        QPointer<SingleTabbedWidget> selector;
        QLabel* emptyArea{nullptr};
    };

    static void resetStackGrip(PanelOverlay& panel);
    void createStackGrip(PanelOverlay& panel);
    void updateStackOverlay(PanelOverlay& panel, const QRect& rect);

    struct DragSession
    {
        QPointer<FyWidget> source;
        QPointer<QDrag> nativeDrag;
        std::unique_ptr<QUndoCommand> dropCommand;
        QString creationKey;
        std::unique_ptr<RootContainer> staging;
        QPointer<QWidget> previousFocus;
        QMetaObject::Connection sourceDestroyed;
        QPointer<QTabWidget> hoverTabs;
        QPointer<QWidget> hoverPage;
        QPointF position;
        Qt::KeyboardModifiers polledModifiers;
    };

    EditableLayout* m_layout;
    QUndoStack* m_history;
    WidgetProvider* m_provider;
    SettingsManager* m_settings;

    std::vector<PanelOverlay> m_panels;
    LayoutDragSurface* m_surface;
    OverlayWidget* m_sourceOverlay;
    WidgetPalette* m_palette;
    QWidget* m_paletteHint;
    LayoutDragCard* m_dragCard;
    OverlayWidget* m_selectionControl;
    QScrollArea* m_selectionBar;
    QToolButton* m_selectionClose;
    LayoutPanelOverlay* m_selectionOutline;
    LayoutStackGrip* m_selectionGrip;
    QPointer<FyWidget> m_selected;
    QPointer<FyWidget> m_selectionAnchor;
    std::vector<QPointer<FyWidget>> m_selectionPath;
    std::optional<DragSession> m_drag;
    int m_paletteWidth;
    int m_paletteExpansion;
    bool m_paletteDocked;
    bool m_updatingPalette;
    QBasicTimer m_palettePollTimer;
    QBasicTimer m_paletteHoverTimer;
    QPoint m_paletteHoverPosition;
    bool m_paletteHovered;
    QBasicTimer m_tabHoverTimer;
    bool m_updatePending;
    bool m_editing;
    bool m_updatingPanels;
};
} // namespace Fooyin
