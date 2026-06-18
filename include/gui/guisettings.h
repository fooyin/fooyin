/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <utils/settings/settingsentry.h>

#include <QFont>
#include <QMap>
#include <QObject>
#include <QPalette>

namespace Fooyin {
struct ResolvedAppStyle
{
    QPalette palette;
    QFont defaultFont;
    QMap<QString, QFont> classFonts;
    quint64 revision{0};

    [[nodiscard]] QFont font(const QString& className = {}) const
    {
        return className.isEmpty() ? defaultFont : classFonts.value(className, defaultFont);
    }
};

namespace Settings::Gui {
Q_NAMESPACE_EXPORT(FYGUI_EXPORT)

enum ToolButtonOption : uint8_t
{
    None    = 0,
    Raise   = 1 << 0,
    Stretch = 1 << 1,
};
Q_DECLARE_FLAGS(ToolButtonOptions, ToolButtonOption)

enum GuiSettings : uint32_t
{
    LayoutEditing              = 1 | Type::Bool,
    StartupBehaviour           = 2 | Type::Int,
    WaitForTracks              = 3 | Type::Bool,
    IconTheme                  = 4 | Type::Int,
    ResolvedAppStyle           = 5 | Type::Variant,
    CursorFollowsPlayback      = 6 | Type::Bool,
    PlaybackFollowsCursor      = 7 | Type::Bool,
    ToolButtonStyle            = 8 | Type::Int,
    MainWindowPixelRatio       = 9 | Type::Double,
    StarRatingSize             = 10 | Type::Int,
    Style                      = 11 | Type::String,
    SeekStepSmall              = 12 | Type::Int,
    SeekStepLarge              = 13 | Type::Int,
    ShowStatusTips             = 14 | Type::Bool,
    CustomTheme                = 15 | Type::Variant,
    ShowSplitterHandles        = 16 | Type::Bool,
    LockSplitterHandles        = 17 | Type::Bool,
    SplitterHandleSize         = 18 | Type::Int,
    VolumeStep                 = 19 | Type::Double,
    SearchSuccessClear         = 20 | Type::Bool,
    SearchAutoDelay            = 21 | Type::Int,
    SearchPlaylistName         = 22 | Type::String,
    SearchPlaylistAppendSearch = 23 | Type::Bool,
    SearchSuccessFocus         = 24 | Type::Bool,
    SearchErrorBg              = 25 | Type::Variant,
    SearchErrorFg              = 26 | Type::Variant,
    SearchSuccessClose         = 27 | Type::Bool,
    ShowMenuBar                = 28 | Type::Bool,
    RatingFullStarSymbol       = 29 | Type::String,
    RatingHalfStarSymbol       = 30 | Type::String,
    RatingEmptyStarSymbol      = 31 | Type::String,
    SeekBarMouseFocus          = 32 | Type::Bool,
    PlaylistIntegratedSearch   = 33 | Type::Bool,
    PlaylistSearchMode         = 34 | Type::Int,
    PlaylistSearchScript       = 35 | Type::String,
    DragOnlyAfterSelect        = 36 | Type::Bool,
};
Q_ENUM_NS(GuiSettings)
} // namespace Settings::Gui
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::ResolvedAppStyle)