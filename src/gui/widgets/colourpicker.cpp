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

#include <gui/widgets/colourpicker.h>

#include <core/coresettings.h>
#include <utils/fypaths.h>

#include <QAbstractButton>
#include <QApplication>
#include <QConicalGradient>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QScopedValueRollback>
#include <QSettings>
#include <QSlider>
#include <QStyleOptionFocusRect>
#include <QStyleOptionSlider>
#include <QVBoxLayout>

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

using namespace Qt::StringLiterals;

constexpr size_t HistorySize = 32;
constexpr auto RecentColours = "ColourPicker/RecentColours"_L1;

namespace Fooyin {
namespace {
class HexColourValidator : public QRegularExpressionValidator
{
public:
    explicit HexColourValidator(QObject* parent)
        : QRegularExpressionValidator{QRegularExpression{u"#[0-9A-Fa-f]{0,8}"_s}, parent}
    { }

    State validate(QString& input, int& position) const override
    {
        if(!input.startsWith(u'#')) {
            input.prepend(u'#');
            ++position;
        }
        return QRegularExpressionValidator::validate(input, position);
    }
};

void paintChequerboard(QPainter& painter, const QRectF& bounds)
{
    painter.fillRect(bounds, QColor{220, 220, 220});

    for(int y{0}; y < bounds.height(); y += 6) {
        for(int x{0}; x < bounds.width(); x += 6) {
            if(((x / 6) + (y / 6)) % 2) {
                painter.fillRect(QRectF{bounds.left() + x, bounds.top() + y, 6, 6}, QColor{160, 160, 160});
            }
        }
    }
}

void paintFocus(QPainter& painter, const QWidget* widget)
{
    if(!widget->hasFocus()) {
        return;
    }

    QStyleOptionFocusRect option;
    option.initFrom(widget);
    option.rect            = widget->rect();
    option.backgroundColor = widget->palette().color(QPalette::Window);
    widget->style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, widget);
}

class ColourPreview : public QWidget
{
public:
    explicit ColourPreview(QWidget* parent)
        : QWidget{parent}
    {
        setMinimumHeight(28);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    void setColour(const QColor& colour)
    {
        m_colour = colour;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF bounds = QRectF{rect()}.adjusted(0.5, 0.5, -0.5, -0.5);

        QPainterPath path;
        path.addRoundedRect(bounds, 5, 5);

        painter.save();
        painter.setClipPath(path);
        paintChequerboard(painter, bounds);
        painter.fillRect(bounds, m_colour);
        painter.restore();

        auto border = palette().color(QPalette::WindowText);
        border.setAlphaF(0.25);

        painter.setPen(QPen{border, 1.0});
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

private:
    QColor m_colour{Qt::white};
};

class ColourHistory : public QObject
{
    Q_OBJECT

public:
    explicit ColourHistory(QObject* parent);

    [[nodiscard]] const std::vector<QColor>& colours() const;

    void add(const QColor& colour);
    void clear();

Q_SIGNALS:
    void changed();

private:
    void save();

    std::vector<QColor> m_colours;
};

ColourHistory::ColourHistory(QObject* parent)
    : QObject{parent}
{
    const FyStateSettings settings;
    const auto saved = settings.value(RecentColours).toStringList();
    for(const auto& text : saved) {
        const QColor colour{text};
        if(colour.isValid() && std::ranges::find(m_colours, colour) == m_colours.cend()) {
            m_colours.push_back(colour);
            if(m_colours.size() == HistorySize) {
                break;
            }
        }
    }
}

const std::vector<QColor>& ColourHistory::colours() const
{
    return m_colours;
}

void ColourHistory::add(const QColor& colour)
{
    if(!colour.isValid()) {
        return;
    }

    const auto stored = QColor::fromRgba(colour.rgba());
    if(!m_colours.empty() && m_colours.front() == stored) {
        return;
    }

    std::erase(m_colours, stored);
    m_colours.insert(m_colours.begin(), stored);
    if(m_colours.size() > HistorySize) {
        m_colours.resize(HistorySize);
    }

    save();
}

void ColourHistory::clear()
{
    if(!m_colours.empty()) {
        m_colours.clear();
        save();
    }
}

void ColourHistory::save()
{
    QStringList saved;
    for(const auto& colour : m_colours) {
        saved.push_back(colour.name(QColor::HexArgb));
    }
    {
        FyStateSettings settings;
        settings.setValue(RecentColours, saved);
    }
    Q_EMIT changed();
}

ColourHistory* colourHistory()
{
    static QPointer<ColourHistory> history;
    if(!history) {
        history = new ColourHistory(qApp);
    }
    return history;
}

class HistorySwatch : public QAbstractButton
{
public:
    explicit HistorySwatch(QWidget* parent)
        : QAbstractButton{parent}
    {
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        const int side = std::max(20, fontMetrics().height() + 4);
        return {side, side};
    }

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        return sizeHint();
    }

    void setColour(const QColor& colour, bool selected)
    {
        if(m_colour == colour && m_selected == selected && isEnabled() == colour.isValid()) {
            return;
        }

        m_colour   = colour;
        m_selected = selected;
        setEnabled(colour.isValid());

        const auto name = colour.isValid() ? ColourPicker::tr("Use colour ‘%1’").arg(colour.name(QColor::HexArgb))
                                           : ColourPicker::tr("Empty custom colour");
        setToolTip(name);
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);

        const auto bounds = QRectF{rect()}.adjusted(2.5, 2.5, -2.5, -2.5);
        QPainterPath path;
        path.addRoundedRect(bounds, 2, 2);

        painter.save();
        painter.setClipPath(path);

        if(m_colour.isValid()) {
            paintChequerboard(painter, bounds);
            painter.fillRect(bounds, m_colour);
        }
        else {
            painter.fillRect(bounds, palette().color(QPalette::Active, QPalette::Base));
        }

        painter.restore();

        auto border = palette().color(QPalette::Active, QPalette::WindowText);
        border.setAlphaF(0.5F);
        if(m_selected) {
            border = palette().color(QPalette::Active, QPalette::Highlight);
        }
        painter.setPen(QPen{border, m_selected ? 2.0 : 1.0});
        painter.drawPath(path);
        paintFocus(painter, this);
    }

private:
    QColor m_colour;
    bool m_selected{false};
};

class ColourWheel : public QWidget
{
    Q_OBJECT

public:
    explicit ColourWheel(QWidget* parent);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    void setHsv(double hue, double saturation, double value);

Q_SIGNALS:
    void hueChanged(double hue);
    void positionChanged(double saturation, double value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class DragTarget : uint8_t
    {
        None = 0,
        Hue,
        Field,
    };

    [[nodiscard]] QPointF wheelCentre() const;
    [[nodiscard]] double outerRadius() const;
    [[nodiscard]] double innerRadius() const;
    [[nodiscard]] QRectF fieldRect() const;

    void setPosition(const QPointF& position);
    void setPosition(const QPointF& position, DragTarget target);

    double m_hue{0.0};
    double m_saturation{0.0};
    double m_value{1.0};
    DragTarget m_dragTarget{DragTarget::None};
};

ColourWheel::ColourWheel(QWidget* parent)
    : QWidget{parent}
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setToolTip(ColourPicker::tr("Use the arrow keys to adjust saturation and value.\nHold Ctrl to adjust hue, "
                                "or Shift for larger steps."));
}

QSize ColourWheel::sizeHint() const
{
    return {340, 340};
}

QSize ColourWheel::minimumSizeHint() const
{
    return {280, 280};
}

void ColourWheel::setHsv(double hue, double saturation, double value)
{
    m_hue        = hue;
    m_saturation = saturation;
    m_value      = value;
    update();
}

void ColourWheel::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    const auto centre   = wheelCentre();
    const double radius = outerRadius();
    const double inner  = innerRadius();

