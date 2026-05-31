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

#include <gui/widgets/colourbutton.h>

#include <QComboBox>
#include <QFrame>
#include <QGradient>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

constexpr auto PreviewWidth  = 36;
constexpr auto PreviewHeight = 120;
constexpr auto MinStopHeight = 20;

namespace Fooyin {
void setGradientColours(QGradient& gradient, const std::vector<QColor>& colours)
{
    if(colours.empty()) {
        gradient.setColorAt(0.0, Qt::transparent);
        gradient.setColorAt(1.0, Qt::transparent);
        return;
    }
    if(colours.size() == 1) {
        gradient.setColorAt(0.0, colours.front());
        gradient.setColorAt(1.0, colours.front());
        return;
    }

    const double step = 1.0 / static_cast<double>(colours.size() - 1);

    for(qsizetype index{0}; std::cmp_less(index, colours.size()); ++index) {
        gradient.setColorAt(static_cast<double>(index) * step, colours.at(index));
    }
}

QLinearGradient linearGradient(const QRectF& rect, Qt::Orientation orientation, const std::vector<QColor>& colours)
{
    QLinearGradient gradient = orientation == Qt::Vertical ? QLinearGradient{0.0, rect.top(), 0.0, rect.bottom()}
                                                           : QLinearGradient{rect.left(), 0.0, rect.right(), 0.0};
    setGradientColours(gradient, colours);
    return gradient;
}

class GradientEditorPreview : public QWidget
{
    Q_OBJECT

public:
    explicit GradientEditorPreview(QWidget* parent = nullptr)
        : QWidget{parent}
    {
        setFixedWidth(PreviewWidth);
        setMinimumHeight(PreviewHeight);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

    void setColours(const std::vector<QColor>& colours)
    {
        m_colours = colours;
        update();
    }

    void setOrientation(Qt::Orientation orientation)
    {
        m_orientation = orientation;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);

        QPainter painter{this};
        const QRectF bounds = rect().adjusted(1, 1, -1, -1);
        painter.fillRect(bounds, linearGradient(bounds, m_orientation, m_colours));
        painter.setPen(palette().mid().color());
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }

private:
    std::vector<QColor> m_colours;
    Qt::Orientation m_orientation{Qt::Vertical};
};

class GradientEditorPrivate
{
public:
    explicit GradientEditorPrivate(GradientEditor* self)
        : m_self{self}
        , m_coloursWidget{new QWidget(m_self)}
        , m_coloursLayout{new QVBoxLayout()}
        , m_coloursScroll{new QScrollArea(m_self)}
        , m_preview{new GradientEditorPreview(m_self)}
        , m_orientation{new QComboBox(m_self)}
    {
        m_coloursWidget->setLayout(m_coloursLayout);
        m_coloursLayout->setContentsMargins({});

        m_coloursScroll->setWidget(m_coloursWidget);
        m_coloursScroll->setWidgetResizable(true);
        m_coloursScroll->setFrameShape(QFrame::NoFrame);
        m_coloursScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_coloursScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_coloursScroll->setMinimumHeight(PreviewHeight);
        m_coloursScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        m_orientation->addItem(GradientEditor::tr("Vertical"), static_cast<int>(Qt::Vertical));
        m_orientation->addItem(GradientEditor::tr("Horizontal"), static_cast<int>(Qt::Horizontal));
        m_orientation->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void addColour(const QColor& colour)
    {
        if(colour.isValid()) {
            m_colours.emplace_back(colour);
        }
        else {
            m_colours.emplace_back(!m_colours.empty() ? m_colours.back() : m_self->palette().highlight().color());
        }

        rebuildColourButtons();
        updatePreview();
        Q_EMIT m_self->coloursChanged();
    }

    void removeColour()
    {
        if(m_colours.size() <= 1) {
            return;
        }

        m_colours.pop_back();
        rebuildColourButtons();
        updatePreview();
        Q_EMIT m_self->coloursChanged();
    }

    void reverseColours()
    {
        std::ranges::reverse(m_colours);
        rebuildColourButtons();
        updatePreview();
        Q_EMIT m_self->coloursChanged();
    }

    void rebuildColourButtons()
    {
        m_coloursWidget->setMinimumHeight(0);

        while(const auto* item = m_coloursLayout->takeAt(0)) {
            if(QWidget* widget = item->widget()) {
                widget->deleteLater();
            }
            delete item;
        }

        for(qsizetype index{0}; std::cmp_less(index, m_colours.size()); ++index) {
            auto* button = new ColourButton(m_colours.at(index), m_coloursWidget);
            button->setMinimumHeight(MinStopHeight);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            m_coloursLayout->addWidget(button, 1);

            QObject::connect(button, &ColourButton::colourUpdated, m_self, [this, button, index]() {
                m_colours[index] = button->colour();
                updatePreview();
                Q_EMIT m_self->coloursChanged();
            });
        }

        const auto stopCount    = static_cast<int>(m_colours.size());
        const int spacing       = stopCount > 1 ? m_coloursLayout->spacing() * (stopCount - 1) : 0;
        const int contentHeight = (stopCount * MinStopHeight) + spacing;
        m_coloursWidget->setMinimumHeight(std::max(PreviewHeight, contentHeight));
    }

    void updatePreview() const
    {
        m_preview->setColours(m_colours);
    }

    [[nodiscard]] Qt::Orientation orientation() const
    {
        return static_cast<Qt::Orientation>(m_orientation->currentData().toInt());
    }

    void setOrientation(Qt::Orientation orientation)
    {
        const int index = m_orientation->findData(static_cast<int>(orientation));
        m_orientation->setCurrentIndex(index >= 0 ? index : m_orientation->findData(static_cast<int>(Qt::Vertical)));
    }

    void setOrientationControlVisible(bool visible)
    {
        m_orientation->setVisible(visible);
    }

    [[nodiscard]] bool isOrientationControlVisible() const
    {
        return m_orientation->isVisible();
    }

    GradientEditor* m_self;

    std::vector<QColor> m_colours;
    QWidget* m_coloursWidget;
    QVBoxLayout* m_coloursLayout;
    QScrollArea* m_coloursScroll;
    GradientEditorPreview* m_preview;
    QComboBox* m_orientation;
};

GradientEditor::GradientEditor(QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<GradientEditorPrivate>(this)}
{
    auto* addButton     = new QPushButton(tr("Add"), this);
    auto* removeButton  = new QPushButton(tr("Remove"), this);
    auto* reverseButton = new QPushButton(tr("Reverse"), this);

    auto* buttonLayout = new QVBoxLayout();
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(removeButton);
    buttonLayout->addWidget(reverseButton);
    buttonLayout->addWidget(p->m_orientation, 0, Qt::AlignBottom);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(p->m_preview);
    layout->addWidget(p->m_coloursScroll);
    layout->addLayout(buttonLayout);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    QObject::connect(addButton, &QPushButton::clicked, this, [this]() { p->addColour({}); });
    QObject::connect(removeButton, &QPushButton::clicked, this, [this]() { p->removeColour(); });
    QObject::connect(reverseButton, &QPushButton::clicked, this, [this]() { p->reverseColours(); });
    QObject::connect(p->m_orientation, &QComboBox::currentIndexChanged, this, [this]() {
        const Qt::Orientation orientation = p->orientation();
        p->m_preview->setOrientation(orientation);
        Q_EMIT orientationChanged(orientation);
    });
}

GradientEditor::~GradientEditor() = default;

std::vector<QColor> GradientEditor::colours() const
{
    return p->m_colours;
}

void GradientEditor::setColours(const std::vector<QColor>& colours)
{
    p->m_colours.clear();

    for(const QColor& colour : colours) {
        if(colour.isValid()) {
            p->m_colours.emplace_back(colour);
        }
    }

    p->rebuildColourButtons();
    p->updatePreview();
}

Qt::Orientation GradientEditor::orientation() const
{
    return p->orientation();
}

bool GradientEditor::isOrientationControlVisible() const
{
    return p->isOrientationControlVisible();
}

void GradientEditor::setOrientation(Qt::Orientation orientation)
{
    p->setOrientation(orientation);
}

void GradientEditor::setOrientationControlVisible(bool visible)
{
    p->setOrientationControlVisible(visible);
}
} // namespace Fooyin

#include "gui/widgets/gradienteditor.moc"
#include "gui/widgets/moc_gradienteditor.cpp"
