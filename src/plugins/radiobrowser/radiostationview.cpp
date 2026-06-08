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

#include "radiostationview.h"

#include "radiostation.h"

#include <gui/widgets/autoheaderview.h>

#include <QPaintEvent>
#include <QPainter>

using namespace Qt::StringLiterals;

namespace Fooyin::RadioBrowser {
RadioStationView::RadioStationView(QWidget* parent)
    : ExpandedTreeView{parent}
    , m_header{new AutoHeaderView(Qt::Horizontal, this)}
    , m_loading{false}
{
    setHeader(m_header);
    setIconItemColumn(Station);
    setUniformRowHeights(true);
    setSelectionBehavior(SelectRows);
    setSelectionMode(ExtendedSelection);
    setEditTriggers(NoEditTriggers);
    setSortingEnabled(true);
    setContextMenuPolicy(Qt::CustomContextMenu);

    m_header->setStretchEnabled(true);
    m_header->setSectionsMovable(true);
    m_header->setFirstSectionMovable(false);
    m_header->setSectionsClickable(true);
    m_header->setContextMenuPolicy(Qt::CustomContextMenu);
}

AutoHeaderView* RadioStationView::stationHeader() const
{
    return m_header;
}

void RadioStationView::finaliseView(const QByteArray& headerState)
{
    if(!headerState.isEmpty()) {
        m_header->restoreHeaderState(headerState);
    }
    else {
        resetColumnsToDefault();
    }
}

void RadioStationView::resetColumnsToDefault()
{
    m_header->resetSectionPositions();
    m_header->setHeaderSectionWidths({{Station, 0.34},
                                      {Country, 0.14},
                                      {Tags, 0.12},
                                      {Language, 0.14},
                                      {Codec, 0.09},
                                      {Bitrate, 0.08},
                                      {Votes, 0.045},
                                      {Clicks, 0.045}});
    m_header->setHeaderSectionHidden(Tags, true);
    m_header->setHeaderSectionHidden(Language, true);
    m_header->setHeaderSectionHidden(Votes, true);
    m_header->setHeaderSectionHidden(Clicks, true);
}

void RadioStationView::setEmptyText(const QString& text)
{
    m_emptyText = text;
    viewport()->update();
}

void RadioStationView::setLoading(const bool loading)
{
    m_loading = loading;
    viewport()->update();
}

void RadioStationView::setLoadingText(const QString& text)
{
    m_loadingText = text;
    viewport()->update();
}

void RadioStationView::changeEvent(QEvent* event)
{
    ExpandedTreeView::changeEvent(event);

    switch(event->type()) {
        case QEvent::FontChange:
        case QEvent::StyleChange:
            Q_EMIT displayChanged();
            break;
        default:
            break;
    }
}

void RadioStationView::paintEvent(QPaintEvent* event)
{
    if(model() && model()->rowCount(rootIndex()) > 0) {
        ExpandedTreeView::paintEvent(event);
        return;
    }

    QPainter painter{viewport()};
    painter.fillRect(viewport()->rect(), palette().brush(QPalette::Base));

    const QString text = m_loading ? m_loadingText : m_emptyText;
    if(text.isEmpty()) {
        return;
    }

    QRect textRect = painter.fontMetrics().boundingRect(text);
    textRect.moveCenter(viewport()->rect().center());
    painter.drawText(textRect, Qt::AlignCenter, text);
}

void RadioStationView::clearSort()
{
    if(auto* radioModel = model()) {
        radioModel->sort(-1, Qt::AscendingOrder);
    }
}
} // namespace Fooyin::RadioBrowser

#include "moc_radiostationview.cpp"