    QPainterPath ring;
    ring.setFillRule(Qt::OddEvenFill);
    ring.addEllipse(centre, radius, radius);
    ring.addEllipse(centre, inner, inner);

    QConicalGradient hue{centre, 0.0};
    for(int stop{0}; stop <= 6; ++stop) {
        hue.setColorAt(static_cast<float>(stop) / 6.0F, QColor::fromHsvF(static_cast<float>(stop) / 6.0F, 1.0F, 1.0F));
    }
    painter.fillPath(ring, hue);

    const auto bounds = fieldRect();
    QLinearGradient saturation{bounds.topLeft(), bounds.topRight()};
    saturation.setColorAt(0.0, QColor::fromHsvF(static_cast<float>(m_hue), 1.0F, 1.0F));
    saturation.setColorAt(1.0, Qt::white);
    painter.fillRect(bounds, saturation);

    QLinearGradient value{bounds.topLeft(), bounds.bottomLeft()};
    value.setColorAt(0.0, Qt::transparent);
    value.setColorAt(1.0, Qt::black);
    painter.fillRect(bounds, value);

    if(!isEnabled()) {
        auto overlay = palette().color(QPalette::Window);
        overlay.setAlpha(160);
        painter.fillPath(ring, overlay);
        painter.fillRect(bounds, overlay);
    }

