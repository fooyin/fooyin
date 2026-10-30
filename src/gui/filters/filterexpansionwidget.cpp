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

#include "filterexpansionwidget.h"

#include "artwork/artworkreflection.h"
#include "filterexpansionmodel.h"
#include "filterexpansionpopulator.h"
#include "filterexpansiontrackdelegate.h"

#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/backgroundimagesource.h>
#include <gui/widgets/expandedtreeview.h>
#include <gui/widgets/widgetbackground.h>
#include <utils/actions/widgetcontext.h>
#include <utils/tooltipfilter.h>
#include <utils/utils.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyleOptionViewItem>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>
#include <set>

using namespace Qt::StringLiterals;

constexpr auto PointerHeight              = 8;
constexpr auto ContentMargin              = 10;
constexpr auto ColumnWidthReferenceGlyph  = u'M';
constexpr auto DefaultColumnWidthInGlyphs = 45.0;
constexpr auto FullPercentage             = 100.0;

namespace Fooyin::Filters {
namespace {
QColor expansionBackground(const QPalette& palette, QPalette::ColorGroup group)
{
    const QColor base            = palette.color(group, QPalette::Base);
    const QColor highlight       = palette.color(group, QPalette::Highlight);
    static constexpr double Tint = 0.2;
    return QColor::fromRgbF((base.redF() * (1.0 - Tint)) + (highlight.redF() * Tint),
                            (base.greenF() * (1.0 - Tint)) + (highlight.greenF() * Tint),
                            (base.blueF() * (1.0 - Tint)) + (highlight.blueF() * Tint));
}

class ExpansionTrackView : public ExpandedTreeView
{
public:
    using ExpandedTreeView::ExpandedTreeView;
};
} // namespace

class ExpansionFooterWidget : public QWidget
{
public:
    using QWidget::QWidget;

    void setContent(const RichTextLayout& layout)
    {
        m_layout = layout;

        QStringList description;
        for(const auto& line : layout.lines) {
            description.push_back(line.left.joinedText() + u" "_s + line.right.joinedText());
        }
        setFixedHeight(layout.height);
        setVisible(layout.height > 0);
        updateGeometry();
        update();
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        return {0, m_layout.height};
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter{this};
        QStyleOptionViewItem option;
        option.initFrom(this);
        option.font = font();
        drawRichTextLayout(&painter, option, rect().adjusted(4, 0, -4, 0), m_layout);
    }

private:
    RichTextLayout m_layout;
};

FilterExpansionWidget::FilterExpansionWidget(TrackSelectionController* selection, CoverRepository* coverRepo,
                                             QWidget* parent)
    : QWidget{parent}
    , m_selection{selection}
    , m_coverProvider{coverRepo ? new CoverProvider(coverRepo, this) : nullptr}
    , m_backgroundSource{new BackgroundImageSource(coverRepo ? new CoverProvider(coverRepo, this) : nullptr, this)}
    , m_background{new WidgetBackground(this)}
    , m_context{new WidgetContext(this, Context{Constants::Context::TrackSelection}, this)}
    , m_runningGeneration{0}
    , m_workerBusy{false}
    , m_model{new ExpansionTrackModel(this)}
    , m_tracks{new ExpansionTrackView(this)}
    , m_footer{new ExpansionFooterWidget(this)}
    , m_close{new QToolButton(this)}
    , m_coverType{Track::Cover::Front}
    , m_showArtwork{true}
    , m_artworkOnLeft{true}
    , m_artworkReflection{false}
    , m_artworkAutoMargins{false}
    , m_showCloseButton{true}
    , m_previewMode{false}
    , m_layoutPending{false}
    , m_scrollAnchorY{0}
    , m_scrollAnchorScroll{0}
    , m_minimumColumnRows{4}
    , m_maximumColumns{0}
    , m_stretchColumns{false}
    , m_minimumColumnWidth{100}
    , m_artworkSize{100}
    , m_artworkMargins{10, 10, 10, 10}
    , m_effectiveArtworkMargins{10, 10, 10, 10}
    , m_coverRequest{0}
{
    m_populator.moveToThread(&m_populatorThread);

    setFocusProxy(m_tracks);
    auto policy{sizePolicy()};
    policy.setHeightForWidth(true);
    setSizePolicy(policy);

    m_tracks->setModel(m_model);
    m_tracks->setViewMode(ExpandedTreeView::ViewMode::Flow);

    m_tracks->setItemDelegate(new ExpansionTrackDelegate(m_tracks));
    m_tracks->setFrameShape(QFrame::NoFrame);
    m_tracks->setAutoFillBackground(false);
    m_tracks->viewport()->setAutoFillBackground(false);
    m_tracks->setProperty("transparent_base_rows", true);
    m_tracks->setHeaderHidden(true);
    m_tracks->header()->setSectionResizeMode(QHeaderView::Stretch);
    m_tracks->setIndentation(0);
    m_tracks->setFlowMinimumRows(4);
    m_tracks->setFlowStretchColumns(m_stretchColumns);
    m_tracks->setUniformRowHeights(false);
    m_tracks->setSelectIgnoreParents(true);
    m_tracks->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tracks->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tracks->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tracks->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tracks->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tracks->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tracks->setDragEnabled(true);
    m_tracks->setDragDropMode(QAbstractItemView::DragOnly);
    m_tracks->setDefaultDropAction(Qt::CopyAction);
    m_tracks->setMouseTracking(true);
    m_tracks->installEventFilter(this);
    m_tracks->viewport()->installEventFilter(this);
    m_tracks->viewport()->installEventFilter(new ToolTipFilter(this));

    m_close->setAutoRaise(true);
    Gui::setThemeIcon(m_close, Constants::Icons::Close);
    m_close->setToolTip(tr("Collapse group"));

    m_footer->setContent({});

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_tracks, 1);
    layout->addWidget(m_footer);

