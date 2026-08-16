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

#include "fygui_export.h"

#include <QFrame>

#include <memory>

namespace Fooyin {
class FySplitterHandle;
class FySplitterPrivate;

/**
 * A heavily modified splitter implementation based on Qt's QSplitter.
 *
 * FySplitter retains compatibility with QSplitter's saved state where practical,
 * while allowing Fooyin-specific behaviour such as per-widget size locking.
 */
class FYGUI_EXPORT FySplitter : public QFrame
{
    Q_OBJECT

public:
    explicit FySplitter(QWidget* parent = nullptr);
    explicit FySplitter(Qt::Orientation orientation, QWidget* parent = nullptr);
    ~FySplitter() override;

    void addWidget(QWidget* widget);
    void insertWidget(int index, QWidget* widget);
    QWidget* replaceWidget(int index, QWidget* replacement);

    [[nodiscard]] Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    [[nodiscard]] bool childrenCollapsible() const;
    void setChildrenCollapsible(bool collapse);

    [[nodiscard]] bool isCollapsible(int index) const;
    void setCollapsible(int index, bool collapse);

    [[nodiscard]] bool isLocked(int index) const;
    /**
     * Locks a widget's size against automatic resizing.
     * By default, moving either adjacent handle can resize the
     * widget and establish its new locked size, while non-adjacent handles preserve it.
     */
    bool setLocked(int index, bool locked);
    /** Controls whether only adjacent handles can resize locked widgets during user-driven splitter resizing. */
    void setLockedWidgetsResizeAdjacentOnly(bool enabled);

    [[nodiscard]] bool opaqueResize() const;
    void setOpaqueResize(bool opaque = true);

    void refresh();

    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] QSize sizeHint() const override;

    [[nodiscard]] QList<int> sizes() const;
    void setSizes(const QList<int>& list);

    [[nodiscard]] QByteArray saveState() const;
    bool restoreState(const QByteArray& state);

    [[nodiscard]] int handleWidth() const;
    void setHandleWidth(int width);

    int indexOf(QWidget* widget) const;
    [[nodiscard]] QWidget* widget(int index) const;
    [[nodiscard]] int count() const;

    void getRange(int index, int* farMin, int* min, int* max, int* farMax) const;
    [[nodiscard]] FySplitterHandle* handle(int index) const;

    void setStretchFactor(int index, int stretch);

Q_SIGNALS:
    void splitterMoved(int pos, int index);

protected:
    virtual FySplitterHandle* createHandle();

    void childEvent(QChildEvent* event) override;

    bool event(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    void changeEvent(QEvent* event) override;
    void moveSplitter(int requestedPosition, int handleIndex);
    void setRubberBand(int position);
    int closestLegalPosition(int requestedPosition, int handleIndex);

private:
    friend class FySplitterHandle;
    friend class FySplitterPrivate;

    std::unique_ptr<FySplitterPrivate> p;
};

class FySplitterHandlePrivate;

class FYGUI_EXPORT FySplitterHandle : public QWidget
{
    Q_OBJECT

public:
    explicit FySplitterHandle(Qt::Orientation orientation, FySplitter* parent);
    ~FySplitterHandle() override;

    void setOrientation(Qt::Orientation orientation);
    [[nodiscard]] Qt::Orientation orientation() const;
    [[nodiscard]] bool opaqueResize() const;
    [[nodiscard]] FySplitter* splitter() const;

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool event(QEvent* event) override;

    void moveSplitter(int p);
    int closestLegalPosition(int p);

private:
    std::unique_ptr<FySplitterHandlePrivate> p;
};
} // namespace Fooyin