    const double angle = m_hue * 2.0 * std::numbers::pi;
    const QPointF direction{std::cos(angle), -std::sin(angle)};
    const QLineF marker{centre + direction * (inner + 3.0), centre + direction * (radius - 3.0)};
    painter.setPen(QPen{Qt::black, 3.0});
    painter.drawLine(marker);
    painter.setPen(QPen{Qt::white, 1.0});
    painter.drawLine(marker);

    const QPointF position{bounds.right() - (m_saturation * bounds.width()),
                           bounds.bottom() - (m_value * bounds.height())};
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen{Qt::black, 3.0});
    painter.drawEllipse(position, 5, 5);
    painter.setPen(QPen{Qt::white, 1.5});
    painter.drawEllipse(position, 5, 5);

    paintFocus(painter, this);
}

void ColourWheel::mousePressEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const auto offset     = event->position() - wheelCentre();
    const double distance = std::hypot(offset.x(), offset.y());

    if(distance >= innerRadius() && distance <= outerRadius()) {
        m_dragTarget = DragTarget::Hue;
    }
    else if(fieldRect().contains(event->position())) {
        m_dragTarget = DragTarget::Field;
    }
    else {
        event->ignore();
        return;
    }

    setFocus(Qt::MouseFocusReason);
    setPosition(event->position());
    event->accept();
}

void ColourWheel::mouseMoveEvent(QMouseEvent* event)
{
    if(event->buttons() & Qt::LeftButton && m_dragTarget != DragTarget::None) {
        setPosition(event->position());
        event->accept();
    }
    else {
        QWidget::mouseMoveEvent(event);
    }
}

void ColourWheel::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton && m_dragTarget != DragTarget::None) {
        const auto position = event->position();
        const auto target   = m_dragTarget;
        m_dragTarget        = DragTarget::None;
        setPosition(position, target);
        event->accept();
    }
    else {
        QWidget::mouseReleaseEvent(event);
    }
}

void ColourWheel::keyPressEvent(QKeyEvent* event)
{
    const bool horizontal = event->key() == Qt::Key_Left || event->key() == Qt::Key_Right;
    const bool vertical   = event->key() == Qt::Key_Up || event->key() == Qt::Key_Down;
    if(!horizontal && !vertical) {
        QWidget::keyPressEvent(event);
        return;
    }

    const double step = event->modifiers() & Qt::ShiftModifier ? 0.1 : 0.01;
    if(event->modifiers() & Qt::ControlModifier) {
        const double direction = event->key() == Qt::Key_Right || event->key() == Qt::Key_Up ? 1.0 : -1.0;
        const double hue       = m_hue + (direction * step);
        Q_EMIT hueChanged(hue - std::floor(hue));
    }
    else {
        const double saturation = m_saturation + (horizontal ? (event->key() == Qt::Key_Left ? step : -step) : 0.0);
        const double value      = m_value + (vertical ? (event->key() == Qt::Key_Up ? step : -step) : 0.0);
        Q_EMIT positionChanged(std::clamp(saturation, 0.0, 1.0), std::clamp(value, 0.0, 1.0));
    }

    event->accept();
}

