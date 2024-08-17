/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <utils/widgets/autoheaderview.h>

#include <QActionGroup>
#include <QApplication>
#include <QIODevice>
#include <QLoggingCategory>
#include <QMenu>
#include <QMouseEvent>

Q_LOGGING_CATEGORY(AUTO_HEADER, "fy.autoheader")

constexpr auto MinSectionWidth = 20;

namespace Fooyin {
using SectionWidths  = QList<double>;
using SectionIndexes = QList<int>;

enum class SectionState
{
    None,
    Resizing,
    Moving,
};

class AutoHeaderViewPrivate
{
public:
    explicit AutoHeaderViewPrivate(AutoHeaderView* self)
        : m_self{self}
    { }

    void calculateSectionWidths();
    void normaliseWidths(const SectionIndexes& sections = {});
    void updateWidths(const SectionIndexes& sections = {}) const;

    void sectionResized(int resizedSection, int newSize);
    [[nodiscard]] int sectionHandleAt(int pos) const;
    [[nodiscard]] int rightLogicalSection(int visual) const;

    [[nodiscard]] bool shouldBlockResize(int oldPos) const;
    void columnsAboutToBeRemoved(int first, int last);
    void sectionCountChanged(int oldCount, int newCount);

    AutoHeaderView* m_self;

    bool m_stretchEnabled{false};
    SectionWidths m_sectionWidths;

    SectionState m_state{SectionState::None};
    int m_firstPos{-1};
    int m_mouseTarget{-1};
    int m_mouseSection{-1};
    int m_resizingSection{-1};
    int m_lastResizePos{-1};
    int m_firstPressed{-1};
    bool m_moving{false};

