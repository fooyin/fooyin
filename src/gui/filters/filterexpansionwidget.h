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

#include "filterexpansionpopulator.h"

#include "artwork/artworkreflection.h"

#include <core/track.h>
#include <gui/coverartworktypes.h>
#include <gui/widgets/backgroundconfig.h>

#include <QColor>
#include <QMargins>
#include <QPixmap>
#include <QThread>
#include <QWidget>

#include <optional>

class QToolButton;

namespace Fooyin {
class CoverProvider;
class CoverRepository;
class BackgroundImageSource;
class WidgetBackground;
class ExpandedTreeView;
class TrackSelectionController;
class WidgetContext;

namespace Filters {
class ExpansionTrackModel;
class ExpansionFooterWidget;

class FilterExpansionWidget : public QWidget
{
    Q_OBJECT

public:
    FilterExpansionWidget(TrackSelectionController* selection, CoverRepository* coverRepo, QWidget* parent = nullptr);
    ~FilterExpansionWidget() override;

    void submit(ExpansionRequest request);
    void cancel();

    void setContent(const ExpansionResult& content, bool preserveSelection = true);
    void updateContent(const ExpansionResult& content);

    void setPreviewMode();
    void setPreviewArtwork(const QPixmap& artwork, bool show);
    void setBackground(const BackgroundConfig& config, const QString& colour);
    void setMinimumColumnRows(int rows);
    void setMaximumColumns(int columns);
    void setStretchColumns(bool stretch);
    void setMinimumColumnWidth(int percent);
    void setArtworkOnLeft(bool left);
    void setArtworkSize(int size);
    void setArtworkMargins(const QMargins& margins);
    void setArtworkAutoMargins(bool enabled);
    void setArtworkReflection(const Gui::ArtworkReflectionOptions& options);
    void setCloseButtonVisible(bool visible);
    void setArtwork(const Track& track, Track::Cover type, std::optional<ArtworkSourcePreference> source, bool show,
                    const QPixmap& preview);

    void clear();

    [[nodiscard]] QColor defaultBackgroundColour() const;
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] int heightForWidth(int width) const override;
    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    void populated(const Fooyin::Filters::ExpansionResult& result);
    void closeRequested();
    void contentChanged();
    void trackFocused(const QRect& rect);
    void scrollAdjustmentRequested(int delta);
    void requestContextMenu(const QPoint& pos, Fooyin::WidgetContext* context, bool hasSelection);
    void doubleClicked(const QModelIndex& index);
    void middleClicked(const QModelIndex& index);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    struct LayoutMetrics
    {
        int artworkExtent{0};
        int controlsMargin{0};
        int height{0};
        QMargins artworkMargins;
    };

    bool updateTracks(const ExpansionResult& content, bool rebuild, bool preserveSelection = true);
    void updateSelection();
    void activate(const QModelIndex& index);
    void updateArtwork();
    void updateLayout();
    void scheduleLayoutUpdate();
    void rememberScrollAnchor();
    [[nodiscard]] LayoutMetrics layoutMetrics(int width) const;
    void updatePalette();

    TrackSelectionController* m_selection;
    CoverProvider* m_coverProvider;

    BackgroundImageSource* m_backgroundSource;
    WidgetBackground* m_background;
    QString m_backgroundColour;

    WidgetContext* m_context;
    QThread m_populatorThread;
    FilterExpansionPopulator m_populator;
    std::optional<ExpansionRequest> m_pending;
    uint64_t m_runningGeneration;
    bool m_workerBusy;
    ExpansionTrackModel* m_model;
    ExpandedTreeView* m_tracks;
    ExpansionFooterWidget* m_footer;
    QToolButton* m_close;
    Track m_coverTrack;
    Track::Cover m_coverType;
    std::optional<ArtworkSourcePreference> m_coverSource;
    bool m_showArtwork;
    bool m_artworkOnLeft;
    Gui::ArtworkReflectionOptions m_artworkReflection;
    bool m_artworkAutoMargins;
    bool m_showCloseButton;
    bool m_previewMode;
    bool m_layoutPending;
    QString m_scrollAnchorKey;
    int m_scrollAnchorY;
    int m_scrollAnchorScroll;
    int m_minimumColumnRows;
    int m_maximumColumns;
    bool m_stretchColumns;
    int m_minimumColumnWidth;
    int m_artworkSize;
    QMargins m_artworkMargins;
    QMargins m_effectiveArtworkMargins;
    QPixmap m_artwork;
    QRect m_artworkRect;
    uint64_t m_coverRequest;
};
} // namespace Filters
} // namespace Fooyin