QPointF ColourWheel::wheelCentre() const
{
    return QRectF{rect()}.center();
}

double ColourWheel::outerRadius() const
{
    return std::max(1.0, (std::min(width(), height()) / 2.0) - 8.0);
}

double ColourWheel::innerRadius() const
{
    return outerRadius() * 0.75;
}

QRectF ColourWheel::fieldRect() const
{
    const double halfSide = std::max(1.0, innerRadius() - 6.0) / std::numbers::sqrt2;
    return {wheelCentre() - QPointF{halfSide, halfSide}, QSizeF{halfSide * 2.0, halfSide * 2.0}};
}

void ColourWheel::setPosition(const QPointF& position)
{
    setPosition(position, m_dragTarget);
}

void ColourWheel::setPosition(const QPointF& position, DragTarget target)
{
    if(target == DragTarget::Hue) {
        const auto offset = position - wheelCentre();
        const double hue  = std::atan2(-offset.y(), offset.x()) / (2.0 * std::numbers::pi);
        Q_EMIT hueChanged(hue - std::floor(hue));
    }
    else if(target == DragTarget::Field) {
        const auto bounds = fieldRect();
        Q_EMIT positionChanged(std::clamp((bounds.right() - position.x()) / bounds.width(), 0.0, 1.0),
                               std::clamp((bounds.bottom() - position.y()) / bounds.height(), 0.0, 1.0));
    }
}

class ColourChannelSlider : public QAbstractSlider
{
public:
    enum Channel : uint8_t
    {
        Hue = 0,
        Saturation,
        Value,
        Red,
        Green,
        Blue,
        Opacity,
        Count,
    };

    ColourChannelSlider(Channel channel, QWidget* parent);

    [[nodiscard]] QSize sizeHint() const override;

    void setColour(const QColor& colour, double hue, double saturation, double value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] bool isUpsideDown() const;
    [[nodiscard]] QRectF trackRect() const;

    void setPosition(const QPointF& position);

    Channel m_channel;
    QColor m_colour{Qt::white};
    double m_hue{0.0};
    double m_saturation{0.0};
    double m_value{1.0};
};

ColourChannelSlider::ColourChannelSlider(Channel channel, QWidget* parent)
    : QAbstractSlider{parent}
    , m_channel{channel}
{
    const bool rgb = channel >= Red && channel <= Blue;
    setRange(0, channel == Hue ? 3599 : (rgb ? 255 : 1000));
    setSingleStep(rgb ? 1 : 10);
    setPageStep(rgb ? 16 : 100);

    setMinimumWidth(110);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);
    setOrientation(Qt::Horizontal);
}

QSize ColourChannelSlider::sizeHint() const
{
    return {150, std::max(20, fontMetrics().height() + 2)};
}

void ColourChannelSlider::setColour(const QColor& colour, double hue, double saturation, double value)
{
    m_colour     = colour;
    m_hue        = hue;
    m_saturation = saturation;
    m_value      = value;
    update();
}

