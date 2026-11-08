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

#include "statscharts.h"

#include <QColor>
#include <QDate>
#include <QFrame>
#include <QPushButton>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

class QButtonGroup;
class QHBoxLayout;
class QLabel;
class QVBoxLayout;

namespace Fooyin::Stats {
//! Listening time as "3d 4h", "12h 34m" or "45m".
QString formatListenTime(qint64 seconds);
//! A span as "1:23:45" or "4:05".
QString formatDuration(qint64 ms);
//! A time of day given as a minute of the day, e.g. 1,125 as 18:45 in the locale's time format.
QString formatMinuteOfDay(int minute);
QString formatDay(const QDate& day);
//! A date range as "3 Nov – 9 Nov".
QString formatRange(const QDate& first, const QDate& last);

//! A titled panel holding a chart or list.
class StatsCard : public QFrame
{
    Q_OBJECT

public:
    explicit StatsCard(const QString& title, const QString& subtitle = {}, QWidget* parent = nullptr);

    void setSubtitle(const QString& subtitle);
    //! Adds a widget, such as a toggle, to the right of the title.
    void addHeaderWidget(QWidget* widget);
    void addWidget(QWidget* widget);
    void addLayout(QLayout* layout);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* m_subtitle;
    QHBoxLayout* m_header;
    QVBoxLayout* m_body;
};

//! A labelled figure, optionally with how much it changed compared with a previous period.
class MiniTile : public QWidget
{
    Q_OBJECT

public:
    MiniTile(QString label, QString value, QWidget* parent = nullptr);

    void setDetail(const QString& detail);
    //! Shows the percentage change from @p previous to @p current. Nothing is shown when there's no previous value.
    void setDelta(qint64 current, qint64 previous);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    [[nodiscard]] QString deltaText() const;

    QString m_label;
    QString m_value;
    QString m_detail;
    std::optional<double> m_delta;
};

//! A large figure with a caption, used on the year in review cards.
class BigStat : public QWidget
{
    Q_OBJECT

public:
    BigStat(QString value, QString label, QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_value;
    QString m_label;
};

//! A row of mutually exclusive toggle buttons.
class ChipBar : public QWidget
{
    Q_OBJECT

public:
    explicit ChipBar(QWidget* parent = nullptr);

    void setItems(const QStringList& items);
    //! Selects an item without emitting indexChanged(). -1 clears the selection.
    void setCurrentIndex(int index);
    [[nodiscard]] int currentIndex() const;

Q_SIGNALS:
    void indexChanged(int index);

private:
    QHBoxLayout* m_layout;
    QButtonGroup* m_group;
};

//! Reveals more rows of a list in steps, and collapses it again.
class ShowMoreButton : public QPushButton
{
    Q_OBJECT

public:
    ShowMoreButton(int total, int initial, int step, QWidget* parent = nullptr);

    [[nodiscard]] int shown() const;

Q_SIGNALS:
    void shownChanged(int shown);

private:
    void updateText();

    int m_total;
    int m_initial;
    int m_step;
    int m_shown;
};

//! Rows of a ranked list, each with a title and subtitle on the left and a value on the right.
class RowListWidget : public ChartWidget
{
    Q_OBJECT

public:
    struct Row
    {
        QString title;
        QString subtitle;
        //! The main figure, e.g. the number of listens.
        QString value;
        //! A secondary note under the value, e.g. a date.
        QString trailing;
    };

    explicit RowListWidget(QWidget* parent = nullptr);

    void setRows(std::vector<Row> rows, bool showRank = true);
    void setRowHeight(int height);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    [[nodiscard]] int hitTest(const QPoint& pos) const override;
    [[nodiscard]] QString tipFor(int item, const QPoint& pos) const override;

    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<Row> m_rows;
    bool m_showRank;
    int m_rowHeight;
};

//! The tinted card each page of the year in review is shown on.
class StoryCard : public QWidget
{
    Q_OBJECT

public:
    explicit StoryCard(QWidget* parent = nullptr);

    //! Fades the card from a lighter to a darker shade of @p tint, with light text and charts over the top.
    void setTint(const QColor& tint);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor m_tint;
};

//! Shows how many pages there are and which is current, and lets one be picked.
class PageDots : public QWidget
{
    Q_OBJECT

public:
    explicit PageDots(QWidget* parent = nullptr);

    void setCount(int count);
    void setCurrent(int current);

    [[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
    void pageClicked(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] QRectF dotRect(int index) const;

    int m_count;
    int m_current;
};

//! A message shown when there's nothing to chart.
QWidget* createEmptyHint(const QString& text, QWidget* parent = nullptr);
} // namespace Fooyin::Stats