    QObject::connect(&m_populator, &FilterExpansionPopulator::ready, this, &FilterExpansionWidget::populated);
    QObject::connect(&m_populator, &Worker::finished, this, [this]() {
        m_workerBusy = false;
        if(m_pending) {
            auto request = std::move(*m_pending);
            m_pending.reset();
            submit(std::move(request));
        }
    });
    QObject::connect(m_model, &QAbstractItemModel::dataChanged, this,
                     [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
                         if(roles.empty() || roles.contains(Qt::SizeHintRole)) {
                             rememberScrollAnchor();
                             scheduleLayoutUpdate();
                         }
                     });
    QObject::connect(m_tracks, &ExpandedTreeView::contentSizeChanged, this,
                     &FilterExpansionWidget::scheduleLayoutUpdate);
    QObject::connect(m_close, &QToolButton::clicked, this, &FilterExpansionWidget::closeRequested);
    QObject::connect(m_tracks, &ExpandedTreeView::doubleClicked, this, &FilterExpansionWidget::activate);
    QObject::connect(m_tracks, &ExpandedTreeView::middleClicked, this, [this](const QModelIndex& index) {
        if(index.isValid() && !m_previewMode) {
            updateSelection();
            Q_EMIT middleClicked(index);
        }
    });
    QObject::connect(m_tracks->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &FilterExpansionWidget::updateSelection);
    QObject::connect(m_tracks->selectionModel(), &QItemSelectionModel::currentChanged, this,
                     [this](const QModelIndex& index) {
                         if(index.isValid() && m_tracks->hasFocus()) {
                             QRect trackRect = m_tracks->visualRect(index);
                             trackRect.moveTopLeft(m_tracks->viewport()->mapTo(this, trackRect.topLeft()));
                             Q_EMIT trackFocused(trackRect);
                         }
                     });

    QObject::connect(m_backgroundSource, &BackgroundImageSource::pixmapChanged, m_background,
                     &WidgetBackground::setPixmap);