void ColourChannelSlider::paintEvent(QPaintEvent* /*event*/)
{
    const bool upsideDown = isUpsideDown();
    const auto bounds     = trackRect();
    QLinearGradient gradient{upsideDown ? bounds.topRight() : bounds.topLeft(),
                             upsideDown ? bounds.topLeft() : bounds.topRight()};

    if(m_channel == Hue) {
        for(int stop{0}; stop <= 6; ++stop) {
            gradient.setColorAt(stop / 6.0, QColor::fromHsvF(static_cast<float>(stop) / 6.0F, 1.0F, 1.0F));
        }
    }
    else {
        QColor first = m_colour;
        QColor last  = m_colour;
        first.setAlpha(255);
        last.setAlpha(255);
        switch(m_channel) {
            case Saturation:
                first = QColor::fromHsvF(static_cast<float>(m_hue), 0.0F, static_cast<float>(m_value));
                last  = QColor::fromHsvF(static_cast<float>(m_hue), 1.0F, static_cast<float>(m_value));
                break;
            case Value:
                first = Qt::black;
                last  = QColor::fromHsvF(static_cast<float>(m_hue), static_cast<float>(m_saturation), 1.0F);
                break;
            case Red:
                first.setRed(0);
                last.setRed(255);
                break;
            case Green:
                first.setGreen(0);
                last.setGreen(255);
                break;
            case Blue:
                first.setBlue(0);
                last.setBlue(255);
                break;
            case Opacity:
                first.setAlpha(0);
                break;
            default:
                break;
        }
        gradient.setColorAt(0.0, first);
        gradient.setColorAt(1.0, last);
    }

    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(bounds, 5, 5);

    painter.save();
    painter.setClipPath(path);
    paintChequerboard(painter, bounds);
    painter.fillRect(bounds, gradient);

    if(!isEnabled()) {
        auto overlay = palette().color(QPalette::Window);
        overlay.setAlpha(160);
        painter.fillRect(bounds, overlay);
    }

    painter.restore();
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawPath(path);

    const double fraction = (value() - minimum()) / static_cast<double>(maximum() - minimum());
    const double position = bounds.left() + ((upsideDown ? 1.0 - fraction : fraction) * bounds.width());
    const double markerX  = std::clamp(position, bounds.left() + 2.0, bounds.right() - 2.0);
    const QLineF marker{QPointF{markerX, bounds.top()}, QPointF{markerX, bounds.bottom()}};
    painter.save();
    painter.setClipPath(path);
    painter.setPen(QPen{Qt::black, 4.0});
    painter.drawLine(marker);
    painter.setPen(QPen{Qt::white, 2.0});
    painter.drawLine(marker);
    painter.restore();

    if(hasFocus()) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen{palette().color(QPalette::Highlight), 2.0});
        painter.drawPath(path);
    }
}

void ColourChannelSlider::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        setFocus(Qt::MouseFocusReason);
        setSliderDown(true);
        setPosition(event->position());
        event->accept();
    }
    else {
        QAbstractSlider::mousePressEvent(event);
    }
}

void ColourChannelSlider::mouseMoveEvent(QMouseEvent* event)
{
    if(event->buttons() & Qt::LeftButton) {
        setPosition(event->position());
        event->accept();
    }
    else {
        QAbstractSlider::mouseMoveEvent(event);
    }
}

void ColourChannelSlider::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        setPosition(event->position());
        setSliderDown(false);
        event->accept();
    }
    else {
        QAbstractSlider::mouseReleaseEvent(event);
    }
}

bool ColourChannelSlider::isUpsideDown() const
{
    return invertedAppearance() != (layoutDirection() == Qt::RightToLeft);
}

QRectF ColourChannelSlider::trackRect() const
{
    return QRectF{rect()}.adjusted(2.0, 1.0, -2.0, -1.0);
}

void ColourChannelSlider::setPosition(const QPointF& position)
{
    const auto bounds = trackRect();
    const int offset  = static_cast<int>(std::lround(position.x() - bounds.left()));
    setValue(QStyle::sliderValueFromPosition(minimum(), maximum(), offset, static_cast<int>(bounds.width()),
                                             isUpsideDown()));
}
} // namespace

class ColourPickerPrivate
{
public:
    explicit ColourPickerPrivate(ColourPicker* self);

    QGridLayout* createChannelControls();
    void createHistoryControls(QGridLayout* layout);
    void updateControls();
    void updateHistory();
    void changeChannel(size_t channel, double value);
    void updateFromHsv();
    void applyColour(const QColor& colour);
    void setColour(const QColor& colour);

    struct ChannelEditor
    {
        ColourChannelSlider* slider{nullptr};
        QDoubleSpinBox* spin{nullptr};
    };

    ColourPicker* m_self;
    QColor m_colour{Qt::white};
    QColor m_original{Qt::white};
    double m_hue{0.0};
    double m_saturation{0.0};
    double m_value{1.0};
    ColourWheel* m_wheel;
    std::array<ChannelEditor, ColourChannelSlider::Count> m_channels;
    QLineEdit* m_hex;
    ColourPreview* m_originalPreview;
    ColourPreview* m_currentPreview;
    std::array<HistorySwatch*, HistorySize> m_historySwatches{};
    QPushButton* m_addHistory;
    QPushButton* m_clearHistory;
    bool m_updating{false};
    bool m_editingHex{false};
};

