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

#include "fygui_export.h"

#include <gui/widgets/backgroundconfig.h>

#include <QWidget>

namespace Fooyin {
class BackgroundConfigWidgetPrivate;

class FYGUI_EXPORT BackgroundConfigWidget : public QWidget
{
    Q_OBJECT

public:
    enum Option : uint8_t
    {
        None            = 0,
        TrackPreference = 1 << 0,
        FadeDuration    = 1 << 1,
        NoGlobalSource  = 1 << 2,
    };
    Q_DECLARE_FLAGS(Options, Option)

    explicit BackgroundConfigWidget(QWidget* parent = nullptr);
    ~BackgroundConfigWidget() override;

    [[nodiscard]] BackgroundConfig config() const;
    void setConfig(const BackgroundConfig& config);

    void setOptions(Options options);

Q_SIGNALS:
    void configChanged();

private:
    std::unique_ptr<BackgroundConfigWidgetPrivate> p;
};
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::BackgroundConfigWidget::Options)
