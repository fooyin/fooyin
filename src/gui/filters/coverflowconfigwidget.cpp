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

#include "coverflowconfigwidget.h"

#include "filterdelegate.h"
#include "trackgenerator.h"

#include <gui/guiutils.h>
#include <gui/widgets/expandedtreeview.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScopedValueRollback>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
CoverFlowConfigWidget::CoverFlowConfigWidget(QWidget* parent)
    : QWidget{parent}
    , m_preview{new ExpandedTreeView(this)}
    , m_labelPosition{new QComboBox(this)}
    , m_updating{false}
{
    auto* tabs = new QTabWidget(this);

    const auto page = [tabs](const QString& title, int columnCount) {
        auto* controls = new QWidget(tabs);
        auto* columns  = new QHBoxLayout(controls);

        std::vector<QGridLayout*> grids;
        for(int i{0}; i < columnCount; ++i) {
            auto* column = new QGridLayout();
            column->setColumnStretch(2, 1);
            columns->addLayout(column, 1);
            grids.push_back(column);
        }

        tabs->addTab(controls, title);
        return grids;
    };

    const auto layout     = page(tr("Layout"), 3);
    const auto appearance = page(tr("Appearance"), 2);

    auto* grid = layout[0];
    int row{0};

    const auto section = [this, &grid, &row](const QString& title) {
        grid->addWidget(Gui::createSectionHeader(title, this), row++, 0, 1, 3);
    };

    const auto integer = [this, &grid, &row](const QString& label, int CoverFlowSettings::* member, int minimum,
                                             int maximum, const QString& suffix, const QString& tooltip) {
        auto* editor = new QSpinBox(this);
        editor->setRange(minimum, maximum);
        editor->setSuffix(suffix);
        editor->setToolTip(tooltip);

        if(member == &CoverFlowSettings::itemsPerSide) {
            editor->setSpecialValueText(tr("Auto"));
        }

        grid->addWidget(new QLabel(label + u":"_s, this), row, 0);
        grid->addWidget(editor, row++, 1);

        m_integerControls.emplace_back(member, editor);
        QObject::connect(editor, &QSpinBox::valueChanged, this, &CoverFlowConfigWidget::updateControls);
    };

    const auto boolean
        = [this, &grid, &row](const QString& label, bool CoverFlowSettings::* member, const QString& tooltip) {
              auto* editor = new QCheckBox(label, this);
              editor->setToolTip(tooltip);

              grid->addWidget(editor, row++, 0, 1, 3);

              m_booleanControls.emplace_back(member, editor);
              QObject::connect(editor, &QCheckBox::toggled, this, &CoverFlowConfigWidget::updateControls);
          };

    section(tr("Artwork arrangement"));
    boolean(tr("Fit artwork to available space"), &CoverFlowSettings::autoFitArtwork,
            tr("Automatically size artwork to fit the widget."));
    integer(tr("Side artwork tilt"), &CoverFlowSettings::sideArtworkTilt, -85, 85, u"°"_s,
            tr("Tilt the artwork on either side of the current item.\n"
               "Negative values reverse the tilt; 0° keeps artwork flat."));
    integer(tr("Centre spacing"), &CoverFlowSettings::centreSpacing, 20, 200, u" %"_s,
            tr("Distance between the centres of the current artwork and its nearest neighbour, "
               "as a percentage of the current artwork’s width"));
    integer(tr("Side spacing"), &CoverFlowSettings::sideSpacing, 0, 100, u" %"_s,
            tr("Distance between successive images on each side, as a percentage of the current artwork’s width"));
    integer(tr("Items on each side"), &CoverFlowSettings::itemsPerSide, 0, 20, {},
            tr("Set the number of items on each side. ‘Auto’ fills the available width."));
    grid->setRowStretch(row, 1);

    grid = layout[1];
    row  = 0;
    section(tr("Depth effects"));
    integer(tr("Size progression"), &CoverFlowSettings::sizeProgression, 20, 100, u" %"_s,
            tr("Set each image’s size relative to the previous image, moving away from the centre.\n"
               "At 100%, all artwork stays the same size."));
    integer(tr("Minimum artwork size"), &CoverFlowSettings::minArtworkSize, 0, 100, u" %"_s,
            tr("Smallest side artwork size as a percentage of the current artwork size"));
    integer(tr("Perspective"), &CoverFlowSettings::perspective, 0, 200, u" %"_s,
            tr("Adjust the depth effect of tilted artwork. At 0%, tilted artwork has no perspective distortion."));
    integer(tr("Side artwork fading"), &CoverFlowSettings::backgroundBlending, 0, 100, u" %"_s,
            tr("Artwork fades progressively with distance from the centre.\n"
               "Higher values make side artwork fainter."));
    grid->setRowStretch(row, 1);

    grid = layout[2];
    row  = 0;
    section(tr("Scrolling"));
    boolean(tr("Animate scrolling"), &CoverFlowSettings::animateScrolling,
            tr("Animate scrolling when selecting artwork or using the mouse wheel, touchpad or keyboard."));
    integer(tr("Animation duration"), &CoverFlowSettings::animationDuration, 0, 500, u" ms"_s,
            tr("Duration of the scrolling animation"));
    integer(tr("Scroll sensitivity"), &CoverFlowSettings::scrollSensitivity, 10, 500, u" %"_s,
            tr("Distance moved by the mouse wheel or touchpad, relative to the default scrolling distance"));
    grid->setRowStretch(row, 1);

    m_labelPosition->addItems({tr("Below artwork"), tr("Above artwork"), tr("On artwork")});
    m_labelPosition->setToolTip(tr("Position of the current item’s labels relative to its artwork"));

    grid = appearance[0];
    row  = 0;
    section(tr("Labels"));
    boolean(tr("Show labels"), &CoverFlowSettings::showLabels,
            tr("Show text from the displayed filter columns for the current item"));
    grid->addWidget(new QLabel(tr("Position") + u":"_s, this), row, 0);
    grid->addWidget(m_labelPosition, row++, 1);
    QObject::connect(m_labelPosition, &QComboBox::currentIndexChanged, this, &CoverFlowConfigWidget::updateControls);
    boolean(tr("Overlay labels on reflections"), &CoverFlowSettings::overlayLabelsOnReflections,
            tr("Draw labels in the reflection area when positioned below the artwork."));
    integer(tr("Spacing"), &CoverFlowSettings::labelSpacing, 0, 100, u" px"_s,
            tr("Space between the artwork area and its labels"));
    grid->setRowStretch(row, 1);

    grid = appearance[1];
    row  = 0;
    section(tr("Artwork reflection"));
    boolean(tr("Show artwork reflection"), &CoverFlowSettings::showReflection,
            tr("Draw a fading reflection below the artwork"));
    integer(tr("Height"), &CoverFlowSettings::reflectionHeight, 0, 100, u" %"_s,
            tr("Reflection height as a percentage of the artwork height"));
    integer(tr("Opacity"), &CoverFlowSettings::reflectionOpacity, 0, 100, u" %"_s,
            tr("Opacity at the top of the reflection, fading to transparent"));
    integer(tr("Gap below artwork"), &CoverFlowSettings::reflectionGap, 0, 100, u" px"_s,
            tr("Space between the artwork and its reflection"));
    grid->setRowStretch(row, 1);

    auto* previewGroup  = new QGroupBox(tr("Preview"), this);
    auto* previewLayout = new QVBoxLayout(previewGroup);
    previewLayout->addWidget(m_preview);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tabs, 1);
    mainLayout->addWidget(previewGroup, 1);

    setupPreview();
    setSettings({});
}