ColourPickerPrivate::ColourPickerPrivate(ColourPicker* self)
    : m_self{self}
    , m_wheel{new ColourWheel(self)}
    , m_channels{}
    , m_hex{new QLineEdit(self)}
    , m_originalPreview{new ColourPreview(self)}
    , m_currentPreview{new ColourPreview(self)}
    , m_addHistory{new QPushButton(ColourPicker::tr("Add"), self)}
    , m_clearHistory{new QPushButton(ColourPicker::tr("Clear"), self)}
{
    auto* body = new QHBoxLayout();
    body->addWidget(m_wheel, 3);
    body->addLayout(createChannelControls(), 2);

    auto* reset = new QPushButton(ColourPicker::tr("Reset"), self);
    reset->setAutoDefault(false);
    reset->setToolTip(ColourPicker::tr("Restore the original colour"));

    auto* footer = new QGridLayout();
    footer->addWidget(new QLabel(ColourPicker::tr("Original"), self), 0, 0);
    footer->addWidget(new QLabel(ColourPicker::tr("Current"), self), 0, 1);
    footer->addWidget(m_originalPreview, 1, 0);
    footer->addWidget(m_currentPreview, 1, 1);
    footer->addWidget(reset, 2, 0, 1, 2);
    footer->setColumnStretch(0, 1);
    footer->setColumnStretch(1, 1);
    footer->setColumnStretch(2, 6);

    createHistoryControls(footer);

    QWidget* previous{m_wheel};
    const auto tabAfter = [&previous](QWidget* next) {
        QWidget::setTabOrder(previous, next);
        previous = next;
    };

    for(const auto& editor : m_channels) {
        tabAfter(editor.slider);
        tabAfter(editor.spin);
    }
    tabAfter(m_hex);
    for(auto* swatch : m_historySwatches) {
        tabAfter(swatch);
    }
    tabAfter(reset);
    tabAfter(m_addHistory);
    tabAfter(m_clearHistory);

    self->setFocusProxy(m_wheel);

    auto* layout = new QVBoxLayout(self);
    layout->setContentsMargins({});
    layout->addLayout(body, 1);
    layout->addLayout(footer);

    QObject::connect(m_wheel, &ColourWheel::positionChanged, self, [this](double saturation, double value) {
        m_saturation = saturation;
        m_value      = value;
        updateFromHsv();
    });
    QObject::connect(m_wheel, &ColourWheel::hueChanged, self, [this](double hue) {
        m_hue = hue;
        updateFromHsv();
    });
    QObject::connect(m_hex, &QLineEdit::textEdited, self, [this](const QString& text) {
        if((text.size() == 7 || text.size() == 9) && text.startsWith(u'#')) {
            QColor colour{text};
            if(colour.isValid()) {
                if(text.size() == 7) {
                    colour.setAlphaF(m_colour.alphaF());
                }
                const QPointer guard{m_self};
                m_editingHex = true;
                setColour(colour);
                if(guard) {
                    m_editingHex = false;
                }
            }
        }
    });
    QObject::connect(m_hex, &QLineEdit::editingFinished, self,
                     [this] { m_hex->setText(m_colour.name(QColor::HexArgb)); });
    QObject::connect(reset, &QPushButton::clicked, self, [this] { setColour(m_original); });
    QObject::connect(colourHistory(), &ColourHistory::changed, self, [this] { updateHistory(); });
    updateControls();
}