    int m_pendingColumns{0};
    QByteArray m_pendingState;
};

void AutoHeaderViewPrivate::calculateSectionWidths()
{
    if(!m_stretchEnabled) {
        return;
    }

    m_sectionWidths.clear();

    const int sectionCount = m_self->count();
    const int headerWidth  = m_self->width();

    m_sectionWidths.resize(sectionCount);

    for(int section{0}; section < sectionCount; ++section) {
        const auto width         = static_cast<double>(m_self->sectionSize(section)) / headerWidth;
        m_sectionWidths[section] = width;
    }
}

void AutoHeaderViewPrivate::normaliseWidths(const SectionIndexes& sections)
{
    if(!m_stretchEnabled) {
        return;
    }

    double widthTotal{0.0};

    const int sectionCount = m_self->count();
    for(int section{0}; section < sectionCount; ++section) {
        if(!m_self->isSectionHidden(section)) {
            widthTotal += m_sectionWidths.at(section);
        }
    }

    if(widthTotal == 0.0 || widthTotal == 1.0) {
        return;
    }

    double rightWidthTotal{widthTotal};

    if(!sections.empty()) {
        rightWidthTotal = 0.0;

        for(int section{0}; section < sectionCount; ++section) {
            if(!m_self->isSectionHidden(section) && sections.contains(section)) {
                rightWidthTotal += m_sectionWidths.at(section);
            }
        }
    }

    const double multiplier = (rightWidthTotal + (1.0 - widthTotal)) / rightWidthTotal;

    for(int section{0}; section < sectionCount; ++section) {
        if(!m_self->isSectionHidden(section) && (sections.empty() || sections.contains(section))) {
            m_sectionWidths[section] *= multiplier;
        }
    }
}

void AutoHeaderViewPrivate::updateWidths(const SectionIndexes& sections) const
{
    if(!m_stretchEnabled) {
        return;
    }

    const int sectionCount = static_cast<int>(m_sectionWidths.size());
    const int headerWidth  = m_self->width();

    for(int section{0}; section < sectionCount; ++section) {
        const int logical = m_self->logicalIndex(section);

        if(logical < 0 || logical >= sectionCount) {
            continue;
        }

        const bool visible           = !m_self->isSectionHidden(logical);
        const double normalisedWidth = m_sectionWidths.at(logical);
        const int width              = !visible ? 0 : static_cast<int>(normalisedWidth * headerWidth);

        if(!sections.empty() && !sections.contains(logical)) {
            continue;
        }

        if(width > 0) {
            m_self->resizeSection(logical, width);
        }
    }
}

void AutoHeaderViewPrivate::sectionResized(int resizedSection, int newSize)
{
    if(!m_stretchEnabled || m_sectionWidths.empty()) {
        return;
    }

    // Resize section to right
    if(m_state == SectionState::Resizing) {
        m_sectionWidths[resizedSection] = static_cast<double>(newSize) / m_self->width();

        SectionIndexes sectionToResize;

        const int resizedIndex = m_self->visualIndex(resizedSection);
        const int rightSection = rightLogicalSection(resizedIndex);
        if(rightSection >= 0) {
            sectionToResize.push_back(rightSection);
        }

        if(!sectionToResize.empty()) {
            m_state = SectionState::None;

            normaliseWidths(sectionToResize);
            updateWidths(sectionToResize);

            m_state = SectionState::Resizing;
        }
    }
}

int AutoHeaderViewPrivate::sectionHandleAt(int pos) const
{
    int visualIndex = m_self->visualIndexAt(pos);

    if(visualIndex == -1) {
        return -1;
    }

    const int logicalIndex = m_self->logicalIndex(visualIndex);
    const int viewportPos  = m_self->sectionViewportPosition(logicalIndex);
    const int gripMargin   = m_self->style()->pixelMetric(QStyle::PM_HeaderGripMargin, nullptr, m_self) * 2;

    bool left  = pos < viewportPos + gripMargin;
    bool right = (pos > viewportPos + m_self->sectionSize(logicalIndex) - gripMargin);

    if(m_self->orientation() == Qt::Horizontal && m_self->isRightToLeft()) {
        std::swap(left, right);
    }

    if(left) {
        while(visualIndex > -1) {
            const int leftIndex = m_self->logicalIndex(--visualIndex);
            if(!m_self->isSectionHidden(leftIndex)) {
                return m_self->visualIndex(leftIndex);
            }
        }
    }
    else if(right) {
        return visualIndex;
    }

    return -1;
}

int AutoHeaderViewPrivate::rightLogicalSection(int visual) const
{
    const int count = m_self->count();
    for(int i{visual + 1}; i < count; ++i) {
        const int logical = m_self->logicalIndex(i);
        if(!m_self->isSectionHidden(logical)) {
            return logical;
        }
    }
    return -1;
}

bool AutoHeaderViewPrivate::shouldBlockResize(const int oldPos) const
{
    if(!m_stretchEnabled) {
        return false;
    }

    const int rightSection  = rightLogicalSection(m_resizingSection);
    const int rightSize     = m_self->sectionSize(rightSection);
    const int rightBoundary = m_self->sectionViewportPosition(rightSection) + rightSize;
    const int endBoundary   = (m_self->orientation() == Qt::Horizontal) ? m_self->width() : m_self->height();

    const bool posPastEnd          = m_lastResizePos >= endBoundary - MinSectionWidth;
    const bool posPastRightSection = m_resizingSection >= 0 && m_lastResizePos >= rightBoundary - MinSectionWidth;
    const bool sectionAtMinSize    = rightSize <= MinSectionWidth;
    const bool isResizingSmaller   = m_lastResizePos >= oldPos;

    return (posPastEnd || posPastRightSection || (sectionAtMinSize && isResizingSmaller));
}

void AutoHeaderViewPrivate::columnsAboutToBeRemoved(int first, int last)
{
    const auto count = static_cast<int>(m_sectionWidths.size());
    if(first >= 0 && first < count && last >= 0 && last < count) {
        m_sectionWidths.remove(first, last - first + 1);
    }
}

void AutoHeaderViewPrivate::sectionCountChanged(int oldCount, int newCount)
{
    if(m_pendingColumns > 0 && !m_pendingState.isEmpty() && newCount == m_pendingColumns) {
        m_self->restoreHeaderState(m_pendingState);
        return;
    }

    if(!m_stretchEnabled || newCount == oldCount) {
        return;
    }

    if(newCount > oldCount) {
        m_sectionWidths.resize(newCount);
        const auto ratio = static_cast<double>(newCount - oldCount) / static_cast<double>(newCount);
        for(int section{oldCount}; section < newCount; ++section) {
            m_sectionWidths[section] = ratio;
        }
    }

    normaliseWidths();
    updateWidths();
}

AutoHeaderView::AutoHeaderView(Qt::Orientation orientation, QWidget* parent)
    : QHeaderView{orientation, parent}
    , p{std::make_unique<AutoHeaderViewPrivate>(this)}
{
    setMinimumSectionSize(MinSectionWidth);
    setTextElideMode(Qt::ElideRight);

    QObject::connect(this, &QHeaderView::sectionResized, this,
                     [this](int section, int /*oldSize*/, int newSize) { p->sectionResized(section, newSize); });
    QObject::connect(this, &QHeaderView::sectionCountChanged, this,
                     [this](int oldCount, int newCount) { p->sectionCountChanged(oldCount, newCount); });
}

void AutoHeaderView::setModel(QAbstractItemModel* model)
{
    if(!model) {
        return;
    }

    QHeaderView::setModel(model);

    QObject::connect(
        model, &QAbstractItemModel::columnsAboutToBeRemoved, this,
        [this](const QModelIndex& /*parent*/, int first, int last) { p->columnsAboutToBeRemoved(first, last); });
    QObject::connect(model, &QAbstractItemModel::columnsRemoved, this, [this]() {
        p->normaliseWidths();
        p->updateWidths();
    });
}

AutoHeaderView::~AutoHeaderView() = default;

void AutoHeaderView::resetSections()
{
    const int sectionCount = count();

    if(sectionCount > 0) {
        const int headerWidth = width();

        const auto width = headerWidth / sectionCount;

        for(int section{0}; section < sectionCount; ++section) {
            resizeSection(section, width);
        }

        if(p->m_stretchEnabled) {
            p->calculateSectionWidths();
            p->normaliseWidths();
            p->updateWidths();
        }
    }
}

void AutoHeaderView::resetSectionPositions()
{
    const int sectionCount = count();

    for(int section{0}; section < sectionCount; ++section) {
        setSectionHidden(section, false);
        moveSection(visualIndex(section), section);
    }
}

void AutoHeaderView::hideHeaderSection(int logical)
{
    const int sectionCount = count();

    bool allHidden{true};
    for(int section{0}; section < sectionCount; ++section) {
        if(section != logical && !isSectionHidden(section) && sectionSize(section) > 0) {
            allHidden = false;
            break;
        }
    }

    if(allHidden) {
        return;
    }

    hideSection(logical);
    emit sectionVisiblityChanged(logical);

    if(!p->m_stretchEnabled) {
        return;
    }

    p->normaliseWidths();
    p->updateWidths();
}

void AutoHeaderView::showHeaderSection(int logical)
{
    if(!p->m_stretchEnabled) {
        showSection(logical);
        return;
    }

    showSection(logical);
    emit sectionVisiblityChanged(logical);

    p->normaliseWidths();
    p->updateWidths();
}

void AutoHeaderView::setHeaderSectionHidden(int logical, bool hidden)
{
    if(hidden) {
        hideHeaderSection(logical);
    }
    else {
        showHeaderSection(logical);
    }
}

void AutoHeaderView::setHeaderSectionWidth(int logical, double width)
{
    if(!p->m_stretchEnabled) {
        return;
    }

    p->m_sectionWidths.resize(logical + 1);
    p->m_sectionWidths[logical] = width;

    const int sectionCount = count();

    SectionIndexes rightColumns;

    for(int section{0}; section < sectionCount; ++section) {
        if(!isSectionHidden(section) && section >= logical) {
            rightColumns.push_back(section);
        }
    }

    p->normaliseWidths(rightColumns);
    p->updateWidths();
}

void AutoHeaderView::setHeaderSectionWidths(const std::map<int, double>& widths)
{
    if(!p->m_stretchEnabled) {
        return;
    }

    for(const auto& [logical, width] : widths) {
        if(!isSectionHidden(logical)) {
            p->m_sectionWidths.resize(logical + 1);
            p->m_sectionWidths[logical] = width;
        }
    }

    p->normaliseWidths();
    p->updateWidths();
}

void AutoHeaderView::setHeaderSectionAlignment(int logical, Qt::Alignment alignment)
{
    const QModelIndex index = model()->index(0, logical);
    if(index.isValid()) {
        model()->setData(index, alignment.toInt(), Qt::TextAlignmentRole);
    }
}

bool AutoHeaderView::isStretchEnabled() const
{
    return p->m_stretchEnabled;
}

void AutoHeaderView::setStretchEnabled(bool enabled)
{
    if(std::exchange(p->m_stretchEnabled, enabled) != enabled && enabled) {
        if(p->m_sectionWidths.empty()) {
            resetSections();
        }
        else {
            p->normaliseWidths();
            p->updateWidths();
        }
    }
    emit stretchChanged(enabled);
}

void AutoHeaderView::addHeaderContextMenu(QMenu* menu, const QPoint& pos)
{
    auto* toggleStretch = new QAction(tr("Auto-size sections"), menu);
    toggleStretch->setCheckable(true);
    toggleStretch->setChecked(p->m_stretchEnabled);
    QObject::connect(toggleStretch, &QAction::triggered, this, [this]() { setStretchEnabled(!p->m_stretchEnabled); });
    menu->addAction(toggleStretch);

    menu->addSeparator();

    auto* showMenu = new QMenu(tr("Show Hidden"), menu);

    const int sectionCount = count();
    const int hiddenCount  = hiddenSectionCount();

    if(!pos.isNull()) {
        const int logical = logicalIndexAt(mapFromGlobal(pos));
        if(logical >= 0 && logical < sectionCount) {
            auto* hideSection
                = new QAction(tr("Hide %1").arg(model()->headerData(logical, orientation()).toString()), menu);
            QObject::connect(hideSection, &QAction::triggered, this, [this, logical]() { hideHeaderSection(logical); });
            hideSection->setEnabled(sectionCount - hiddenCount > 1);
            menu->addAction(hideSection);
        }
    }

    for(int section{0}; section < sectionCount; ++section) {
        const int logical = logicalIndex(section);
        if(isSectionHidden(logical)) {
            auto* showSection = new QAction(model()->headerData(logical, orientation()).toString(), showMenu);
            QObject::connect(showSection, &QAction::triggered, this, [this, logical]() { showHeaderSection(logical); });
            showMenu->addAction(showSection);
        }
    }

    showMenu->setEnabled(hiddenCount > 0);

    menu->addMenu(showMenu);
}

void AutoHeaderView::addHeaderAlignmentMenu(QMenu* menu, const QPoint& pos)
{
    auto* alignMenu = new QMenu(tr("Alignment"), menu);

    const int logical = logicalIndexAt(mapFromGlobal(pos));

    if(logical < 0 || logical >= count()) {
        return;
    }

    auto* alignmentGroup = new QActionGroup(alignMenu);

    auto* alignLeft   = new QAction(tr("&Left"), alignMenu);
    auto* alignCentre = new QAction(tr("&Centre"), alignMenu);
    auto* alignRight  = new QAction(tr("&Right"), alignMenu);

    alignLeft->setCheckable(true);
    alignCentre->setCheckable(true);
    alignRight->setCheckable(true);

    const auto currentAlignment = model()->headerData(logical, orientation(), SectionAlignment).value<Qt::Alignment>();

    if(currentAlignment & Qt::AlignLeft) {
        alignLeft->setChecked(true);
    }
    else if(currentAlignment & Qt::AlignHCenter) {
        alignCentre->setChecked(true);
    }
    else if(currentAlignment & Qt::AlignRight) {
        alignRight->setChecked(true);
    }

    auto changeAlignment = [this, logical](Qt::Alignment alignment) {
        model()->setHeaderData(logical, orientation(), alignment.toInt(), SectionAlignment);
    };

    QObject::connect(alignLeft, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignLeft); });
    QObject::connect(alignCentre, &QAction::triggered, this,
                     [changeAlignment]() { changeAlignment(Qt::AlignHCenter); });
    QObject::connect(alignRight, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignRight); });

    alignmentGroup->addAction(alignLeft);
    alignmentGroup->addAction(alignCentre);
    alignmentGroup->addAction(alignRight);

    alignMenu->addAction(alignLeft);
    alignMenu->addAction(alignCentre);
    alignMenu->addAction(alignRight);

    menu->addMenu(alignMenu);
}