    if(m_coverProvider) {
        QObject::connect(m_coverProvider, &CoverProvider::coverAdded, this, [this](const Track& track) {
            if(track.sameIdentityAs(m_coverTrack)) {
                updateArtwork();
            }
        });
        QObject::connect(m_coverProvider, &CoverProvider::placeholderChanged, this,
                         &FilterExpansionWidget::updateArtwork);
    }

    updatePalette();
    updateLayout();
}

FilterExpansionWidget::~FilterExpansionWidget()
{
    m_populator.closeThread();
    m_populatorThread.quit();
    m_populatorThread.wait();
}

void FilterExpansionWidget::submit(ExpansionRequest request)
{
    request.context.freeze();

    if(m_workerBusy) {
        if(request.generation != m_runningGeneration) {
            m_populator.stopThread();
        }

        if(m_pending && m_pending->generation == request.generation) {
            const bool allRows = ((m_pending->sections & ExpansionTracks) && m_pending->rows.empty())
                              || ((request.sections & ExpansionTracks) && request.rows.empty());
            request.sections |= m_pending->sections;

            if(allRows) {
                request.rows.clear();
            }
            else {
                std::set<int> rows{request.rows.cbegin(), request.rows.cend()};
                rows.insert(m_pending->rows.cbegin(), m_pending->rows.cend());
                request.rows.assign(rows.cbegin(), rows.cend());
            }

            request.rebuild |= m_pending->rebuild;
        }

        m_pending = std::move(request);
        return;
    }

    if(!m_populatorThread.isRunning()) {
        m_populatorThread.start();
    }

    m_workerBusy        = true;
    m_runningGeneration = request.generation;

    QMetaObject::invokeMethod(
        &m_populator,
        [worker = &m_populator, request = std::move(request), stop = m_populator.prepareWork()]() mutable {
            worker->run(std::move(request), stop);
        },
        Qt::QueuedConnection);
}

void FilterExpansionWidget::cancel()
{
    m_pending.reset();
    m_populator.stopThread();
    if(m_populatorThread.isRunning()) {
        QMetaObject::invokeMethod(&m_populator, &FilterExpansionPopulator::clearGroups, Qt::QueuedConnection);
    }
}

void FilterExpansionWidget::setContent(const ExpansionResult& content, bool preserveSelection)
{
    if(preserveSelection) {
        rememberScrollAnchor();
    }
    else {
        m_scrollAnchorKey.clear();
    }

    updateTracks(content, true, preserveSelection);
    m_footer->setContent(content.footer);
    scheduleLayoutUpdate();
    Q_EMIT contentChanged();
}

void FilterExpansionWidget::updateContent(const ExpansionResult& content)
{
    rememberScrollAnchor();

    const int oldHeight    = sizeHint().height();
    const bool rowsResized = updateTracks(content, false);

    if(content.sections & ExpansionFooter) {
        m_footer->setContent(content.footer);
    }
    if(rowsResized || oldHeight != sizeHint().height()) {
        scheduleLayoutUpdate();
        Q_EMIT contentChanged();
    }
    if(!m_layoutPending) {
        m_scrollAnchorKey.clear();
    }
}

void FilterExpansionWidget::setPreviewMode()
{
    m_previewMode = true;
    m_showArtwork = false;
    m_close->hide();
    m_tracks->setDragEnabled(false);
    m_tracks->setSelectionMode(QAbstractItemView::NoSelection);
    m_tracks->setFocusPolicy(Qt::NoFocus);
    updateLayout();
}

void FilterExpansionWidget::setBackground(const BackgroundConfig& config, const QString& colour)
{
    m_backgroundColour = colour;

    auto background{config};
    background.fadeDuration = 0;
    if(m_previewMode && background.imageMode == static_cast<int>(BackgroundImageMode::AlbumCover)) {
        background.imageMode = static_cast<int>(BackgroundImageMode::None);
    }
    m_background->setConfig(background);
    m_backgroundSource->setConfig(background);

    updatePalette();
}

