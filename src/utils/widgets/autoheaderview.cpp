/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <utils/helpers.h>

#include <QApplication>
#include <QIODevice>
#include <QMenu>
#include <QMouseEvent>

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

struct AutoHeaderView::Private
{
    AutoHeaderView* self;

    bool stretchEnabled{false};
    SectionWidths sectionWidths;

    SectionState state{NoState};
    int firstPos{-1};
    int target{-1};
    int section{-1};
    int firstPressed{-1};
    bool moving{false};

    explicit Private(AutoHeaderView* self)
        : self{self}
    { }

    void calculateSectionWidths()
    {
        if(!stretchEnabled) {
            return;
        }

        sectionWidths.clear();

        const int sectionCount = self->count();
        const int headerWidth  = self->width();

        sectionWidths.resize(sectionCount);

        for(int section{0}; section < sectionCount; ++section) {
            const auto width       = static_cast<double>(self->sectionSize(section)) / headerWidth;
            sectionWidths[section] = width;
        }
    }

    void normaliseWidths(const SectionIndexes& sections = {})
    {
        if(!stretchEnabled) {
            return;
        }

        double widthTotal{0.0};

        const int sectionCount = static_cast<int>(sectionWidths.size());
        for(int section{0}; section < sectionCount; ++section) {
            if(!self->isSectionHidden(section)) {
                widthTotal += sectionWidths.at(section);
            }
        }

        if(widthTotal == 0.0 || widthTotal == 1.0) {
            return;
        }

        double rightWidthTotal{widthTotal};

        if(!sections.empty()) {
            rightWidthTotal = 0.0;

            for(int section{0}; section < sectionCount; ++section) {
                if(sections.contains(section)) {
                    rightWidthTotal += sectionWidths.at(section);
                }
            }
        }

        const double multiplier = (rightWidthTotal + (1.0 - widthTotal)) / rightWidthTotal;

        for(int section{0}; section < sectionCount; ++section) {
            if(sections.empty() || sections.contains(section)) {
                sectionWidths[section] *= multiplier;
            }
        }
    }

    [[nodiscard]] int lastVisibleIndex() const
    {
        const int sectionCount = static_cast<int>(sectionWidths.size());

        for(int section{sectionCount - 1}; section >= 0; --section) {
            const int logical = self->logicalIndex(section);
            if(!self->isSectionHidden(logical)) {
                return logical;
            }
        }
        return -1;
    }

    void updateWidths(const SectionIndexes& sections = {}) const
    {
        if(!stretchEnabled) {
            return;
        }

        const int sectionCount = static_cast<int>(sectionWidths.size());
        const int finalRow     = lastVisibleIndex();

        int totalWidth{0};

        for(int section{0}; section < sectionCount; ++section) {
            const int logical = self->logicalIndex(section);

            if(logical < 0 || logical >= sectionCount) {
                continue;
            }

            const bool visible           = !self->isSectionHidden(logical);
            const double normalisedWidth = sectionWidths.at(logical);
            const int headerWidth        = self->width();
            const int width
                = logical == finalRow ? (headerWidth - totalWidth) : static_cast<int>(normalisedWidth * headerWidth);

            if(visible) {
                totalWidth += width;
            }

            if(!sections.empty() && !sections.contains(logical)) {
                continue;
            }

            if(!visible && width > MinSectionWidth) {
                self->showSection(logical);
            }

            if(width > 0) {
                self->resizeSection(logical, width);
            }
        }
    }

    void sectionResized(int resizedSection, int newSize)
    {
        if(!stretchEnabled || sectionWidths.empty()) {
            return;
        }

        // Resize sections to right of section
        if(state == SectionState::Resizing) {
            sectionWidths[resizedSection] = static_cast<double>(newSize) / self->width();

            SectionIndexes sectionToResize;

            const int resizedIndex = self->visualIndex(resizedSection);
            const int sectionCount = self->count();

            for(int section{0}; section < sectionCount; ++section) {
                const int visualIndex = self->visualIndex(section);
                if(visualIndex > resizedIndex) {
                    if(self->isSectionHidden(section)) {
                        sectionWidths[section] = 0.01;
                    }
                    sectionToResize.push_back(section);
                }
            }

            if(!sectionToResize.empty()) {
                state = SectionState::None;
                normaliseWidths(sectionToResize);
                updateWidths(sectionToResize);
                state = SectionState::Resizing;
            }
        }
    }

