#pragma once

#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <gui/plugins/guiplugin.h>
#include <gui/windowcontroller.h>

#include <QPointer>
#include <QAbstractNativeEventFilter>
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

    PlayerController* m_playerController = nullptr;
    PlaylistHandler* m_playlistHandler = nullptr;
    QPointer<WindowController> m_windowController = nullptr;

    Microsoft::WRL::ComPtr<ITaskbarList3> m_taskbarList;

    class ThumbnailToolbarNativeEventFilter : public QAbstractNativeEventFilter {
    public:
        ThumbnailToolbarNativeEventFilter(ThumbnailToolbarPlugin* plugin) : m_plugin(plugin) {}

        bool nativeEventFilter(const QByteArray& eventType, void* message, [[maybe_unused]] qintptr* result) override {
            if (eventType == "windows_generic_MSG") {
                MSG* msg = static_cast<MSG*>(message);
                if (msg && msg->message == WM_COMMAND) {
                    if (HIWORD(msg->wParam) == THBN_CLICKED) {
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
