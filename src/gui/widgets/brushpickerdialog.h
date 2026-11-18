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

#include <gui/brushsource.h>

#include <QDialog>
#include <QPointer>

class QComboBox;
class QDialogButtonBox;
class QFormLayout;
class QGridLayout;
class QPushButton;
class QTabBar;

namespace Fooyin {
class BrushPreview;
class ColourSourcePanel;
class GradientEditor;

class BrushPickerDialog : public QDialog
{
    Q_OBJECT

public:
    BrushPickerDialog(const BrushSource& brush, const QPalette& palette, QPalette::ColorGroup group, bool gradients,
                      QWidget* parent = nullptr);

    [[nodiscard]] const BrushSource& value() const;

    void setColourPalette(const QPalette& palette, QPalette::ColorGroup group);
    void setDirectionControlVisible(bool visible);

Q_SIGNALS:
    void valueChanged(const Fooyin::BrushSource& brush);

private:
    void changeMode(int mode);
    void selectStop(int index);
    void updateGradient();
    void updateStops();
    void updateValue();

    QGridLayout* m_layout;
    BrushSource m_value;
    LinearGradientSource m_gradient;
    QPalette m_palette;
    QPalette::ColorGroup m_group;
    QTabBar* m_mode;
    BrushPreview* m_preview;
    ColourSourcePanel* m_colourPanel;
    QWidget* m_gradientPanel;
    GradientEditor* m_gradientEditor;
    QComboBox* m_direction;
    QFormLayout* m_directionLayout;
    QDialogButtonBox* m_buttons;
    std::vector<QPointer<QPushButton>> m_stopButtons;
    int m_selectedStop;
    bool m_hasGradient;
    bool m_updating;
};
} // namespace Fooyin
