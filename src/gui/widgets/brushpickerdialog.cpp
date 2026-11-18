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

#include "brushpickerdialog.h"

#include "swatchbutton.h"

#include <gui/widgets/colourpicker.h>
#include <gui/widgets/gradienteditor.h>
#include <gui/widgets/hintlabel.h>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMetaEnum>
#include <QPainter>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

constexpr auto PreviewHeight = 60;

namespace Fooyin {
namespace {
BrushSource colourBrush(const ColourSource& colour)
{
    return std::visit([](const auto& source) -> BrushSource { return source; }, colour);
}

ColourSource stopColour(const GradientEditor::ColourStop& stop)
{
    const auto parsed = readColourSource(QJsonValue::fromVariant(stop.data), {});
    return parsed ? *parsed : ColourSource{stop.colour};
}
} // namespace

class BrushPreview : public QWidget
{
public:
    explicit BrushPreview(QWidget* parent)
        : QWidget{parent}
    {
        setFixedHeight(PreviewHeight);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setBrush(const BrushSource& brush, const QPalette& palette, QPalette::ColorGroup group)
    {
        m_brush   = brush;
        m_palette = palette;
        m_group   = group;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter{this};

        auto brush{m_brush};
        if(auto* gradient = std::get_if<LinearGradientSource>(&brush)) {
            // The stop bar always runs from 0% to 100%
            // Direction applies to the themed control
            gradient->direction = LinearGradientSource::Direction::Horizontal;
        }
        paintBrushSwatch(painter, brush, m_palette, m_group, rect());
    }

private:
    BrushSource m_brush{QPalette::Window};
    QPalette m_palette;
    QPalette::ColorGroup m_group{QPalette::Active};
};

class ColourSourcePanel : public QTabWidget
{
    Q_OBJECT

public:
    ColourSourcePanel(const QPalette& palette, QPalette::ColorGroup group, QWidget* parent);

    [[nodiscard]] const ColourSource& value() const;

