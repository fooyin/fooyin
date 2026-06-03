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

#include "decoderpage.h"

#include "decoderdelegate.h"
#include "decodermodel.h"

#include <core/engine/audioloader.h>
#include <core/engine/inputplugin.h>
#include <core/internalcoresettings.h>
#include <core/plugins/plugin.h>
#include <core/plugins/plugininfo.h>
#include <core/plugins/pluginmanager.h>
#include <gui/guiconstants.h>
#include <gui/plugins/pluginsettingsprovider.h>
#include <pluginsettingsregistry.h>
#include <utils/settings/settingsmanager.h>

#include <QGridLayout>
#include <QLabel>
#include <QListView>
#include <QMenu>

using namespace Qt::StringLiterals;

namespace {
template <typename T>
void applyChanges(std::vector<T> existing, std::vector<T> loaders,
                  const std::function<void(const QString&, int)>& changeIndexFunc,
                  const std::function<void(const QString&, bool)>& setEnabledFunc)
{
    std::ranges::sort(existing, {}, &T::name);
    std::ranges::sort(loaders, {}, &T::name);

    const size_t minSize = std::min(loaders.size(), existing.size());
    for(size_t i{0}; i < minSize; ++i) {
        const auto& modelLoader    = loaders[i];
        const auto& existingLoader = existing[i];
        if(modelLoader.index != existingLoader.index) {
            changeIndexFunc(modelLoader.name, modelLoader.index);
        }
        if(modelLoader.enabled != existingLoader.enabled) {
            setEnabledFunc(modelLoader.name, modelLoader.enabled);
        }
    }
}

Fooyin::DecoderModel::SettingsHandlerMap inputSettingsHandlers(Fooyin::PluginManager& pluginManager,
                                                               Fooyin::PluginSettingsRegistry& registry)
{
    Fooyin::DecoderModel::SettingsHandlerMap handlers;

    for(const auto& [pluginId, pluginInfo] : pluginManager.allPluginInfo()) {
        auto* const inputPlugin = pluginInfo ? qobject_cast<Fooyin::InputPlugin*>(pluginInfo->root()) : nullptr;
        if(!inputPlugin) {
            continue;
        }

        auto* provider = registry.providerFor(pluginId);
        if(!provider) {
            continue;
        }

        handlers[inputPlugin->inputName()] = [provider](QWidget* parent) {
            provider->showSettings(parent);
        };
    }

    return handlers;
}

Fooyin::DecoderModel::SettingsHandlerMap
readerSettingsHandlers(const std::vector<Fooyin::AudioLoader::LoaderEntry<Fooyin::DecoderCreator>>& decoders,
                       Fooyin::DecoderModel::SettingsHandlerMap handlers)
{
    for(const auto& decoder : decoders) {
        handlers.erase(decoder.name);
    }

    return handlers;
}

} // namespace

namespace Fooyin {
class DecoderPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit DecoderPageWidget(AudioLoader* audioLoader, PluginManager* pluginManager,
                               PluginSettingsRegistry* pluginSettingsRegistry, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void showConfigureMenu(QListView* view, DecoderModel* model, const QPoint& pos);

    AudioLoader* m_audioLoader;
    PluginManager* m_pluginManager;
    PluginSettingsRegistry* m_pluginSettingsRegistry;
    SettingsManager* m_settings;

    QListView* m_decoderList;
    DecoderModel* m_decoderModel;
    DecoderDelegate* m_decoderDelegate;
    QListView* m_readerList;
    DecoderModel* m_readerModel;
    DecoderDelegate* m_readerDelegate;
};

DecoderPageWidget::DecoderPageWidget(AudioLoader* audioLoader, PluginManager* pluginManager,
                                     PluginSettingsRegistry* pluginSettingsRegistry, SettingsManager* settings)
    : m_audioLoader{audioLoader}
    , m_pluginManager{pluginManager}
    , m_pluginSettingsRegistry{pluginSettingsRegistry}
    , m_settings{settings}
    , m_decoderList{new QListView(this)}
    , m_decoderModel{new DecoderModel(this)}
    , m_decoderDelegate{new DecoderDelegate(m_decoderList, this)}
    , m_readerList{new QListView(this)}
    , m_readerModel{new DecoderModel(this)}
    , m_readerDelegate{new DecoderDelegate(m_readerList, this)}
{
    auto setupModel = [](QAbstractItemView* view) {
        view->setDragDropMode(QAbstractItemView::InternalMove);
        view->setDefaultDropAction(Qt::MoveAction);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        view->setSelectionMode(QAbstractItemView::SingleSelection);
        view->setDragEnabled(true);
        view->setDropIndicatorShown(true);
        view->setMouseTracking(true);
    };

    m_decoderList->setModel(m_decoderModel);
    m_decoderList->setItemDelegate(m_decoderDelegate);
    m_readerList->setModel(m_readerModel);
    m_readerList->setItemDelegate(m_readerDelegate);
    setupModel(m_decoderList);
    setupModel(m_readerList);

    m_decoderList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_readerList->setContextMenuPolicy(Qt::CustomContextMenu);

    int row{0};
    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(new QLabel(tr("Decoders") + ":"_L1, this), row, 0);
    layout->addWidget(new QLabel(tr("Tag readers") + ":"_L1, this), row++, 1);
    layout->addWidget(m_decoderList, row, 0);
    layout->addWidget(m_readerList, row, 1);
    layout->setRowStretch(row++, 1);

    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);