    [[nodiscard]] int sectionHandleAt(int pos) const
    {
        int visualIndex = self->visualIndexAt(pos);

        if(visualIndex == -1) {
            return -1;
        }

        const int logicalIndex = self->logicalIndex(visualIndex);
        const int viewportPos  = self->sectionViewportPosition(logicalIndex);
        const int gripMargin   = self->style()->pixelMetric(QStyle::PM_HeaderGripMargin, nullptr, self);

        bool left  = pos < viewportPos + gripMargin;
        bool right = (pos > viewportPos + self->sectionSize(logicalIndex) - gripMargin);

        if(self->orientation() == Qt::Horizontal && self->isRightToLeft()) {
            std::swap(left, right);
        }

        if(left) {
            while(visualIndex > -1) {
                const int leftIndex = self->logicalIndex(--visualIndex);
                if(!self->isSectionHidden(leftIndex)) {
                    return self->visualIndex(leftIndex);
                }
            }
        }
        else if(right) {
            return visualIndex;
        }

        return -1;
    }
};

AutoHeaderView::AutoHeaderView(Qt::Orientation orientation, QWidget* parent)
    : QHeaderView{orientation, parent}
    , p{std::make_unique<Private>(this)}
{
    setMinimumSectionSize(MinSectionWidth);

    QObject::connect(this, &QHeaderView::sectionResized, this,
                     [this](int section, int /*oldSize*/, int newSize) { p->sectionResized(section, newSize); });

    QObject::connect(this, &QHeaderView::sectionCountChanged, this, [this](int /*oldCount*/, int newCount) {
        if(p->stretchEnabled && newCount != static_cast<int>(p->sectionWidths.size())) {
            const int sectionCount = count();
            for(int section{0}; section < sectionCount; ++section) {
                if(isSectionHidden(section)) {
                    showHeaderSection(section);
                }
            }
            resetSections();
        }
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

        if(p->stretchEnabled) {
            p->calculateSectionWidths();
            p->normaliseWidths();
            p->updateWidths();
        }
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

    if(!p->stretchEnabled) {
        hideSection(logical);
        return;
    }

    p->sectionWidths[logical] = 0;
    hideSection(logical);
    resetSections();
}

void AutoHeaderView::showHeaderSection(int logical)
{
    if(!p->stretchEnabled) {
        showSection(logical);
        return;
    }

    const int sectionCount = count();

    int sectionsVisible{0};
    for(int section{0}; section < sectionCount; ++section) {
        if(!isSectionHidden(section)) {
            ++sectionsVisible;
        }
    }

    const int sectionWidth = static_cast<int>(sectionsVisible == 0 ? 1.0 : (1.0 / sectionsVisible) * width());

    showSection(logical);
    resizeSection(logical, sectionWidth);
    resetSections();
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
    if(!p->stretchEnabled) {
        return;
    }

    p->sectionWidths.resize(logical + 1);
    p->sectionWidths[logical] = width;

    const int sectionCount = count();

    SectionIndexes otherColumns;

    for(int section{0}; section < sectionCount; ++section) {
        if(!isSectionHidden(section) && section != logical) {
            otherColumns.push_back(section);
        }
    }

    p->normaliseWidths(otherColumns);
    p->updateWidths();
}

bool AutoHeaderView::isStretchEnabled() const
{
    return p->stretchEnabled;
}

void AutoHeaderView::setStretchEnabled(bool enabled)
{
    if(std::exchange(p->stretchEnabled, enabled) != enabled && enabled) {
        resetSections();
    }
    emit stretchChanged(enabled);
}

void AutoHeaderView::addHeaderContextMenu(QMenu* menu, const QPoint& pos)
{
    auto* toggleStretch = new QAction(tr("Auto-size sections"), menu);
    toggleStretch->setCheckable(true);
    toggleStretch->setChecked(p->stretchEnabled);
    QObject::connect(toggleStretch, &QAction::triggered, this, [this]() { setStretchEnabled(!p->stretchEnabled); });
    menu->addAction(toggleStretch);

    menu->addSeparator();

    auto* showMenu = new QMenu(tr("Show Hidden"), menu);

    const int sectionCount = count();
    const int hiddenCount  = hiddenSectionCount();

    if(!pos.isNull()) {
        const int logical = logicalIndexAt(mapFromGlobal(pos));
        if(logical >= 0 && logical < sectionCount) {
            auto* hideSection = new QAction(tr("Hide ") + model()->headerData(logical, orientation()).toString(), menu);
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

QByteArray AutoHeaderView::saveHeaderState() const
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_5);

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
    stream << p->sectionWidths;
    stream << p->stretchEnabled;
    stream << sortOrder;
    stream << sortSection;

    return result;
}

void AutoHeaderView::restoreHeaderState(const QByteArray& data)
{
    Qt::SortOrder sortOrder{Qt::AscendingOrder};
    int sortSection{0};

    if(!data.isEmpty()) {
        SectionIndexes pixelWidths;
        SectionIndexes logicalIndexes;

        QDataStream stream{data};
        stream.setVersion(QDataStream::Qt_6_5);

        stream >> pixelWidths;
        stream >> logicalIndexes;
        stream >> p->sectionWidths;
        stream >> p->stretchEnabled;
        stream >> sortOrder;
        stream >> sortSection;

        const int sectionCount
            = std::min({count(), static_cast<int>(logicalIndexes.size()), static_cast<int>(pixelWidths.size()),
                        static_cast<int>(p->sectionWidths.size())});

        for(int section{0}; section < sectionCount; ++section) {
            setSectionHidden(section, pixelWidths[section] <= MinSectionWidth);
            moveSection(visualIndex(logicalIndexes[section]), section);

            if(!p->stretchEnabled) {
                resizeSection(section, pixelWidths[section]);
            }
        }
    }

    setSortIndicator(sortSection, sortOrder);

    const int sectionCount = count();
    if(sectionCount > 0) {
        const int widthCount = static_cast<int>(p->sectionWidths.size());
        if(widthCount < sectionCount) {
            p->sectionWidths.resize(sectionCount);
        }

        if(p->stretchEnabled) {
            p->updateWidths();
        }
    }
}

void AutoHeaderView::mousePressEvent(QMouseEvent* event)
{
    if(p->state != SectionState::None || event->button() != Qt::LeftButton) {
        return;
    }

    const QPoint position = event->pos();
    const int pos         = orientation() == Qt::Horizontal ? position.x() : position.y();
    const int handleIndex = p->sectionHandleAt(pos);

    const bool validHandle = handleIndex >= 0 && handleIndex < count() - 1;

    if(handleIndex == -1) {
        p->firstPressed = logicalIndexAt(pos);
        p->target       = -1;
        p->section      = p->firstPressed;
        if(p->firstPressed >= 0) {
            p->state = SectionState::Moving;
        }
    }

    if(!p->stretchEnabled || handleIndex == -1 || validHandle) {
        QHeaderView::mousePressEvent(event);
    }

    if(handleIndex >= 0) {
        p->state = SectionState::Resizing;
    }

    p->firstPos = pos;
}

void AutoHeaderView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint position = event->pos();
    const int pos         = orientation() == Qt::Horizontal ? position.x() : position.y();
    const int handleIndex = p->sectionHandleAt(pos);

    if(p->stretchEnabled && handleIndex == count() - 1) {
        return;
    }

    if(event->buttons() == Qt::NoButton) {
        p->state        = SectionState::None;
        p->firstPressed = -1;
    }

    if(p->state == SectionState::Resizing) {
        const int rightBoundary = (orientation() == Qt::Horizontal) ? width() : height();
        const int gripMargin    = style()->pixelMetric(QStyle::PM_HeaderGripMargin) / 2;

        if(p->stretchEnabled && (pos + gripMargin) > rightBoundary) {
            return;
        }
    }
    else if(p->state == SectionState::Moving) {
        if(std::abs(pos - p->firstPos) >= QApplication::startDragDistance()) {
            const int visual = visualIndexAt(pos);
            if(visual >= 0) {
                const int moving = visualIndex(p->section);
                if(visual == moving) {
                    p->target = p->section;
                }
                p->moving = (p->section >= 0 && p->target >= 0);
            }
        }
    }

    QHeaderView::mouseMoveEvent(event);
}

void AutoHeaderView::mouseReleaseEvent(QMouseEvent* event)
{
    if(p->state == SectionState::Moving && !p->moving) {
        p->state = SectionState::None;
    }

    if(p->state == SectionState::None) {
        const QPoint pos       = event->pos();
        const int handleIndex  = p->sectionHandleAt(orientation() == Qt::Horizontal ? pos.x() : pos.y());
        const int logicalIndex = logicalIndexAt(pos);

        if(logicalIndex >= 0 && handleIndex == -1) {
            emit leftClicked(logicalIndex, pos);
        }
    }
    else if(p->state == SectionState::Resizing) {
        const int sectionCount = static_cast<int>(p->sectionWidths.size());

        for(int section{0}; section < sectionCount; ++section) {
            const double width   = p->sectionWidths.at(section);
            const int pixelWidth = static_cast<int>(width * this->width());

            if(pixelWidth < (MinSectionWidth / 2) && !isSectionHidden(section)) {
                hideSection(section);
            }
        }

        p->normaliseWidths();
        p->updateWidths();
    }

    p->firstPressed = -1;
    p->moving       = false;
    p->state        = SectionState::None;

    QHeaderView::mouseReleaseEvent(event);
}

void AutoHeaderView::mouseDoubleClickEvent(QMouseEvent* event)
{
    p->state = SectionState::Resizing;

    QHeaderView::mouseDoubleClickEvent(event);

    p->state = SectionState::None;
}

void AutoHeaderView::resizeEvent(QResizeEvent* event)
{
    QHeaderView::resizeEvent(event);

    if(p->stretchEnabled) {
        p->updateWidths();
    }
}
} // namespace Fooyin

#include "utils/widgets/moc_autoheaderview.cpp"
