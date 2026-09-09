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

#include "fyutils_export.h"

#include <QPainterPath>
#include <QWidget>

namespace Fooyin {
class FYUTILS_EXPORT HeartValue
{
public:
    enum class EditMode : uint8_t
    {
        Editable,
        ReadOnly
    };

    HeartValue();
    explicit HeartValue(bool loved);
    HeartValue(bool loved, int scale);

    [[nodiscard]] bool loved() const;
    [[nodiscard]] int scale() const;

    void setLoved(bool loved);
    void setScale(int scale);

    void paint(QPainter* painter, const QRect& rect, const QPalette& palette, EditMode mode,
               Qt::Alignment alignment = Qt::AlignLeft, bool selected = false) const;
    [[nodiscard]] QSize sizeHint() const;

    operator QVariant() const
    {
        return QVariant::fromValue(*this);
    }

private:
    QPainterPath m_heart;
    bool m_loved;
    int m_scale;
};

class FYUTILS_EXPORT HeartEditor : public QWidget
{
    Q_OBJECT

public:
    explicit HeartEditor(Qt::Alignment align, QWidget* parent = nullptr);

    [[nodiscard]] HeartValue loved() const;
    void setValue(const HeartValue& value);
    [[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
    void editingFinished();

protected:
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    HeartValue m_loved;
    bool m_originaLoved;
    Qt::Alignment m_align;
};
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::HeartValue)
