/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <gui/widgets/colourbutton.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>

namespace Fooyin {
class ColourSwatch : public QPushButton
{
public:
    explicit ColourSwatch(const QColor& colour, QWidget* parent = nullptr)
        : QPushButton{parent}
        , m_colour{colour}
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    [[nodiscard]] QColor colour() const
    {
        return m_colour;
    }

    void setColour(const QColor& colour)
    {
        m_colour = colour;
        update();
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        return {20, 20};
    }

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        return {20, 20};
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);

        QPainter painter{this};
        painter.fillRect(rect(), m_colour);

        if(!isEnabled()) {
            QColor overlay = palette().color(QPalette::Window);
            overlay.setAlpha(160);
            painter.fillRect(rect(), overlay);
        }

        const auto colourGroup = isEnabled() ? QPalette::Active : QPalette::Disabled;
        painter.setPen({palette().color(colourGroup, QPalette::Text), 2});
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }

private:
    QColor m_colour;
};

ColourButton::ColourButton(QWidget* parent)
    : ColourButton{{}, {}, false, parent}
{ }

ColourButton::ColourButton(bool checkable, QWidget* parent)
    : ColourButton{{}, {}, checkable, parent}
{ }

ColourButton::ColourButton(const QColor& colour, QWidget* parent)
    : ColourButton{{}, colour, false, parent}
{ }

ColourButton::ColourButton(const QString& text, const QColor& colour, QWidget* parent)
    : ColourButton{text, colour, false, parent}
{ }

ColourButton::ColourButton(const QString& text, bool checkable, QWidget* parent)
    : ColourButton{text, {}, checkable, parent}
{ }

ColourButton::ColourButton(const QString& text, const QColor& colour, bool checkable, QWidget* parent)
    : QWidget{parent}
    , m_changed{false}
    , m_checkBox{new QCheckBox(text, this)}
    , m_swatch{new ColourSwatch(colour, this)}
{
    m_checkBox->setVisible(checkable);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_checkBox);
    layout->addWidget(m_swatch, 1);

    QObject::connect(m_swatch, &QPushButton::clicked, this, [this]() {
        Q_EMIT clicked();
        pickColour();
    });
    QObject::connect(m_checkBox, &QCheckBox::toggled, this, [this](bool checked) {
        updateButtonState();
        Q_EMIT toggled(checked);
    });

    updateButtonState();
}

QColor ColourButton::colour() const
{
    return m_swatch->colour();
}

bool ColourButton::colourChanged() const
{
    return m_changed;
}

bool ColourButton::isCheckable() const
{
    return !m_checkBox->isHidden();
}

bool ColourButton::isChecked() const
{
    return !isCheckable() || m_checkBox->isChecked();
}

int ColourButton::labelWidthHint() const
{
    return m_checkBox->sizeHint().width();
}

QString ColourButton::text() const
{
    return m_checkBox->text();
}

void ColourButton::setColour(const QColor& colour)
{
    if(m_swatch->colour() == colour) {
        return;
    }

    m_swatch->setColour(colour);
    Q_EMIT colourUpdated(colour);
}

void ColourButton::setCheckable(bool checkable)
{
    m_checkBox->setToolTip(toolTip());
    m_checkBox->setVisible(checkable);
    updateButtonState();
    updateGeometry();
}

void ColourButton::setChecked(bool checked)
{
    m_checkBox->setChecked(checked);
}

void ColourButton::setLabelWidth(int width)
{
    m_checkBox->setMinimumWidth(std::max(width, labelWidthHint()));
    updateGeometry();
}

void ColourButton::setText(const QString& text)
{
    m_checkBox->setText(text);
    updateGeometry();
}

void ColourButton::setToolTip(const QString& text)
{
    QWidget::setToolTip(text);
    m_checkBox->setToolTip(text);
    m_swatch->setToolTip(text);
}

void ColourButton::pickColour()
{
    const QColor chosenColour
        = QColorDialog::getColor(colour(), this, tr("Select Colour"), QColorDialog::ShowAlphaChannel);
    if(chosenColour.isValid() && chosenColour != colour()) {
        setColour(chosenColour);
        m_changed = true;
    }
}

void ColourButton::updateButtonState()
{
    m_swatch->setEnabled(isChecked());
}
} // namespace Fooyin

#include "gui/widgets/moc_colourbutton.cpp"