    void setValue(const ColourSource& colour);
    void setColourPalette(const QPalette& palette, QPalette::ColorGroup group);
    void setModesVisible(bool visible);

Q_SIGNALS:
    void valueChanged(const Fooyin::ColourSource& colour);

private:
    ColourSource m_value;
    QPalette m_palette;
    QPalette::ColorGroup m_group;
    QTreeWidget* m_roles;
    ColourPicker* m_custom;
    bool m_updating{false};
};

ColourSourcePanel::ColourSourcePanel(const QPalette& palette, QPalette::ColorGroup group, QWidget* parent)
    : QTabWidget{parent}
    , m_value{QPalette::Window}
    , m_palette{palette}
    , m_group{group}
    , m_roles{new QTreeWidget(this)}
    , m_custom{new ColourPicker(this)}
{
    auto* palettePage = new QWidget(this);
    m_roles->setHeaderLabels({tr("Palette role"), tr("Colour")});
    m_roles->setRootIsDecorated(false);
    m_roles->setItemsExpandable(false);
    m_roles->setUniformRowHeights(true);
    m_roles->setIconSize(QSize{48, 16});
    m_roles->setSelectionMode(QAbstractItemView::SingleSelection);
    m_roles->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_roles->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_roles->header()->setSectionResizeMode(1, QHeaderView::Stretch);

    const auto meta = QMetaEnum::fromType<QPalette::ColorRole>();
    for(int role{0}; role < QPalette::NColorRoles; ++role) {
        if(role != QPalette::NoRole) {
            if(const auto* name = meta.valueToKey(role)) {
                auto* item = new QTreeWidgetItem(m_roles);
                item->setText(0, QString::fromLatin1(name));
                item->setData(0, Qt::UserRole, role);
            }
        }
    }

    auto* paletteLayout = new QVBoxLayout(palettePage);
    paletteLayout->addWidget(m_roles, 1);
    paletteLayout->addWidget(new HintLabel(tr("Palette colours follow changes to the theme palette."), palettePage));

    auto* customPage   = new QWidget(this);
    auto* customLayout = new QVBoxLayout(customPage);
    customLayout->addWidget(m_custom);

    addTab(palettePage, tr("Palette"));
    addTab(customPage, tr("Custom colour"));

    setColourPalette(palette, group);
    setValue(m_value);

    QObject::connect(m_roles, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        if(!m_updating && currentIndex() == 0 && item) {
            m_value = static_cast<QPalette::ColorRole>(item->data(0, Qt::UserRole).toInt());
            Q_EMIT valueChanged(m_value);
        }
    });
    QObject::connect(m_custom, &ColourPicker::colourChanged, this, [this](const QColor& colour) {
        if(!m_updating && currentIndex() == 1 && colour.isValid()) {
            m_value = colour;
            Q_EMIT valueChanged(m_value);
        }
    });
    QObject::connect(this, &QTabWidget::currentChanged, this, [this](int mode) {
        if(m_updating) {
            return;
        }

        const QScopedValueRollback guard{m_updating, true};

        if(mode == 1) {
            const auto colour = resolveBrush(m_value, m_palette, m_group).color();
            m_custom->setOriginalColour(colour);
            m_custom->setColour(colour);
            m_value = colour;
        }
        else {
            auto* item = m_roles->currentItem();
            m_value = item ? static_cast<QPalette::ColorRole>(item->data(0, Qt::UserRole).toInt()) : QPalette::Window;
        }

        Q_EMIT valueChanged(m_value);
    });
}

const ColourSource& ColourSourcePanel::value() const
{
    return m_value;
}

void ColourSourcePanel::setValue(const ColourSource& colour)
{
    const QScopedValueRollback guard{m_updating, true};

    m_value = colour;

    if(const auto* role = std::get_if<QPalette::ColorRole>(&colour)) {
        for(int row{0}; row < m_roles->topLevelItemCount(); ++row) {
            auto* item = m_roles->topLevelItem(row);
            if(item->data(0, Qt::UserRole).toInt() == *role) {
                m_roles->setCurrentItem(item);
                m_roles->scrollToItem(item);
                break;
            }
        }
        setCurrentIndex(0);
    }
    else {
        setCurrentIndex(1);
    }

    const auto resolved = resolveBrush(colour, m_palette, m_group).color();
    m_custom->setOriginalColour(resolved);
    if(m_custom->colour() != resolved) {
        m_custom->setColour(resolved);
    }
}

void ColourSourcePanel::setColourPalette(const QPalette& palette, QPalette::ColorGroup group)
{
    m_palette = palette;
    m_group   = group;

    for(int row{0}; row < m_roles->topLevelItemCount(); ++row) {
        auto* item      = m_roles->topLevelItem(row);
        const auto role = static_cast<QPalette::ColorRole>(item->data(0, Qt::UserRole).toInt());
        item->setText(1, palette.color(group, role).name(QColor::HexArgb));
        item->setIcon(1, brushSourceIcon(role, palette, group, devicePixelRatioF(), m_roles->iconSize()));
    }

    setValue(m_value);
}

void ColourSourcePanel::setModesVisible(bool visible)
{
    tabBar()->setVisible(visible);
}

BrushPickerDialog::BrushPickerDialog(const BrushSource& brush, const QPalette& palette, QPalette::ColorGroup group,
                                     bool gradients, QWidget* parent)
    : QDialog{parent}
    , m_layout{new QGridLayout(this)}
    , m_value{brush}
    , m_palette{palette}
    , m_group{group}
    , m_mode{new QTabBar(this)}
    , m_preview{new BrushPreview(this)}
    , m_colourPanel{new ColourSourcePanel(palette, group, this)}
    , m_gradientPanel{new QWidget(this)}
    , m_gradientEditor{new GradientEditor(m_gradientPanel)}
    , m_direction{new QComboBox(m_gradientPanel)}
    , m_directionLayout{new QFormLayout()}
    , m_buttons{new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this)}
    , m_selectedStop{0}
    , m_hasGradient{std::holds_alternative<LinearGradientSource>(brush)}
    , m_updating{false}
{
    setWindowTitle(gradients ? tr("Choose Brush") : tr("Choose Colour"));
    setModal(true);
    setAttribute(Qt::WA_DeleteOnClose);

    m_mode->addTab(tr("Palette"));
    m_mode->addTab(tr("Custom colour"));
    if(gradients) {
        m_mode->addTab(tr("Linear gradient"));
    }
    m_mode->setExpanding(false);

    m_buttons->button(QDialogButtonBox::Ok)->setDefault(true);

    m_gradientPanel->setMinimumWidth(320);

    using Direction = LinearGradientSource::Direction;
    m_direction->addItem(tr("Along control"), static_cast<int>(Direction::AlongControl));
    m_direction->setItemData(m_direction->count() - 1, tr("Follow the control’s orientation from minimum to maximum."),
                             Qt::ToolTipRole);
    m_direction->addItem(tr("Across control"), static_cast<int>(Direction::AcrossControl));
    m_direction->setItemData(m_direction->count() - 1, tr("Run perpendicular to the control’s orientation."),
                             Qt::ToolTipRole);
    m_direction->addItem(tr("Horizontal"), static_cast<int>(Direction::Horizontal));
    m_direction->setItemData(m_direction->count() - 1,
                             tr("Run from left to right, regardless of the control’s orientation."), Qt::ToolTipRole);
    m_direction->addItem(tr("Vertical"), static_cast<int>(Direction::Vertical));
    m_direction->setItemData(m_direction->count() - 1,
                             tr("Run from top to bottom, regardless of the control’s orientation."), Qt::ToolTipRole);

    m_gradientEditor->setColourEditorFactory([this](int index, const GradientEditor::ColourStop&, QWidget* stopParent) {
        if(index == 0) {
            m_stopButtons.clear();
        }

        auto* editor = new QPushButton(stopParent);
        editor->setCheckable(true);
        editor->setAutoDefault(false);
        editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        QObject::connect(editor, &QPushButton::clicked, editor, [this, editor] {
            const int stopIndex = m_gradientEditor->colourEditorIndex(editor);
            m_gradientEditor->setSelectedStop(stopIndex);
            selectStop(stopIndex);
        });

        m_stopButtons.emplace_back(editor);
        return editor;
    });

    m_directionLayout->addRow(tr("Direction:"), m_direction);

    auto* gradientLayout = new QVBoxLayout(m_gradientPanel);
    gradientLayout->setContentsMargins({});
    gradientLayout->addLayout(m_directionLayout);
    gradientLayout->addWidget(m_gradientEditor, 1);

    m_layout->addWidget(m_mode, 0, 0, 1, 2);
    m_layout->addWidget(new QLabel(tr("Preview"), this), 1, 0, 1, 2);
    m_layout->addWidget(m_preview, 2, 0, 1, 2);
    m_layout->addWidget(m_gradientPanel, 3, 0);
    m_layout->addWidget(m_colourPanel, 3, 1);
    m_layout->addWidget(m_buttons, 4, 0, 1, 2);
    m_layout->setColumnStretch(0, 1);
    m_layout->setColumnStretch(1, 2);
    m_layout->setRowStretch(3, 1);

    QObject::connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QObject::connect(m_mode, &QTabBar::currentChanged, this, &BrushPickerDialog::changeMode);

    QObject::connect(m_colourPanel, &ColourSourcePanel::valueChanged, this, [this](const ColourSource& colour) {
        if(m_updating) {
            return;
        }

        if(m_mode->currentIndex() == 2) {
            auto stop   = m_gradientEditor->colourStops().at(m_selectedStop);
            stop.colour = resolveBrush(colour, m_palette, m_group).color();
            stop.data   = colourSourceToJson(colour).toVariantMap();
            m_gradientEditor->updateColourStop(m_selectedStop, stop);
        }
        else {
            updateValue();
        }
    });

    QObject::connect(m_gradientEditor, &GradientEditor::stopsChanged, this, &BrushPickerDialog::updateGradient);
    QObject::connect(m_gradientEditor, &GradientEditor::selectedStopChanged, this, &BrushPickerDialog::selectStop);

    QObject::connect(m_direction, &QComboBox::currentIndexChanged, this, [this] {
        if(!m_updating) {
            m_gradient.direction = static_cast<Direction>(m_direction->currentData().toInt());
            updateValue();
        }
    });

    {
        const QScopedValueRollback guard{m_updating, true};

        if(const auto* gradient = std::get_if<LinearGradientSource>(&brush)) {
            m_gradient = *gradient;
            m_mode->setCurrentIndex(2);
        }
        else if(const auto* role = std::get_if<QPalette::ColorRole>(&brush)) {
            m_colourPanel->setValue(*role);
            m_mode->setCurrentIndex(0);
        }
        else {
            m_colourPanel->setValue(std::get<QColor>(brush));
            m_mode->setCurrentIndex(1);
        }
    }

    changeMode(m_mode->currentIndex());
    m_preview->setBrush(m_value, m_palette, m_group);

    resize(gradients ? 1000 : 660, 680);
}

