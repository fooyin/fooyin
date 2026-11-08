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

#include "statscolours.h"

#include <QDate>
#include <QString>
#include <QVariantAnimation>
#include <QWidget>

#include <functional>
#include <optional>
#include <vector>

namespace Fooyin::Stats {
struct ChartData
{
    QString label;
    int value{0};
};

//! Formats a number compactly, e.g. 1,284 as 1.3K.
QString compactNumber(qint64 value);
//! "1 listen" or "12 listens".
QString listensText(qint64 count);

/*!
 * Base for the custom painted charts.
 *
 * Provides the colours, the animation that grows the data in when it changes and the hover tooltips.
 */
class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget* parent = nullptr);

    //! Stops the intro animation, drawing the chart as it appears once settled (e.g. before exporting it).
    void skipReveal();

protected:
    [[nodiscard]] const Colours& colours() const;
    //! Progress of the intro animation between 0 and 1.
    [[nodiscard]] double reveal() const;
    void restartReveal();

    [[nodiscard]] QFont smallFont() const;

    //! Identifies what, if anything, is at @p pos (negative when nothing). Used to highlight on hover.
    [[nodiscard]] virtual int hitTest(const QPoint& pos) const;
    //! Text shown in the tooltip for the item returned by hitTest().
    [[nodiscard]] virtual QString tipFor(int item, const QPoint& pos) const;
    [[nodiscard]] int hovered() const;
    void setHovered(int item);
    //! Whether hovering an item shows a pointer cursor.
    void setClickableItems(bool clickable);
    [[nodiscard]] bool clickableItems() const;

    void showEvent(QShowEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    Colours m_colours;
    QVariantAnimation m_reveal;
    double m_revealValue;
    bool m_revealPending;
    int m_hovered;
    bool m_clickableItems;
};

//! Columns for a handful of values, such as listens per month.
class VerticalBarsChart : public ChartWidget
{
    Q_OBJECT

public:
    explicit VerticalBarsChart(QWidget* parent = nullptr);

    void setData(std::vector<ChartData> data);
    void setPlotHeight(int height);
    //! Whether clicking a column emits barClicked().
    void setClickable(bool clickable);
    void setTipFormatter(std::function<QString(const ChartData&)> formatter);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    void barClicked(int index);

protected:
    [[nodiscard]] int hitTest(const QPoint& pos) const override;
    [[nodiscard]] QString tipFor(int item, const QPoint& pos) const override;

    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] QRectF plotRect() const;

    std::vector<ChartData> m_data;
    std::function<QString(const ChartData&)> m_tipFormatter;
    int m_plotHeight;
};

//! Horizontal bars for a ranking, labelled with the name on the left and the value at the tip.
class RankBarsChart : public ChartWidget
{
    Q_OBJECT

public:
    explicit RankBarsChart(QWidget* parent = nullptr);

    void setData(std::vector<ChartData> data);
    void setRowHeight(int height);
    void setClickable(bool clickable);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    void rowClicked(int index);

protected:
    [[nodiscard]] int hitTest(const QPoint& pos) const override;
    [[nodiscard]] QString tipFor(int item, const QPoint& pos) const override;

    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    std::vector<ChartData> m_data;
    int m_rowHeight;
};

//! Share of a total split between categories, with a legend.
class DonutChart : public ChartWidget
{
    Q_OBJECT

public:
    explicit DonutChart(QWidget* parent = nullptr);

    //! @p other is the remainder not covered by the categories in @p data, drawn in a neutral colour.
    void setData(std::vector<ChartData> data, int other = 0);
    void setCentreLabel(const QString& label);
    void setRingSize(int size);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    [[nodiscard]] int hitTest(const QPoint& pos) const override;
    [[nodiscard]] QString tipFor(int item, const QPoint& pos) const override;

    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Segment
    {
        QString label;
        int value{0};
        QColor colour;
    };

    [[nodiscard]] QRectF ringRect() const;
    [[nodiscard]] QRectF legendRect() const;
    [[nodiscard]] qreal legendRowHeight() const;
    [[nodiscard]] int total() const;
    void rebuildSegments();

    std::vector<ChartData> m_data;
    int m_other;
    std::vector<Segment> m_segments;
    QString m_centreLabel;
    int m_ringSize;
};

//! A single value as a fraction of a whole.
class GaugeChart : public ChartWidget
{
    Q_OBJECT

public:
    explicit GaugeChart(QWidget* parent = nullptr);

    void setValue(double fraction, const QString& valueText, const QString& subText);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double m_fraction;
    QString m_valueText;
    QString m_subText;
};