QGridLayout* ColourPickerPrivate::createChannelControls()
{
    using Channel = ColourChannelSlider;
    const std::array names{ColourPicker::tr("Hue"),    ColourPicker::tr("Saturation"), ColourPicker::tr("Value"),
                           ColourPicker::tr("Red"),    ColourPicker::tr("Green"),      ColourPicker::tr("Blue"),
                           ColourPicker::tr("Opacity")};

    auto* layout = new QGridLayout();

    int row{0};
    for(size_t index{0}; index < m_channels.size(); ++index) {
        if(index == Channel::Red || index == Channel::Opacity) {
            auto* separator = new QFrame(m_self);
            separator->setFrameShape(QFrame::HLine);
            layout->addWidget(separator, row++, 0, 1, 3);
        }

        const auto channel = static_cast<Channel::Channel>(index);
        const bool rgb     = channel >= Channel::Red && channel <= Channel::Blue;

        auto& editor  = m_channels[index];
        editor.slider = new ColourChannelSlider(channel, m_self);
        editor.spin   = new QDoubleSpinBox(m_self);
        editor.spin->setDecimals(rgb ? 0 : 1);
        editor.spin->setRange(0.0, rgb ? 255.0 : (channel == Channel::Hue ? 359.9 : 100.0));
        editor.spin->setSuffix(rgb ? QString{} : (channel == Channel::Hue ? u"°"_s : u" %"_s));
        editor.spin->setAlignment(Qt::AlignRight);

        auto* label = new QLabel(names[index] + u":"_s, m_self);
        layout->addWidget(label, row, 0);
        layout->addWidget(editor.slider, row, 1);
        layout->addWidget(editor.spin, row++, 2);

        QObject::connect(editor.slider, &QSlider::valueChanged, m_self, [this, index, rgb](int value) {
            if(!m_updating) {
                changeChannel(index, rgb ? value : value / 10.0);
            }
        });
        QObject::connect(editor.spin, &QDoubleSpinBox::valueChanged, m_self, [this, index](double value) {
            if(!m_updating) {
                changeChannel(index, value);
            }
        });
    }

    m_hex->setMaxLength(9);
    m_hex->setValidator(new HexColourValidator(m_hex));
    m_hex->setPlaceholderText(u"#AARRGGBB"_s);
    m_hex->setToolTip(ColourPicker::tr("Enter #AARRGGBB, or #RRGGBB to keep the current opacity"));

    layout->addWidget(new QLabel(ColourPicker::tr("Hex (ARGB)") + u":"_s, m_self), row, 0);
    layout->addWidget(m_hex, row, 1, 1, 2);
    layout->setColumnStretch(1, 1);

    return layout;
}

void ColourPickerPrivate::createHistoryControls(QGridLayout* layout)
{
    m_addHistory->setAutoDefault(false);
    m_addHistory->setToolTip(ColourPicker::tr("Add the current colour to custom colours"));
    m_clearHistory->setAutoDefault(false);
    m_clearHistory->setToolTip(ColourPicker::tr("Clear custom colours"));

    auto* toolbar = new QHBoxLayout();
    toolbar->addStretch();
    toolbar->addWidget(m_addHistory);
    toolbar->addWidget(m_clearHistory);

    auto* swatches = new QGridLayout();
    swatches->setSpacing(1);

    for(size_t index{0}; index < m_historySwatches.size(); ++index) {
        auto* swatch             = new HistorySwatch(m_self);
        m_historySwatches[index] = swatch;
        swatches->addWidget(swatch, static_cast<int>(index / 16), static_cast<int>(index % 16));
        swatches->setColumnStretch(static_cast<int>(index % 16), 1);
        QObject::connect(swatch, &QAbstractButton::clicked, m_self, [this, index] {
            const auto& colours = colourHistory()->colours();
            if(index < colours.size()) {
                setColour(colours[index]);
            }
        });
    }

    QObject::connect(m_addHistory, &QPushButton::clicked, m_self, [this] { ColourPicker::addToHistory(m_colour); });
    QObject::connect(m_clearHistory, &QPushButton::clicked, colourHistory(), &ColourHistory::clear);

    layout->addWidget(new QLabel(ColourPicker::tr("Custom colours"), m_self), 0, 2);
    layout->addLayout(swatches, 1, 2);
    layout->addLayout(toolbar, 2, 2);
}

void ColourPickerPrivate::updateHistory()
{
    const auto& colours = colourHistory()->colours();
    for(size_t index{0}; index < m_historySwatches.size(); ++index) {
        const auto colour = index < colours.size() ? colours[index] : QColor{};
        m_historySwatches[index]->setColour(colour, colour.isValid() && colour.rgba() == m_colour.rgba());
    }
    m_clearHistory->setEnabled(!colours.empty());
}

