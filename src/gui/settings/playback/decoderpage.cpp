/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "decodermodel.h"

#include <core/engine/audioloader.h>
#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QGridLayout>
#include <QLabel>
#include <QListView>

namespace {
template <typename T>
void applyChanges(std::vector<T> existing, std::vector<T> loaders,
                  const std::function<void(const QString&, int)>& changeIndexFunc,
                  const std::function<void(const QString&, bool)>& setEnabledFunc)
{
    const auto sortByName = [](const auto& a, const auto& b) {
        return a.name < b.name;
    };

    std::ranges::sort(existing, sortByName);
    std::ranges::sort(loaders, sortByName);

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
} // namespace

namespace Fooyin {
class DecoderPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit DecoderPageWidget(AudioLoader* audioLoader);

    void load() override;
    void apply() override;
    void reset() override;

private:
    AudioLoader* m_audioLoader;

    QListView* m_decoderList;
    DecoderModel* m_decoderModel;
    QListView* m_readerList;
    DecoderModel* m_readerModel;
};

DecoderPageWidget::DecoderPageWidget(AudioLoader* audioLoader)
    : m_audioLoader{audioLoader}
    , m_decoderList{new QListView(this)}
    , m_decoderModel{new DecoderModel(this)}
    , m_readerList{new QListView(this)}
    , m_readerModel{new DecoderModel(this)}
{
    auto setupModel = [](QAbstractItemView* view) {
        view->setDragDropMode(QAbstractItemView::InternalMove);
        view->setDefaultDropAction(Qt::MoveAction);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        view->setSelectionMode(QAbstractItemView::SingleSelection);
        view->setDragEnabled(true);
        view->setDropIndicatorShown(true);
    };

    auto* decoderLabel = new QLabel(tr("Decoders") + u":", this);
    auto* readerLabel  = new QLabel(tr("Tag readers") + u":", this);

    m_decoderList->setModel(m_decoderModel);
    m_readerList->setModel(m_readerModel);
    setupModel(m_decoderList);
    setupModel(m_readerList);

    auto* layout = new QGridLayout(this);
    layout->addWidget(decoderLabel, 0, 0);
    layout->addWidget(m_decoderList, 1, 0);
    layout->addWidget(readerLabel, 0, 1);
    layout->addWidget(m_readerList, 1, 1);

    layout->setRowStretch(1, 1);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
}

void DecoderPageWidget::load()
{
    m_decoderModel->setup(m_audioLoader->decoders());
    m_readerModel->setup(m_audioLoader->readers());
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
}

void DecoderPageWidget::reset()
{
    m_audioLoader->reset();
}

DecoderPage::DecoderPage(AudioLoader* audioLoader, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::Decoding);
    setName(tr("General"));
    setCategory({tr("Playback"), tr("Decoding")});
    setWidgetCreator([audioLoader] { return new DecoderPageWidget(audioLoader); });
}
} // namespace Fooyin

#include "decoderpage.moc"
#include "moc_decoderpage.cpp"