void FilterExpansionWidget::setMinimumColumnRows(int rows)
{
    rows = std::max(1, rows);
    if(m_minimumColumnRows == rows) {
        return;
    }

    m_minimumColumnRows = rows;
    rememberScrollAnchor();
    m_tracks->setFlowMinimumRows(rows);
    scheduleLayoutUpdate();
}

void FilterExpansionWidget::setMaximumColumns(int columns)
{
    columns = std::max(0, columns);
    if(m_maximumColumns == columns) {
        return;
    }

    m_maximumColumns = columns;
    rememberScrollAnchor();
    m_tracks->setFlowMaximumColumns(columns);
    scheduleLayoutUpdate();
}

void FilterExpansionWidget::setStretchColumns(bool stretch)
{
    if(m_stretchColumns == stretch) {
        return;
    }

    rememberScrollAnchor();
    m_stretchColumns = stretch;
    m_tracks->setFlowStretchColumns(stretch);
    scheduleLayoutUpdate();
}

void FilterExpansionWidget::setMinimumColumnWidth(int percent)
{
    percent = std::clamp(percent, 25, 300);
    if(m_minimumColumnWidth == percent) {
        return;
    }

    rememberScrollAnchor();
    m_minimumColumnWidth = percent;
    updateLayout();
    scheduleLayoutUpdate();
}

void FilterExpansionWidget::setArtworkOnLeft(bool left)
{
    if(m_artworkOnLeft == left) {
        return;
    }

    rememberScrollAnchor();
    m_artworkOnLeft = left;
    updateLayout();
    scheduleLayoutUpdate();
}

void FilterExpansionWidget::setArtworkSize(int size)
{
    size = std::clamp(size, 50, 200);
    if(m_artworkSize == size) {
        return;
    }

    rememberScrollAnchor();
    m_artworkSize = size;
    updateLayout();
    scheduleLayoutUpdate();
}

void FilterExpansionWidget::setArtworkMargins(const QMargins& margins)
{
    const QMargins clamped{std::clamp(margins.left(), 0, 999), std::clamp(margins.top(), 0, 999),
                           std::clamp(margins.right(), 0, 999), std::clamp(margins.bottom(), 0, 999)};
    if(m_artworkMargins == clamped) {
        return;
    }

    rememberScrollAnchor();
    m_artworkMargins = clamped;
    updateLayout();
    scheduleLayoutUpdate();
}

void FilterExpansionWidget::setArtworkAutoMargins(bool enabled)
{
    if(m_artworkAutoMargins == enabled) {
        return;
    }

    rememberScrollAnchor();
    m_artworkAutoMargins = enabled;
    updateLayout();
    scheduleLayoutUpdate();
}

void FilterExpansionWidget::setArtworkReflection(bool enabled)
{
    if(std::exchange(m_artworkReflection, enabled) != enabled) {
        update();
    }
}

void FilterExpansionWidget::setCloseButtonVisible(bool visible)
{
    if(std::exchange(m_showCloseButton, visible) == visible) {
        return;
    }

    rememberScrollAnchor();
    m_close->setVisible(visible && !m_previewMode);
    updateLayout();
    scheduleLayoutUpdate();
}

void FilterExpansionWidget::setArtwork(const Track& track, Track::Cover type,
                                       std::optional<ArtworkSourcePreference> source, bool show, const QPixmap& preview)
{
    const bool changed = !m_coverTrack.sameIdentityAs(track) || m_coverType != type || m_coverSource != source;

    m_backgroundSource->setTrack(track);
    m_coverTrack  = track;
    m_coverType   = type;
    m_coverSource = source;
    m_showArtwork = show;
    m_coverProvider->setSourcePreference(source);

    if(changed || m_artwork.isNull()) {
        m_artwork = preview;
        updateArtwork();
    }
    updateLayout();
}

void FilterExpansionWidget::clear()
{
    cancel();
    ++m_coverRequest;
    m_coverTrack = {};
    m_backgroundSource->setTrack({});
    m_artwork = {};
    m_model->setContent({});
    m_footer->setContent({});
    updateSelection();
}

