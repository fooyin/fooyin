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

#include <gui/widgets/gradienteditor.h>

#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <gui/widgets/colourbutton.h>

#include <QDoubleSpinBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

constexpr auto MinListHeight = 120;
constexpr auto MinStopHeight = 20;

using namespace Qt::StringLiterals;

namespace Fooyin {
class GradientEditorPrivate : public QObject
{
public:
    struct StopRow
    {
        QWidget* editor;
        QDoubleSpinBox* position;
        QToolButton* remove;
    };

    explicit GradientEditorPrivate(GradientEditor* self)
        : m_self{self}
        , m_coloursWidget{new QWidget(m_self)}
        , m_coloursLayout{new QGridLayout()}
        , m_coloursScroll{new QScrollArea(m_self)}
    {
        m_coloursWidget->setLayout(m_coloursLayout);
        m_coloursLayout->setContentsMargins({});
        m_coloursLayout->setSizeConstraint(QLayout::SetMinimumSize);
        m_coloursLayout->setColumnStretch(0, 1);
        m_coloursLayout->addWidget(new QLabel(GradientEditor::tr("Colour"), m_coloursWidget), 0, 0);
        m_coloursLayout->addWidget(new QLabel(GradientEditor::tr("Position"), m_coloursWidget), 0, 1);

        m_coloursScroll->setWidget(m_coloursWidget);
        m_coloursScroll->setWidgetResizable(true);
        m_coloursScroll->setFrameShape(QFrame::NoFrame);
        m_coloursScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_coloursScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_coloursScroll->setMinimumHeight(MinListHeight);
        m_coloursScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void addStop()
    {
        size_t index{m_stops.size()};
        double position{1.0};
        if(!m_stops.empty()) {
            double start{0.0};
            double widest{-1.0};
            for(size_t gap{0}; gap <= m_stops.size(); ++gap) {
                const double end = gap < m_stops.size() ? m_stops[gap].position : 1.0;
                if(end - start > widest) {
                    widest   = end - start;
                    position = (start + end) / 2.0;
                    index    = gap;
                }
                start = end;
            }
        }

        auto stop     = m_stops.empty() ? GradientEditor::ColourStop{.position = 0.0,
                                                                     .colour   = m_self->palette().highlight().color(),
                                                                     .data     = {}}
                                        : m_stops[index > 0 ? index - 1 : 0];
        stop.position = position;
        m_stops.insert(m_stops.begin() + static_cast<ptrdiff_t>(index), stop);
        m_selectedStop = static_cast<int>(index);

        rebuildColourButtons();
        Q_EMIT m_self->stopsChanged();
        Q_EMIT m_self->selectedStopChanged(m_selectedStop);
    }

    void removeStop(int index)
    {
        if(m_stops.size() <= 2 || index < 0 || std::cmp_greater_equal(index, m_stops.size())) {
            return;
        }

        m_stops.erase(m_stops.begin() + index);
        m_selectedStop -= m_selectedStop > index;
        m_selectedStop = std::min(m_selectedStop, static_cast<int>(m_stops.size()) - 1);

        rebuildColourButtons();

        auto* selectedEditor = m_rows[m_selectedStop].editor;
        selectedEditor->setFocus();
        m_coloursScroll->ensureWidgetVisible(selectedEditor);

        Q_EMIT m_self->stopsChanged();
        Q_EMIT m_self->selectedStopChanged(m_selectedStop);
    }

    void reverseStops()
    {
        if(m_stops.empty()) {
            return;
        }
        std::ranges::reverse(m_stops);
        for(auto& stop : m_stops) {
            stop.position = 1.0 - stop.position;
        }
        m_selectedStop = static_cast<int>(m_stops.size()) - 1 - m_selectedStop;
        rebuildColourButtons();
        Q_EMIT m_self->stopsChanged();
        Q_EMIT m_self->selectedStopChanged(m_selectedStop);
    }

    void spaceStops()
    {
        const auto intervals = m_stops.size() > 1 ? m_stops.size() - 1 : 1;
        for(size_t index{0}; index < m_stops.size(); ++index) {
            m_stops[index].position = static_cast<qreal>(index) / static_cast<qreal>(intervals);
            const QSignalBlocker blocker{m_rows[index].position};
            m_rows[index].position->setValue(m_stops[index].position * 100.0);
        }
    }

    void rebuildColourButtons()
    {
        const QScopedValueRollback guard{m_rebuilding, true};
        m_coloursLayout->setAlignment(m_factory ? Qt::AlignTop : Qt::Alignment{});

        for(size_t index{0}; index < m_rows.size(); ++index) {
            const auto& row = m_rows[index];
            m_coloursLayout->setRowStretch(static_cast<int>(index) + 1, 0);
            for(QWidget* widget :
                {row.editor, static_cast<QWidget*>(row.position), static_cast<QWidget*>(row.remove)}) {
                m_coloursLayout->removeWidget(widget);
                widget->hide();
                widget->deleteLater();
            }
        }
        m_rows.clear();

        for(qsizetype index{0}; std::cmp_less(index, m_stops.size()); ++index) {
            QWidget* editor = m_factory ? m_factory(static_cast<int>(index), m_stops[index], m_coloursWidget) : nullptr;
            if(!editor) {
                auto* button = new ColourButton(m_stops[index].colour, m_coloursWidget);
                QObject::connect(button, &ColourButton::colourUpdated, m_self, [this, button]() {
                    const int stopIndex = m_self->colourEditorIndex(button);
                    if(stopIndex < 0) {
                        return;
                    }
                    auto stop   = m_stops[stopIndex];
                    stop.colour = button->colour();
                    m_self->updateColourStop(stopIndex, stop);
                });
                button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                editor = button;
            }
            editor->setParent(m_coloursWidget);
            editor->setFocusPolicy(Qt::StrongFocus);
            editor->setMinimumHeight(MinStopHeight);

            auto* position = new QDoubleSpinBox(m_coloursWidget);
            position->setRange(0.0, 100.0);
            position->setDecimals(2);
            position->setSuffix(u" %"_s);
            position->setKeyboardTracking(false);
            position->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            position->setToolTip(GradientEditor::tr("Positions must be distinct. Stops are reordered automatically."));
            position->setValue(m_stops[index].position * 100.0);
            position->installEventFilter(this);

            QObject::connect(position, &QDoubleSpinBox::valueChanged, m_self, [this, position](double value) {
                const int stopIndex = m_self->colourEditorIndex(position);
                if(stopIndex < 0) {
                    return;
                }
                m_self->setSelectedStop(stopIndex);

                auto stop     = m_stops[stopIndex];
                stop.position = value / 100.0;
                m_self->updateColourStop(stopIndex, stop);

                const QSignalBlocker blocker{position};
                position->setValue(m_stops[m_self->colourEditorIndex(position)].position * 100.0);
            });

            auto* remove = new QToolButton(m_coloursWidget);
            Gui::setThemeIcon(remove, Constants::Icons::Remove);
            remove->setToolTip(GradientEditor::tr("Remove"));
            remove->setFocusPolicy(Qt::StrongFocus);
            remove->setEnabled(m_stops.size() > 2);
            QObject::connect(remove, &QToolButton::clicked, m_self,
                             [this, remove] { removeStop(m_self->colourEditorIndex(remove)); });
            m_rows.emplace_back(editor, position, remove);
        }

        layoutRows();
        m_coloursLayout->activate();
        m_coloursWidget->adjustSize();
    }

    void layoutRows()
    {
        for(const auto& row : m_rows) {
            m_coloursLayout->removeWidget(row.editor);
            m_coloursLayout->removeWidget(row.position);
            m_coloursLayout->removeWidget(row.remove);
        }

        for(size_t index{0}; index < m_rows.size(); ++index) {
            const auto& row   = m_rows[index];
            const int gridRow = static_cast<int>(index) + 1;
            m_coloursLayout->addWidget(row.editor, gridRow, 0);
            m_coloursLayout->addWidget(row.position, gridRow, 1);
            m_coloursLayout->addWidget(row.remove, gridRow, 2);
            m_coloursLayout->setRowStretch(gridRow, m_factory ? 0 : 1);

            if(index > 0) {
                QWidget::setTabOrder(m_rows[index - 1].remove, row.editor);
            }
            QWidget::setTabOrder(row.editor, row.position);
            QWidget::setTabOrder(row.position, row.remove);
        }
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if(!m_rebuilding && event->type() == QEvent::FocusIn) {
            m_self->setSelectedStop(m_self->colourEditorIndex(qobject_cast<QWidget*>(watched)));
        }
        return QObject::eventFilter(watched, event);
    }

    GradientEditor* m_self;
    bool m_rebuilding{false};

    int m_selectedStop{-1};
    std::vector<GradientEditor::ColourStop> m_stops;
    std::vector<StopRow> m_rows;
    GradientEditor::ColourEditorFactory m_factory;
    QWidget* m_coloursWidget;
    QGridLayout* m_coloursLayout;
    QScrollArea* m_coloursScroll;
};

GradientEditor::GradientEditor(QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<GradientEditorPrivate>(this)}
{
    auto* addButton     = new QPushButton(tr("Add"), this);
    auto* reverseButton = new QPushButton(tr("Reverse"), this);
    auto* evenSpacing   = new QPushButton(tr("Evenly space stops"), this);

    auto* buttonLayout = new QGridLayout();
    buttonLayout->addWidget(addButton, 0, 0, 1, 2);
    buttonLayout->addWidget(reverseButton, 1, 0);
    buttonLayout->addWidget(evenSpacing, 1, 1);
    buttonLayout->setColumnStretch(0, 1);
    buttonLayout->setColumnStretch(1, 1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(p->m_coloursScroll, 1);
    layout->addLayout(buttonLayout);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    QObject::connect(addButton, &QPushButton::clicked, this, [this]() { p->addStop(); });
    QObject::connect(reverseButton, &QPushButton::clicked, this, [this]() { p->reverseStops(); });
    QObject::connect(evenSpacing, &QPushButton::clicked, this, &GradientEditor::spaceStopsEvenly);
}

GradientEditor::~GradientEditor() = default;

int GradientEditor::selectedStop() const
{
    return p->m_selectedStop;
}

int GradientEditor::colourEditorIndex(const QWidget* editor) const
{
    while(editor && editor->parentWidget() != p->m_coloursWidget) {
        editor = editor->parentWidget();
    }
    const auto row = std::ranges::find_if(p->m_rows, [editor](const auto& item) {
        return editor == item.editor || editor == item.position || editor == item.remove;
    });
    return row == p->m_rows.end() ? -1 : static_cast<int>(std::distance(p->m_rows.begin(), row));
}

std::vector<GradientEditor::ColourStop> GradientEditor::colourStops() const
{
    return p->m_stops;
}

void GradientEditor::setColourStops(const std::vector<ColourStop>& stops)
{
    p->m_stops.clear();

    for(const auto& stop : stops) {
        if(stop.colour.isValid() && std::isfinite(stop.position) && stop.position >= 0.0 && stop.position <= 1.0) {
            p->m_stops.push_back(stop);
        }
    }
    std::ranges::stable_sort(p->m_stops, {}, &ColourStop::position);

    const auto duplicates = std::ranges::unique(p->m_stops, {}, &ColourStop::position);
    p->m_stops.erase(duplicates.begin(), duplicates.end());

    p->m_selectedStop
        = p->m_stops.empty() ? -1 : std::clamp(p->m_selectedStop, 0, static_cast<int>(p->m_stops.size()) - 1);
    p->rebuildColourButtons();
}

void GradientEditor::setColourEditorFactory(ColourEditorFactory factory)
{
    p->m_factory = std::move(factory);
    p->rebuildColourButtons();
}

void GradientEditor::updateColourStop(int index, const ColourStop& stop)
{
    if(index < 0 || std::cmp_greater_equal(index, p->m_stops.size()) || !stop.colour.isValid()
       || !std::isfinite(stop.position) || stop.position < 0.0 || stop.position > 1.0) {
        return;
    }

    if(p->m_stops[index].colour == stop.colour && p->m_stops[index].data == stop.data
       && p->m_stops[index].position == stop.position) {
        return;
    }

    const bool moved = p->m_stops[index].position != stop.position;
    int updatedIndex{index};
    if(moved) {
        if(std::ranges::any_of(p->m_stops, [&stop](const auto& other) { return other.position == stop.position; })) {
            return;
        }

        p->m_stops.erase(p->m_stops.begin() + index);

        const auto target = std::ranges::lower_bound(p->m_stops, stop.position, {}, &ColourStop::position);
        updatedIndex      = static_cast<int>(std::distance(p->m_stops.begin(), target));
        p->m_stops.insert(target, stop);

        if(p->m_selectedStop == index) {
            p->m_selectedStop = updatedIndex;
        }
        else {
            p->m_selectedStop -= p->m_selectedStop > index;
            p->m_selectedStop += p->m_selectedStop >= updatedIndex;
        }

        if(updatedIndex != index) {
            const auto row = p->m_rows[index];
            p->m_rows.erase(p->m_rows.begin() + index);
            p->m_rows.insert(p->m_rows.begin() + updatedIndex, row);
            p->layoutRows();
            p->m_coloursScroll->ensureWidgetVisible(row.editor);
        }
    }
    else {
        const qreal position       = p->m_stops[index].position;
        p->m_stops[index]          = stop;
        p->m_stops[index].position = position;
    }

    {
        const QSignalBlocker blocker{p->m_rows[updatedIndex].position};
        p->m_rows[updatedIndex].position->setValue(p->m_stops[updatedIndex].position * 100.0);
    }

    Q_EMIT stopsChanged();
    if(moved) {
        Q_EMIT selectedStopChanged(p->m_selectedStop);
    }
}

void GradientEditor::setSelectedStop(int index)
{
    if(index < 0 || std::cmp_greater_equal(index, p->m_stops.size()) || index == p->m_selectedStop) {
        return;
    }
    p->m_selectedStop = index;
    Q_EMIT selectedStopChanged(index);
}

void GradientEditor::spaceStopsEvenly()
{
    p->spaceStops();
    Q_EMIT stopsChanged();
}
} // namespace Fooyin

#include "gui/widgets/moc_gradienteditor.cpp"