    QObject::connect(m_decoderDelegate, &DecoderDelegate::configureClicked, this,
                     [this](const QModelIndex& index) { m_decoderModel->showSettings(index, this); });
    QObject::connect(m_readerDelegate, &DecoderDelegate::configureClicked, this,
                     [this](const QModelIndex& index) { m_readerModel->showSettings(index, this); });
    QObject::connect(m_decoderList, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showConfigureMenu(m_decoderList, m_decoderModel, pos); });
    QObject::connect(m_readerList, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) { showConfigureMenu(m_readerList, m_readerModel, pos); });
}

void DecoderPageWidget::load()
{
    const auto decoderHandlers = inputSettingsHandlers(*m_pluginManager, *m_pluginSettingsRegistry);
    const auto decoders        = m_audioLoader->decoders();

    m_decoderModel->setup(decoders, decoderHandlers);
    m_readerModel->setup(m_audioLoader->readers(), readerSettingsHandlers(decoders, decoderHandlers));
}

void DecoderPageWidget::apply()
{
    applyChanges(
        m_audioLoader->decoders(), std::get<0>(m_decoderModel->loaders()),
        [this](const QString& name, int index) { m_audioLoader->changeDecoderIndex(name, index); },
        [this](const QString& name, bool enabled) { m_audioLoader->setDecoderEnabled(name, enabled); });

    applyChanges(
        m_audioLoader->readers(), std::get<1>(m_readerModel->loaders()),
        [this](const QString& name, int index) { m_audioLoader->changeReaderIndex(name, index); },
        [this](const QString& name, bool enabled) { m_audioLoader->setReaderEnabled(name, enabled); });

    load();
}

void DecoderPageWidget::reset()
{
    m_audioLoader->reset();
}

void DecoderPageWidget::showConfigureMenu(QListView* view, DecoderModel* model, const QPoint& pos)
{
    const QModelIndex index = view->indexAt(pos);
    if(!index.isValid() || !index.data(DecoderModel::HasSettings).toBool()) {
        return;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* configureAction = new QAction(tr("Configure…"), menu);
    menu->addAction(configureAction);
    QObject::connect(configureAction, &QAction::triggered, this,
                     [this, model, index]() { model->showSettings(index, this); });

    menu->popup(view->viewport()->mapToGlobal(pos));
}

DecoderPage::DecoderPage(AudioLoader* audioLoader, PluginManager* pluginManager,
                         PluginSettingsRegistry* pluginSettingsRegistry, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::Decoding);
    setName(tr("General"));
    setCategory({tr("Playback"), tr("Decoding")});
    setWidgetCreator([audioLoader, pluginManager, pluginSettingsRegistry, settings] {
        return new DecoderPageWidget(audioLoader, pluginManager, pluginSettingsRegistry, settings);
    });
}
} // namespace Fooyin

#include "decoderpage.moc"
#include "moc_decoderpage.cpp"