QColor FilterExpansionWidget::defaultBackgroundColour() const
{
    const auto group = !isEnabled() ? QPalette::Disabled : isActiveWindow() ? QPalette::Active : QPalette::Inactive;
    return expansionBackground(QApplication::palette(this), group);
}

QSize FilterExpansionWidget::sizeHint() const
{
    return {320, heightForWidth(width())};
}

int FilterExpansionWidget::heightForWidth(int width) const
{
    return layoutMetrics(width).height;
}

bool FilterExpansionWidget::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::KeyPress && !m_previewMode) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if(key->key() == Qt::Key_Escape) {
            Q_EMIT closeRequested();
            return true;
        }
        if(key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            activate(m_tracks->currentIndex().isValid() ? m_tracks->currentIndex() : m_model->index(0, 0, {}));
            return true;
        }
    }

    if(watched == m_tracks->viewport()) {
        if(event->type() == QEvent::Wheel) {
            const auto* wheel      = static_cast<QWheelEvent*>(event);
            const QPointF position = QPointF{m_tracks->viewport()->mapTo(parentWidget(), QPoint{})} + wheel->position();
            QWheelEvent forwarded{position,         wheel->globalPosition(), wheel->pixelDelta(), wheel->angleDelta(),
                                  wheel->buttons(), wheel->modifiers(),      wheel->phase(),      wheel->inverted(),
                                  wheel->source(),  wheel->pointingDevice()};
            forwarded.setTimestamp(wheel->timestamp());
            QApplication::sendEvent(parentWidget(), &forwarded);
            event->accept();
            return true;
        }
        if(event->type() == QEvent::ContextMenu && !m_previewMode) {
            const auto* context = static_cast<QContextMenuEvent*>(event);
            Q_EMIT requestContextMenu(context->globalPos(), m_context, m_tracks->selectionModel()->hasSelection());
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void FilterExpansionWidget::mousePressEvent(QMouseEvent* event)
{
    // Prevent closing on artwork click
    event->accept();
}

void FilterExpansionWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};

    const QColor customColour{m_backgroundColour};
    const QColor background = customColour.isValid() ? customColour : defaultBackgroundColour();
    const int anchor        = std::clamp(property("expansionAnchor").toInt(), PointerHeight,
                                         std::max(PointerHeight, width() - PointerHeight));

    QPainterPath shape;
    shape.addRect(rect().adjusted(0, PointerHeight, 0, 0));
    shape.addPolygon(QPolygonF{{static_cast<qreal>(anchor - PointerHeight), PointerHeight},
                               {static_cast<qreal>(anchor), 0},
                               {static_cast<qreal>(anchor + PointerHeight), PointerHeight}});
    painter.save();
    painter.setClipPath(shape);
    painter.fillRect(rect(), background);
    m_background->draw(painter);
    painter.restore();

    if(m_artworkRect.isValid() && !m_artwork.isNull()) {
        const auto scaled = Utils::scalePixmap(m_artwork, m_artworkRect.size(), devicePixelRatioF(), true);

        QRect target{QPoint{}, scaled.deviceIndependentSize().toSize()};
        target.moveCenter(m_artworkRect.center());

        painter.drawPixmap(target.topLeft(), scaled);

        if(m_artworkReflection) {
            const Gui::ArtworkReflectionOptions options{.enabled = true};
            const qreal dpr = scaled.devicePixelRatio();
            const qreal reflectionHeight
                = std::ceil(Gui::artworkReflectionHeight(target.height(), options) * dpr) / dpr;
            const qreal bottom = target.bottom() + 1 + options.gap + reflectionHeight;

            if(bottom <= height() - m_effectiveArtworkMargins.bottom()) {
                Gui::drawArtworkReflection(painter, m_artwork, scaled, target, 0, options);
            }
        }
    }
}

