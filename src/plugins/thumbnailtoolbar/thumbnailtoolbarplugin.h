/*
 * Fooyin
 * Copyright © 2025, Carter Li <zhangsongcui@live.cn>
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

#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <gui/plugins/guiplugin.h>
#include <gui/windowcontroller.h>

#include <QAbstractNativeEventFilter>
#include <QPointer>
#include <ShObjIdl.h>
#include <wrl/client.h>

class QMainWindow;

namespace Fooyin {
struct PlaylistTrack;

namespace ThumbnailToolbar {
class ThumbnailToolbarPlugin : public QObject,
                               public Plugin,
                               public CorePlugin,
                               public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "thumbnailtoolbar.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

public:
    ThumbnailToolbarPlugin();
    ~ThumbnailToolbarPlugin() override;

    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;

    void shutdown() override;

private:
    void trackChanged(const PlaylistTrack& playlistTrack);
    void playStateChanged();
    void updateToolbarButtons();
    void setupToolbar();
    void cleanupToobar();
    void updatePosition();
    void handleThumbnailClick(int buttonId);

    PlayerController* m_playerController          = nullptr;
    QPointer<WindowController> m_windowController = nullptr;

    Microsoft::WRL::ComPtr<ITaskbarList3> m_taskbarList;

    class ThumbnailToolbarNativeEventFilter : public QAbstractNativeEventFilter
    {
    public:
        ThumbnailToolbarNativeEventFilter(ThumbnailToolbarPlugin* plugin)
            : m_plugin(plugin)
        { }

        bool nativeEventFilter(const QByteArray& eventType, void* message, [[maybe_unused]] qintptr* result) override
        {
            if(eventType == "windows_generic_MSG") {
                MSG* msg = static_cast<MSG*>(message);
                if(msg && msg->message == WM_COMMAND) {
                    if(HIWORD(msg->wParam) == THBN_CLICKED) {
                        m_plugin->handleThumbnailClick(LOWORD(msg->wParam));
                        return true;
                    }
                }
            }
            return false;
        }

    private:
        ThumbnailToolbarPlugin* m_plugin;
    };

    std::unique_ptr<ThumbnailToolbarNativeEventFilter> m_nativeEventFilter;
    bool m_toolbarReady = false;
};

} // namespace ThumbnailToolbar

} // namespace Fooyin
