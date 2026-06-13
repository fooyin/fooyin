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
#include <QPushButton>
#include <QResizeEvent>

using namespace Qt::StringLiterals;

namespace Fooyin::RadioBrowser {
RadioStationView::RadioStationView(QWidget* parent)
    : ExpandedTreeView{parent}
    , m_header{new AutoHeaderView(Qt::Horizontal, this)}
    , m_retryButton{new QPushButton(tr("Retry"), viewport())}
    , m_loading{false}
    , m_failed{false}
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

    m_retryButton->hide();
    QObject::connect(m_retryButton, &QPushButton::clicked, this, &RadioStationView::retryRequested);
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

void RadioStationView::setFailureText(const QString& text)
{
    m_failureText = text;
    m_failed      = !text.isEmpty();
    m_loading     = false;
    updateRetryButton();
    viewport()->update();
}

void RadioStationView::setLoading(const bool loading)
{
    m_loading = loading;
    if(loading) {
        m_failed = false;
        m_failureText.clear();
    }
    updateRetryButton();
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
        updateRetryButton();
        ExpandedTreeView::paintEvent(event);
        return;
    }

    updateRetryButton();

    QPainter painter{viewport()};
    painter.fillRect(viewport()->rect(), palette().brush(QPalette::Base));

    const QString text = m_loading ? m_loadingText : m_failed ? m_failureText : m_emptyText;
    if(text.isEmpty()) {
        return;
    }

    QRect textRect = painter.fontMetrics().boundingRect(text);
    textRect.moveCenter(viewport()->rect().center());
    if(m_failed && m_retryButton->isVisible()) {
        textRect.moveCenter(QPoint{viewport()->rect().center().x(),
                                   viewport()->rect().center().y() - (m_retryButton->height() / 2) - 8});
    }
    painter.drawText(textRect, Qt::AlignCenter, text);
}

void RadioStationView::resizeEvent(QResizeEvent* event)
{
    ExpandedTreeView::resizeEvent(event);
    updateRetryButton();
}

void RadioStationView::updateRetryButton()
{
    const bool visible = m_failed && model() && model()->rowCount(rootIndex()) == 0;
    m_retryButton->setVisible(visible);
    if(!visible) {
        return;
    }

    const QSize size = m_retryButton->sizeHint();
    const QRect rect = viewport()->rect();
    const QPoint pos{rect.center().x() - (size.width() / 2), rect.center().y() + 12};
    m_retryButton->setGeometry(QRect{pos, size});
}

void RadioStationView::clearSort()
{
    if(auto* radioModel = model()) {
        radioModel->sort(-1, Qt::AscendingOrder);
    }
}
} // namespace Fooyin::RadioBrowser

#include "moc_radiostationview.cpp"