void FilterExpansionWidget::resizeEvent(QResizeEvent* event)
{
    if(event->size().width() != event->oldSize().width()) {
        rememberScrollAnchor();
    }

    QWidget::resizeEvent(event);

    updateLayout();
    if(event->size().width() != event->oldSize().width()) {
        scheduleLayoutUpdate();
    }
}

void FilterExpansionWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if(event->type() == QEvent::PaletteChange) {
        updatePalette();
    }
    if(event->type() == QEvent::FontChange || event->type() == QEvent::LayoutDirectionChange) {
        updateLayout();
        Q_EMIT contentChanged();
    }
}

bool FilterExpansionWidget::updateTracks(const ExpansionResult& content, bool rebuild, bool preserveSelection)
{
    if(!rebuild && !m_model->changesStructure(content)) {
        return m_model->updateContent(content);
    }

    std::set<QString> selectedKeys;
    std::vector<int> selectedTracks;
    QString currentKey;
    int currentTrack{-1};

    if(preserveSelection) {
        const auto selectedIndexes = m_tracks->selectionModel()->selectedIndexes();
        for(const auto& index : selectedIndexes) {
            const int track = m_model->trackIndex(index);
            if(track >= 0) {
                selectedTracks.push_back(track);
                if(rebuild) {
                    selectedKeys.emplace(m_model->tracks().at(track).identityKey());
                }
            }
        }

        currentTrack = m_model->firstTrack(m_tracks->currentIndex());
        if(rebuild && currentTrack >= 0) {
            currentKey = m_model->tracks().at(currentTrack).identityKey();
        }
    }

    const QSignalBlocker blocker{m_tracks->selectionModel()};
    if(rebuild) {
        m_model->setContent(content);
    }
    else {
        m_model->updateContent(content);
    }

    QItemSelection selection;
    if(rebuild && preserveSelection) {
        for(int row{0}; const Track& track : m_model->tracks()) {
            const auto index = m_model->indexForTrack(row++);
            const auto key   = track.identityKey();

            if(selectedKeys.contains(key)) {
                selection.select(index, index);
            }
            if(key == currentKey) {
                m_tracks->setCurrentIndex(index);
            }
        }
    }
    else if(!rebuild) {
        for(const int track : selectedTracks) {
            const auto index = m_model->indexForTrack(track);
            selection.select(index, index);
        }
        m_tracks->setCurrentIndex(m_model->indexForTrack(currentTrack));
    }

    m_tracks->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    updateSelection();
    return true;
}

void FilterExpansionWidget::updateSelection()
{
    if(!m_selection) {
        return;
    }

    const auto tracks = m_model->tracks();

    TrackSelection selection;
    const auto selectedTracks = m_model->selectedTracks(m_tracks->selectionModel()->selectedIndexes());
    for(const int track : selectedTracks) {
        selection.tracks.push_back(tracks.at(track));
    }

    if(!selectedTracks.empty()) {
        const int primaryTrack = *selectedTracks.begin();
        selection.playbackViewTracks.reserve(tracks.size());
        selection.playbackViewCurrentIndex = primaryTrack;

        for(const auto& track : tracks) {
            selection.playbackViewTracks.emplace_back(track);
        }

        const auto groupTracks = m_model->selectedTracks({m_model->indexForTrack(primaryTrack).parent()});
        selection.playbackGroupTracks.reserve(groupTracks.size());

        for(const int track : groupTracks) {
            if(track == primaryTrack) {
                selection.playbackGroupCurrentIndex = static_cast<int>(selection.playbackGroupTracks.size());
            }
            selection.playbackGroupTracks.push_back(selection.playbackViewTracks.at(track));
        }
    }

    m_selection->changeSelectedTracks(m_context, selection);
}

void FilterExpansionWidget::activate(const QModelIndex& index)
{
    if(m_model->firstTrack(index) >= 0 && !m_previewMode) {
        updateSelection();
        Q_EMIT doubleClicked(index);
    }
}

