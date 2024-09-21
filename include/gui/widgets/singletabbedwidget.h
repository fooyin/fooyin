/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <QWidget>

namespace Fooyin {
class EditableTabBar;
class SingleTabbedWidgetPrivate;

class FYGUI_EXPORT SingleTabbedWidget : public QWidget
{
    Q_OBJECT

public:
    enum class TabShape : uint8_t
    {
        Rounded = 0,
        Triangular
    };
    Q_ENUM(TabShape)

    explicit SingleTabbedWidget(QWidget* parent = nullptr);

    void setWidget(QWidget* widget);

    int addTab(const QString& label);
    int addTab(const QIcon& icon, const QString& label);
    int insertTab(int index, const QString& label);
    int insertTab(int index, const QIcon& icon, const QString& label);
    void removeTab(int index);

    [[nodiscard]] bool isTabEnabled(int index) const;
    void setTabEnabled(int index, bool enabled);

    [[nodiscard]] bool isTabVisible(int index) const;
    void setTabVisible(int index, bool visible);

    [[nodiscard]] QString tabText(int index) const;
    void setTabText(int index, const QString& text);

    [[nodiscard]] QIcon tabIcon(int index) const;
    void setTabIcon(int index, const QIcon& icon);

    [[nodiscard]] int currentIndex() const;
    [[nodiscard]] QWidget* currentWidget() const;
    [[nodiscard]] int count() const;

    [[nodiscard]] bool tabsClosable() const;
    void setTabsClosable(bool closeable);

    [[nodiscard]] bool isMovable() const;
    void setMovable(bool movable);

    [[nodiscard]] TabShape tabShape() const;
    void setTabShape(TabShape shape);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] int heightForWidth(int width) const override;
    [[nodiscard]] bool hasHeightForWidth() const override;

    [[nodiscard]] QWidget* cornerWidget(Qt::Corner corner = Qt::TopRightCorner) const;
    void setCornerWidget(QWidget* widget, Qt::Corner corner = Qt::TopRightCorner);

    [[nodiscard]] Qt::TextElideMode elideMode() const;
    void setElideMode(Qt::TextElideMode mode);

    [[nodiscard]] QSize iconSize() const;
    void setIconSize(const QSize& size);

    [[nodiscard]] bool usesScrollButtons() const;
    void setUsesScrollButtons(bool useButtons);

    [[nodiscard]] bool documentMode() const;
    void setDocumentMode(bool enabled);

    [[nodiscard]] bool tabBarAutoHide() const;
    void setTabBarAutoHide(bool enabled);

    void clear();

    [[nodiscard]] EditableTabBar* tabBar() const;

signals:
    void currentChanged(int index);
    void tabMoved(int oldIndex, int newIndex);
    void tabCloseRequested(int index);
    void tabBarClicked(int index);
    void tabBarDoubleClicked(int index);

public slots:
    void setCurrentIndex(int index);

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool event(QEvent* event) override;

private:
    std::unique_ptr<SingleTabbedWidgetPrivate> p;
};
} // namespace Fooyin