const BrushSource& BrushPickerDialog::value() const
{
    return m_value;
}

void BrushPickerDialog::setColourPalette(const QPalette& palette, QPalette::ColorGroup group)
{
    const QScopedValueRollback guard{m_updating, true};
    m_palette = palette;
    m_group   = group;
    m_colourPanel->setColourPalette(palette, group);
    if(m_mode->currentIndex() == 2) {
        updateStops();
    }
    m_preview->setBrush(m_value, palette, group);
}

void BrushPickerDialog::setDirectionControlVisible(bool visible)
{
    m_directionLayout->setRowVisible(m_direction, visible);
}

void BrushPickerDialog::changeMode(int mode)
{
    if(m_updating) {
        return;
    }

    {
        const QScopedValueRollback guard{m_updating, true};

        m_gradientPanel->setVisible(mode == 2);
        m_layout->removeWidget(m_colourPanel);
        m_layout->addWidget(m_colourPanel, 3, mode == 2 ? 1 : 0, 1, mode == 2 ? 1 : 2);
        m_colourPanel->setModesVisible(mode == 2);

        if(mode == 2) {
            if(!m_hasGradient) {
                const auto first = m_colourPanel->value();
                m_gradient.stops
                    = {{.position = 0.0, .colour = first},
                       {.position = 1.0,
                        .colour = first == ColourSource{QPalette::Highlight} ? QPalette::Button : QPalette::Highlight}};
                m_hasGradient = true;
            }
            updateStops();
        }
        else {
            m_colourPanel->setCurrentIndex(mode);
        }
    }

    updateValue();
}