CoverFlowSettings CoverFlowConfigWidget::settings() const
{
    auto result = m_preview->coverFlowSettings();

    for(const auto& control : m_integerControls) {
        result.*control.member = control.editor->value();
    }
    for(const auto& control : m_booleanControls) {
        result.*control.member = control.editor->isChecked();
    }
    result.labelsAboveArtwork = m_labelPosition->currentIndex() == 1;
    result.labelsOnArtwork    = m_labelPosition->currentIndex() == 2;

    return result;
}

void CoverFlowConfigWidget::setSettings(const CoverFlowSettings& settings)
{
    auto validated = settings;
    validated.normalise();

    {
        const QScopedValueRollback guard{m_updating, true};

        for(const auto& control : m_integerControls) {
            control.editor->setValue(validated.*control.member);
        }
        for(const auto& control : m_booleanControls) {
            control.editor->setChecked(validated.*control.member);
        }
        m_labelPosition->setCurrentIndex(validated.labelsOnArtwork ? 2 : validated.labelsAboveArtwork ? 1 : 0);
    }

    m_preview->setCoverFlowSettings(validated);
    updateControls();
}

void CoverFlowConfigWidget::setArtworkSettings(const QSize& size, int cornerRadius)
{
    m_preview->changeIconSize(size);
    m_preview->setIconItemCornerRadius(cornerRadius);
}