QByteArray AutoHeaderView::saveHeaderState() const
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    const int sectionCount = count();
    SectionIndexes pixelWidths(sectionCount);
    SectionIndexes logicalIndexes(sectionCount);

    for(int section{0}; section < sectionCount; ++section) {
        pixelWidths[section]    = sectionSize(section);
        logicalIndexes[section] = logicalIndex(section);
    }

    const auto sortOrder  = static_cast<int>(sortIndicatorOrder());
    const int sortSection = sortIndicatorSection();

    stream << pixelWidths;
    stream << logicalIndexes;
    stream << p->m_sectionWidths;
    stream << p->m_stretchEnabled;
    stream << sortOrder;
    stream << sortSection;

    return result;
}

void AutoHeaderView::restoreHeaderState(const QByteArray& state)
{
    Qt::SortOrder sortOrder{Qt::AscendingOrder};
    int sortSection{0};

    if(!state.isEmpty()) {
        SectionIndexes pixelWidths;
        SectionIndexes logicalIndexes;

        QDataStream stream{state};
        stream.setVersion(QDataStream::Qt_6_0);

        stream >> pixelWidths;

        if(std::cmp_not_equal(pixelWidths.size(), count())) {
            p->m_pendingColumns = static_cast<int>(pixelWidths.size());
            p->m_pendingState   = state;
            return;
        }

        stream >> logicalIndexes;
        stream >> p->m_sectionWidths;
        stream >> p->m_stretchEnabled;
        stream >> sortOrder;
        stream >> sortSection;

        const int sectionCount
            = std::min({count(), static_cast<int>(logicalIndexes.size()), static_cast<int>(pixelWidths.size()),
                        static_cast<int>(p->m_sectionWidths.size())});

        qCDebug(AUTO_HEADER) << "Restoring state for" << sectionCount << "section(s)";

        for(int section{0}; section < sectionCount; ++section) {
            setSectionHidden(section, pixelWidths[section] < MinSectionWidth);
            moveSection(visualIndex(logicalIndexes[section]), section);

            if(!p->m_stretchEnabled) {
                resizeSection(section, pixelWidths[section]);
            }
        }
    }
    else {
        qCDebug(AUTO_HEADER) << "Header state empty";
    }

    setSortIndicator(sortSection, sortOrder);

    const int sectionCount = count();
    if(sectionCount > 0) {
        const int widthCount = static_cast<int>(p->m_sectionWidths.size());
        if(widthCount < sectionCount) {
            p->m_sectionWidths.resize(sectionCount);
        }

        if(p->m_stretchEnabled) {
            p->updateWidths();
        }
    }

    p->m_pendingColumns = 0;
    p->m_pendingState.clear();

    emit stateRestored();
}