void BrushPickerDialog::selectStop(int index)
{
    if(index < 0 || !std::cmp_less(index, m_gradient.stops.size())) {
        return;
    }
    // Stored stops use 8-bit channels; rounding alone must not reset the active editor.
    if(m_selectedStop != index
       || colourSourceName(m_colourPanel->value()) != colourSourceName(m_gradient.stops[index].colour)) {
        m_colourPanel->setValue(m_gradient.stops[index].colour);
    }
    m_selectedStop = index;
    for(const auto& button : m_stopButtons) {
        if(button) {
            const int stop     = m_gradientEditor->colourEditorIndex(button);
            const auto& colour = m_gradient.stops[stop].colour;
            button->setText(tr("%1. %2").arg(stop + 1).arg(colourSourceName(colour)));
            button->setIcon(brushSourceIcon(colourBrush(colour), m_palette, m_group, devicePixelRatioF()));
            button->setChecked(stop == index);
        }
    }
}

void BrushPickerDialog::updateGradient()
{
    if(m_updating) {
        return;
    }

    m_gradient.stops.clear();

    for(const auto& stop : m_gradientEditor->colourStops()) {
        m_gradient.stops.emplace_back(stop.position, stopColour(stop));
    }
    selectStop(m_gradientEditor->selectedStop());
    updateValue();
}

void BrushPickerDialog::updateStops()
{
    std::vector<GradientEditor::ColourStop> stops;
    stops.reserve(m_gradient.stops.size());

    for(const auto& stop : m_gradient.stops) {
        stops.push_back({.position = stop.position,
                         .colour   = resolveBrush(stop.colour, m_palette, m_group).color(),
                         .data     = colourSourceToJson(stop.colour).toVariantMap()});
    }
    m_gradientEditor->setColourStops(stops);
    m_gradientEditor->setSelectedStop(m_selectedStop);

    m_direction->setCurrentIndex(m_direction->findData(static_cast<int>(m_gradient.direction)));
    selectStop(m_selectedStop);
}

void BrushPickerDialog::updateValue()
{
    const BrushSource updated
        = m_mode->currentIndex() == 2 ? BrushSource{m_gradient} : colourBrush(m_colourPanel->value());
    const bool valid = m_mode->currentIndex() != 2 || (m_gradient.stops.size() >= 2 && m_gradient.stops.size() <= 64);

    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(valid);
    m_preview->setBrush(updated, m_palette, m_group);

    if(std::exchange(m_value, updated) != updated && valid) {
        Q_EMIT valueChanged(m_value);
    }
}
} // namespace Fooyin

#include "brushpickerdialog.moc"
#include "moc_brushpickerdialog.cpp"