//! Listens by hour of the day, drawn around a 24 hour clock face.
class ListeningClockChart : public ChartWidget
{
    Q_OBJECT

public:
    explicit ListeningClockChart(QWidget* parent = nullptr);

    void setData(const std::array<int, 24>& hours);
    void setClockSize(int size);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    [[nodiscard]] int hitTest(const QPoint& pos) const override;
    [[nodiscard]] QString tipFor(int item, const QPoint& pos) const override;

    void paintEvent(QPaintEvent* event) override;

private:
    [[nodiscard]] QPointF centre() const;
    [[nodiscard]] qreal radius() const;

    std::array<int, 24> m_hours;
    int m_size;
};

//! A grid of cells shaded by value, such as listens by weekday and hour.
class GridHeatmapChart : public ChartWidget
{
    Q_OBJECT

public:
    explicit GridHeatmapChart(QWidget* parent = nullptr);

    void setData(std::vector<int> values, int rows, int columns);
    void setLabels(QStringList rowLabels, QStringList columnLabels, int columnLabelStep = 1);
    void setTipFormatter(std::function<QString(int row, int column, int value)> formatter);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    [[nodiscard]] int hitTest(const QPoint& pos) const override;
    [[nodiscard]] QString tipFor(int item, const QPoint& pos) const override;

    void paintEvent(QPaintEvent* event) override;

private:
    struct Layout
    {
        qreal left{0};
        qreal top{0};
        qreal cellWidth{0};
        qreal cellHeight{0};
    };
    [[nodiscard]] Layout layout() const;

    std::vector<int> m_values;
    int m_rows;
    int m_columns;
    QStringList m_rowLabels;
    QStringList m_columnLabels;
    int m_columnLabelStep;
    std::function<QString(int, int, int)> m_tipFormatter;
};

//! A calendar of every day in a range shaded by the number of listens, one row of weeks per year.
class CalendarHeatmapChart : public ChartWidget
{
    Q_OBJECT

public:
    explicit CalendarHeatmapChart(QWidget* parent = nullptr);

    void setData(std::vector<int> listensPerDay, const QDate& firstDay);
    void setMaxCellSize(int size);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;

protected:
    [[nodiscard]] int hitTest(const QPoint& pos) const override;
    [[nodiscard]] QString tipFor(int item, const QPoint& pos) const override;

    void paintEvent(QPaintEvent* event) override;

private:
    //! One row of weeks.
    struct Strip
    {
        QDate start;
        QDate end;
        //! Sunday on or before start.
        QDate gridStart;
        int weeks{0};
        //! The first row of a calendar year, or of the whole range when it's a year or less.
        bool firstOfBlock{false};
    };
    struct Geometry
    {
        qreal cell{0};
        qreal gap{2};
        qreal labelWidth{0};
        qreal headerHeight{0};
        qreal stripSpacing{12};
    };
    struct Layout
    {
        std::vector<Strip> strips;
        Geometry geometry;
        bool showYears{false};
    };

    //! Works out the rows of weeks, wrapping them as needed to keep the cells a readable size at @p width.
    [[nodiscard]] Layout layoutFor(int width) const;
    [[nodiscard]] static qreal stripHeight(const Geometry& geometry);
    [[nodiscard]] static QRectF cellRect(const Layout& layout, size_t strip, const QDate& day);
    [[nodiscard]] int valueAt(const QDate& day) const;

    std::vector<int> m_values;
    QDate m_firstDay;
    QDate m_lastDay;
    int m_maxValue;
    int m_maxCell;
};

//! Listens over time as a line, either as a running total or per day.
class ListensLineChart : public ChartWidget
{
    Q_OBJECT

public:
    explicit ListensLineChart(QWidget* parent = nullptr);

    void setData(std::vector<int> listensPerDay, const QDate& firstDay, bool cumulative);
    void setPlotHeight(int height);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    [[nodiscard]] int hitTest(const QPoint& pos) const override;
    [[nodiscard]] QString tipFor(int item, const QPoint& pos) const override;

    void paintEvent(QPaintEvent* event) override;

private:
    struct Axis
    {
        double step{1};
        int intervals{1};
    };

    [[nodiscard]] Axis axis() const;
    [[nodiscard]] QRectF plotRect() const;
    [[nodiscard]] qreal plotMax() const;
    [[nodiscard]] qreal yFor(double value, const QRectF& plot, double max) const;

    std::vector<int> m_listens;
    std::vector<qint64> m_series;
    QDate m_firstDay;
    bool m_cumulative;
    int m_plotHeight;
};
} // namespace Fooyin::Stats