void FilterExpansionWidget::updateArtwork()
{
    const auto request = ++m_coverRequest;

    if(m_artwork.isNull()) {
        m_artwork = m_coverProvider->placeholderCover(m_coverType);
    }

    update();

    if(!m_coverTrack.isValid()) {
        return;
    }

    m_coverProvider->trackCoverFull(m_coverTrack, m_coverType).then(this, [this, request](const QPixmap& cover) {
        if(request == m_coverRequest) {
            m_artwork = cover.isNull() ? m_coverProvider->placeholderCover(m_coverType) : cover;
            update();
        }
    });
}

void FilterExpansionWidget::updateLayout()
{
    const double defaultColumnWidth
        = fontMetrics().horizontalAdvance(ColumnWidthReferenceGlyph) * DefaultColumnWidthInGlyphs;
    const double scaledColumnWidth = defaultColumnWidth * m_minimumColumnWidth / FullPercentage;
    m_tracks->setFlowMinimumColumnWidth(static_cast<int>(std::llround(scaledColumnWidth)));

    const auto metrics        = layoutMetrics(width());
    const int extent          = metrics.artworkExtent;
    const bool artwork        = extent > 0;
    m_effectiveArtworkMargins = metrics.artworkMargins;

    const int artworkX
        = m_artworkOnLeft ? m_effectiveArtworkMargins.left() : width() - extent - m_effectiveArtworkMargins.right();
    m_artworkRect
        = artwork ? QRect{artworkX, PointerHeight + m_effectiveArtworkMargins.top(), extent, extent} : QRect{};

    const int artworkMargin
        = artwork ? extent + m_effectiveArtworkMargins.left() + m_effectiveArtworkMargins.right() : ContentMargin;
    const int controlsMargin = metrics.controlsMargin;

    layout()->setContentsMargins(m_artworkOnLeft ? artworkMargin : controlsMargin, PointerHeight + ContentMargin,
                                 m_artworkOnLeft ? controlsMargin : artworkMargin, ContentMargin);

    const QSize closeSize = m_close->sizeHint();
    const int closeX      = m_artworkOnLeft ? width() - closeSize.width() - 2 : 2;
    m_close->setGeometry({closeX, PointerHeight + 2, closeSize.width(), closeSize.height()});

    update();
}

void FilterExpansionWidget::scheduleLayoutUpdate()
{
    if(std::exchange(m_layoutPending, true)) {
        return;
    }

    QMetaObject::invokeMethod(
        this,
        [this]() {
            updateGeometry();
            Q_EMIT contentChanged();

            const auto* outer
                = parentWidget() ? qobject_cast<QAbstractItemView*>(parentWidget()->parentWidget()) : nullptr;
            if(!m_scrollAnchorKey.isEmpty() && outer && outer->verticalScrollBar()->value() == m_scrollAnchorScroll) {
                for(int track{0}; std::cmp_less(track, m_model->tracks().size()); ++track) {
                    if(m_model->tracks()[track].identityKey() == m_scrollAnchorKey) {
                        const auto index = m_model->indexForTrack(track);
                        const QRect rect = m_tracks->visualRect(index);
                        if(rect.isValid()) {
                            const int y = m_tracks->viewport()->mapTo(parentWidget(), rect.topLeft()).y();
                            Q_EMIT scrollAdjustmentRequested(y - m_scrollAnchorY);
                        }
                        break;
                    }
                }
            }
            m_scrollAnchorKey.clear();
            m_layoutPending = false;
        },
        Qt::QueuedConnection);
}

