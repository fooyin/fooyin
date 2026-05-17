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

#include <core/player/playerdefs.h>
#include <core/scripting/scriptparser.h>

#include <QObject>

namespace Fooyin {
class PlayerController;
class SettingsManager;

class NowPlayingOutputService : public QObject
{
    Q_OBJECT

public:
    enum UpdateEvent : uint8_t
    {
        UpdateNone      = 0,
        UpdateTrack     = 1 << 0,
        UpdatePlayPause = 1 << 1,
        UpdateStop      = 1 << 2,
        UpdateSecond    = 1 << 3,
        DefaultEvents   = UpdateTrack | UpdatePlayPause | UpdateStop
    };
    Q_DECLARE_FLAGS(UpdateEvents, UpdateEvent)
    Q_FLAG(UpdateEvents)

    enum OutputTarget : uint8_t
    {
        NoOutput        = 0,
        OutputClipboard = 1 << 0,
        OutputFile      = 1 << 1,
    };
    Q_DECLARE_FLAGS(OutputTargets, OutputTarget)
    Q_FLAG(OutputTargets)

    enum OutputOption : uint8_t
    {
        NoOptions            = 0,
        OutputAppendFile     = 1 << 0,
        OutputDefaultOptions = NoOptions
    };
    Q_DECLARE_FLAGS(OutputOptions, OutputOption)
    Q_FLAG(OutputOptions)

    NowPlayingOutputService(PlayerController* playerController, SettingsManager* settings, QObject* parent = nullptr);
    ~NowPlayingOutputService() override;

private:
    void update();
    [[nodiscard]] QString formattedOutput();
    void writeOutput(const QString& output);
    void writeClipboard(const QString& output) const;
    void writeFile(const QString& output) const;

    PlayerController* m_playerController;
    SettingsManager* m_settings;

    bool m_enabled;
    UpdateEvents m_events;
    OutputTargets m_targets;
    OutputOptions m_options;

    ScriptParser m_scriptParser;
    QString m_filepath;
    QString m_script;
    QString m_lastOutput;
};
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::NowPlayingOutputService::UpdateEvents)
Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::NowPlayingOutputService::OutputTargets)
Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::NowPlayingOutputService::OutputOptions)