void ColourPickerPrivate::updateControls()
{
    const QScopedValueRollback guard{m_updating, true};

    m_wheel->setHsv(m_hue, m_saturation, m_value);

    const std::array values{m_hue * 360.0,
                            m_saturation * 100.0,
                            m_value * 100.0,
                            static_cast<double>(m_colour.red()),
                            static_cast<double>(m_colour.green()),
                            static_cast<double>(m_colour.blue()),
                            m_colour.alphaF() * 100.0};

    for(size_t index{0}; index < m_channels.size(); ++index) {
        const auto& editor = m_channels[index];
        const bool rgb     = index >= ColourChannelSlider::Red && index <= ColourChannelSlider::Blue;
        editor.slider->setColour(m_colour, m_hue, m_saturation, m_value);
        editor.slider->setValue(static_cast<int>(std::lround(values[index] * (rgb ? 1.0 : 10.0))));
        editor.spin->setValue(values[index]);
    }

    if(!m_editingHex) {
        m_hex->setText(m_colour.name(QColor::HexArgb));
    }

    m_currentPreview->setColour(m_colour);
    m_originalPreview->setColour(m_original);
    updateHistory();
}

void ColourPickerPrivate::changeChannel(size_t channel, double value)
{
    using Channel = ColourChannelSlider;
    switch(channel) {
        case Channel::Hue:
            m_hue = value / 360.0;
            updateFromHsv();
            return;
        case Channel::Saturation:
            m_saturation = value / 100.0;
            updateFromHsv();
            return;
        case Channel::Value:
            m_value = value / 100.0;
            updateFromHsv();
            return;
        default:
            break;
    }

    auto colour{m_colour};
    switch(channel) {
        case Channel::Red:
            colour.setRed(static_cast<int>(std::lround(value)));
            break;
        case Channel::Green:
            colour.setGreen(static_cast<int>(std::lround(value)));
            break;
        case Channel::Blue:
            colour.setBlue(static_cast<int>(std::lround(value)));
            break;
        case Channel::Opacity:
            colour.setAlphaF(static_cast<float>(value) / 100.0F);
            applyColour(colour);
            return;
        default:
            return;
    }
    setColour(colour);
}

void ColourPickerPrivate::updateFromHsv()
{
    applyColour(QColor::fromHsvF(static_cast<float>(m_hue), static_cast<float>(m_saturation),
                                 static_cast<float>(m_value), m_colour.alphaF())
                    .toRgb());
}

void ColourPickerPrivate::applyColour(const QColor& colour)
{
    const bool changed = std::exchange(m_colour, colour) != m_colour;
    updateControls();
    if(changed) {
        Q_EMIT m_self->colourChanged(m_colour);
    }
}

void ColourPickerPrivate::setColour(const QColor& colour)
{
    if(!colour.isValid()) {
        return;
    }

    const auto rgb = colour.toRgb();
    if(rgb == m_colour) {
        return;
    }
    // Achromatic RGB values can't describe the hue or the saturation of black
    if(rgb.hsvHueF() >= 0.0) {
        m_hue = rgb.hsvHueF();
    }
    if(rgb.valueF() > 0.0) {
        m_saturation = rgb.hsvSaturationF();
    }

    m_value = rgb.valueF();
    applyColour(rgb);
}

ColourPicker::ColourPicker(QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<ColourPickerPrivate>(this)}
{ }

ColourPicker::~ColourPicker()
{
    for(const auto* child : findChildren<QWidget*>()) {
        QObject::disconnect(child, nullptr, this, nullptr);
    }
}

QColor ColourPicker::colour() const
{
    return p->m_colour;
}

void ColourPicker::setColour(const QColor& colour)
{
    p->setColour(colour);
}

void ColourPicker::setOriginalColour(const QColor& colour)
{
    if(colour.isValid()) {
        p->m_original = colour.toRgb();
        p->m_originalPreview->setColour(p->m_original);
    }
}

void ColourPicker::addToHistory(const QColor& colour)
{
    colourHistory()->add(colour);
}
} // namespace Fooyin

#include "colourpicker.moc"
#include "gui/widgets/moc_colourpicker.cpp"