void CoverFlowConfigWidget::setArtworkFrames(const ArtworkFrame& hover, const ArtworkFrame& selection)
{
    auto* delegate = qobject_cast<FilterDelegate*>(m_preview->itemDelegate());
    delegate->setArtworkFrames(hover, selection);
    auto settings       = m_preview->coverFlowSettings();
    settings.frameWidth = std::max(hover.enabled ? hover.width : 0, selection.enabled ? selection.width : 0);
    m_preview->setCoverFlowSettings(settings);
    m_preview->viewport()->update();
}

QSize CoverFlowConfigWidget::sizeHint() const
{
    return QWidget::sizeHint().expandedTo({640, 480});
}

QSize CoverFlowConfigWidget::minimumSizeHint() const
{
    return QWidget::minimumSizeHint().expandedTo({520, 400});
}

void CoverFlowConfigWidget::setupPreview()
{
    m_preview->setMinimumHeight(160);
    m_preview->setHeaderHidden(true);
    m_preview->setViewMode(ExpandedTreeView::ViewMode::CoverFlow);
    m_preview->setCaptionDisplay(ExpandedTreeView::CaptionDisplay::Bottom);
    m_preview->setItemDelegate(new FilterDelegate(m_preview));
    m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_preview->setSelectionMode(QAbstractItemView::SingleSelection);
    m_preview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_preview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* model = new QStandardItemModel(m_preview);

    const auto tracks = TrackGenerator::generateTracks({.albumCount = 41, .discsPerAlbum = 1, .tracksPerDisc = 1});
    for(size_t i{0}; const auto& track : tracks) {
        auto* item = new QStandardItem(track.albumArtists().join(u", "_s) + u"\n"_s + track.album());
        item->setData(TrackGenerator::generateArtwork(static_cast<uint32_t>(i++)), Qt::DecorationRole);
        model->appendRow(item);
    }

    m_preview->setModel(model);

    const auto centre = model->index(model->rowCount() / 2, 0);
    m_preview->setCurrentIndex(centre);
    m_preview->scrollTo(centre, QAbstractItemView::PositionAtCenter);
}
void CoverFlowConfigWidget::updateControls()
{
    if(m_updating) {
        return;
    }

    const auto settings = this->settings();

    m_labelPosition->setEnabled(settings.showLabels);

    for(const auto& [member, editor] : m_integerControls) {
        const bool reflection = member == &CoverFlowSettings::reflectionHeight
                             || member == &CoverFlowSettings::reflectionOpacity
                             || member == &CoverFlowSettings::reflectionGap;
        editor->setEnabled(
            (!reflection || settings.showReflection)
            && (member != &CoverFlowSettings::labelSpacing || (settings.showLabels && !settings.labelsOnArtwork))
            && (member != &CoverFlowSettings::animationDuration || settings.animateScrolling));
    }

    for(const auto& [member, editor] : m_booleanControls) {
        if(member == &CoverFlowSettings::overlayLabelsOnReflections) {
            editor->setEnabled(settings.showLabels && !settings.labelsAboveArtwork && !settings.labelsOnArtwork
                               && settings.showReflection && settings.reflectionHeight > 0);
        }
    }

    m_preview->setCoverFlowSettings(settings);
    Q_EMIT settingsChanged();
}
} // namespace Fooyin::Filters
