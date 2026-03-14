/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include "coverwidget.h"

#include <gui/configdialog.h>

class QCheckBox;
class QButtonGroup;

namespace Fooyin {
class SliderEditor;

class CoverWidgetConfigDialog : public WidgetConfigDialog<CoverWidget, CoverWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit CoverWidgetConfigDialog(CoverWidget* coverWidget, QWidget* parent = nullptr);

protected:
    void setConfig(const CoverWidget::ConfigData& config) override;
    [[nodiscard]] CoverWidget::ConfigData config() const override;

private:
    QButtonGroup* m_coverTypeGroup;
    QButtonGroup* m_alignmentGroup;
    QCheckBox* m_keepAspectRatio;

    QCheckBox* m_fadeEnabled;
    SliderEditor* m_fadeDuration;
};
} // namespace Fooyin