void AutoHeaderView::mousePressEvent(QMouseEvent* event)
{
    if(p->m_state != SectionState::None || event->button() != Qt::LeftButton) {
        return;
    }

    const QPoint position = event->position().toPoint();
    const int pos         = orientation() == Qt::Horizontal ? position.x() : position.y();
    const int handleIndex = p->sectionHandleAt(pos);

    const bool validHandle = handleIndex >= 0 && handleIndex < count() - 1;

    if(handleIndex == -1) {
        p->m_firstPressed = logicalIndexAt(pos);
        p->m_mouseTarget  = -1;
        p->m_mouseSection = p->m_firstPressed;
        if(p->m_firstPressed >= 0) {
            p->m_state = SectionState::Moving;
        }
    }

    if(!p->m_stretchEnabled || handleIndex == -1 || validHandle) {
        QHeaderView::mousePressEvent(event);
    }

    if(handleIndex >= 0) {
        p->m_state = SectionState::Resizing;
    }

    p->m_firstPos = pos;
}

void AutoHeaderView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();
    const int oldPos = std::exchange(p->m_lastResizePos, orientation() == Qt::Horizontal ? position.x() : position.y());

    if(p->m_state == SectionState::Resizing && p->m_resizingSection < 0) {
        p->m_resizingSection = p->sectionHandleAt(p->m_lastResizePos);
    }

    if(p->m_stretchEnabled && p->m_resizingSection == count() - 1) {
        return;
    }

    if(event->buttons() == Qt::NoButton) {
        p->m_state           = SectionState::None;
        p->m_firstPressed    = -1;
        p->m_resizingSection = -1;
    }

    if(p->m_state == SectionState::Resizing) {
        if(p->shouldBlockResize(oldPos)) {
            return;
        }
    }
    else if(p->m_state == SectionState::Moving) {
        if(std::abs(p->m_lastResizePos - p->m_firstPos) >= QApplication::startDragDistance()) {
            const int visual = visualIndexAt(p->m_lastResizePos);
            if(visual >= 0) {
                const int moving = visualIndex(p->m_mouseSection);
                if(visual == moving) {
                    p->m_mouseTarget = p->m_mouseSection;
                }
                p->m_moving = (p->m_mouseSection >= 0 && p->m_mouseTarget >= 0);
            }
        }
    }

    QHeaderView::mouseMoveEvent(event);
}