void FilterExpansionWidget::rememberScrollAnchor()
{
    if(m_previewMode || !parentWidget() || !m_scrollAnchorKey.isEmpty()) {
        return;
    }

    const QRect visible{m_tracks->viewport()->mapFrom(parentWidget(), QPoint{}), parentWidget()->size()};
    auto anchor = m_tracks->currentIndex();
    if(m_model->trackIndex(anchor) < 0 || !m_tracks->visualRect(anchor).intersects(visible)) {
        anchor                    = {};
        const auto visibleIndexes = m_tracks->visibleIndexes();
        for(const auto& index : visibleIndexes) {
            if(m_model->trackIndex(index) >= 0 && m_tracks->visualRect(index).intersects(visible)) {
                anchor = index;
                break;
            }
        }
    }

    const int track = m_model->trackIndex(anchor);
    if(track >= 0) {
        if(const auto* outer = qobject_cast<QAbstractItemView*>(parentWidget()->parentWidget())) {
            m_scrollAnchorScroll = outer->verticalScrollBar()->value();
        }
        m_scrollAnchorKey = m_model->tracks()[track].identityKey();
        m_scrollAnchorY   = m_tracks->viewport()->mapTo(parentWidget(), m_tracks->visualRect(anchor).topLeft()).y();
    }
}

FilterExpansionWidget::LayoutMetrics FilterExpansionWidget::layoutMetrics(int width) const
{
    LayoutMetrics metrics;
    metrics.artworkMargins = m_artworkMargins;
    metrics.controlsMargin
        = (m_previewMode || !m_showCloseButton) ? ContentMargin : m_close->sizeHint().width() + ContentMargin;

    const auto contentHeight = [&](int artworkWidth) {
        const int contentWidth = std::max(1, width - metrics.controlsMargin - artworkWidth);
        int height = PointerHeight + (2 * ContentMargin) + m_tracks->contentSizeForWidth(contentWidth).height();
        if(!m_footer->isHidden()) {
            height += m_footer->sizeHint().height() + layout()->spacing();
        }
        return height;
    };

    const int fontHeight       = fontMetrics().height();
    const int preferredExtent  = static_cast<int>(std::llround(fontHeight * 14.0 * m_artworkSize / 100.0));
    const int minimumExtent    = fontHeight * 6;
    const int minimumTextWidth = fontMetrics().horizontalAdvance(u"M"_s) * 25;

    if(m_artworkAutoMargins) {
        if(m_artworkOnLeft) {
            metrics.artworkMargins.setRight(std::max(ContentMargin, metrics.artworkMargins.right()));
        }
        else {
            metrics.artworkMargins.setLeft(std::max(ContentMargin, metrics.artworkMargins.left()));
        }
    }

    // Reserve margins before measuring content so artwork size is independent of track height
    const int availableExtent = width - metrics.controlsMargin - minimumTextWidth - metrics.artworkMargins.left()
                              - metrics.artworkMargins.right();
    const int extent          = std::min(preferredExtent, availableExtent);
    if(m_showArtwork && extent >= minimumExtent) {
        metrics.artworkExtent = extent;
        if(m_artworkAutoMargins && contentHeight(extent + ContentMargin) <= PointerHeight + extent) {
            metrics.artworkMargins
                = m_artworkOnLeft ? QMargins{0, 0, ContentMargin, 0} : QMargins{ContentMargin, 0, 0, 0};
        }
    }

    const int artworkWidth = metrics.artworkExtent > 0
                               ? metrics.artworkExtent + metrics.artworkMargins.left() + metrics.artworkMargins.right()
                               : ContentMargin;
    metrics.height         = contentHeight(artworkWidth);
    if(metrics.artworkExtent > 0) {
        metrics.height = std::max(metrics.height, PointerHeight + metrics.artworkExtent + metrics.artworkMargins.top()
                                                      + metrics.artworkMargins.bottom());
    }

    return metrics;
}

void FilterExpansionWidget::updatePalette()
{
    QPalette colours{QApplication::palette(this)};
    for(const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        colours.setBrush(group, QPalette::Base, Qt::NoBrush);
        colours.setBrush(group, QPalette::AlternateBase, Qt::NoBrush);
        colours.setBrush(group, QPalette::Window, Qt::NoBrush);
    }
    m_tracks->setPalette(colours);
    m_footer->setPalette(colours);

    update();
}
} // namespace Fooyin::Filters

#include "moc_filterexpansionwidget.cpp"