void AutoHeaderView::mouseReleaseEvent(QMouseEvent* event)
{
    if(p->m_state == SectionState::Moving && !p->m_moving) {
        p->m_state = SectionState::None;
    }

    if(p->m_state == SectionState::None) {
        const QPoint pos       = event->position().toPoint();
        const int handleIndex  = p->sectionHandleAt(orientation() == Qt::Horizontal ? pos.x() : pos.y());
        const int logicalIndex = logicalIndexAt(pos);

        if(logicalIndex >= 0 && handleIndex == -1) {
            emit leftClicked(logicalIndex, pos);
        }
    }
    else if(p->m_state == SectionState::Resizing) {
        p->normaliseWidths();
        p->updateWidths();
    }

    p->m_firstPressed    = -1;
    p->m_resizingSection = -1;
    p->m_moving          = false;
    p->m_state           = SectionState::None;

    QHeaderView::mouseReleaseEvent(event);
}

void AutoHeaderView::mouseDoubleClickEvent(QMouseEvent* event)
{
    p->m_state = SectionState::Resizing;

    QHeaderView::mouseDoubleClickEvent(event);

    p->m_state = SectionState::None;
}

void AutoHeaderView::resizeEvent(QResizeEvent* event)
{
    QHeaderView::resizeEvent(event);

    if(p->m_stretchEnabled) {
        p->updateWidths();
    }
}
} // namespace Fooyin

#include "utils/widgets/moc_autoheaderview.cpp"
