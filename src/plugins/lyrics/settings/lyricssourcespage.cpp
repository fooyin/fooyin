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

#include "lyricssourcespage.h"

#include "lyricsconstants.h"
#include "lyricsfinder.h"
#include "lyricssettings.h"
#include "lyricssourcesmodel.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>

namespace Fooyin::Lyrics {
class LyricsSourcesPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit LyricsSourcesPageWidget(LyricsFinder* lyricsFinder, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    LyricsFinder* m_lyricsFinder;
    SettingsManager* m_settings;

    QListView* m_sourceList;
    LyricsSourcesModel* m_sourceModel;

    QLineEdit* m_lyricTags;
    QPlainTextEdit* m_lyricPaths;
};

LyricsSourcesPageWidget::LyricsSourcesPageWidget(LyricsFinder* lyricsFinder, SettingsManager* settings)
    : m_lyricsFinder{lyricsFinder}
    , m_settings{settings}
    , m_sourceList{new QListView(this)}
    , m_sourceModel{new LyricsSourcesModel(this)}
    , m_lyricTags{new QLineEdit(this)}
    , m_lyricPaths{new QPlainTextEdit(this)}
{
    m_sourceList->setDragDropMode(QAbstractItemView::InternalMove);
    m_sourceList->setDefaultDropAction(Qt::MoveAction);
    m_sourceList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sourceList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sourceList->setDragEnabled(true);
    m_sourceList->setDropIndicatorShown(true);

    m_sourceList->setModel(m_sourceModel);

    auto* layout = new QGridLayout(this);

    int row{0};
    layout->addWidget(new QLabel(tr("Lyric sources") + u":", this), row++, 0);
    layout->addWidget(m_sourceList, row++, 0);
    layout->addWidget(new QLabel(QStringLiteral("🛈 ") + tr("Lyrics will be searched for in the above order."), this),
                      row++, 0);
    layout->addWidget(new QLabel(tr("Metadata tags") + u":", this), row++, 0);
    layout->addWidget(m_lyricTags, row++, 0);
    layout->addWidget(new QLabel(tr("Local files") + u":", this), row++, 0);
    layout->addWidget(m_lyricPaths, row++, 0);
}

void LyricsSourcesPageWidget::load()
{
    m_sourceModel->setup(m_lyricsFinder->sources());

    m_lyricTags->setText(m_settings->value<Settings::Lyrics::SearchTags>().join(u';'));
    const auto paths = m_settings->value<Settings::Lyrics::Paths>();
    m_lyricPaths->setPlainText(paths.join(QStringLiteral("\n")));
}

void LyricsSourcesPageWidget::apply()
{
    auto existing = m_lyricsFinder->sources();
    auto sources  = m_sourceModel->sources();

    std::ranges::sort(existing, {}, &LyricSource::name);
    std::ranges::sort(sources, {}, &LyricSourceItem::name);

    const size_t minSize = std::min(sources.size(), existing.size());
    for(size_t i{0}; i < minSize; ++i) {
        const auto& modelSource    = sources[i];
        const auto& existingSource = existing[i];
        if(modelSource.name == existingSource->name()) {
            existingSource->setIndex(modelSource.index);
            existingSource->setEnabled(modelSource.enabled);
        }
    }

    m_lyricsFinder->sort();

    m_settings->set<Settings::Lyrics::SearchTags>(m_lyricTags->text().split(u';', Qt::SkipEmptyParts));
    m_settings->set<Settings::Lyrics::Paths>(m_lyricPaths->toPlainText().split(u'\n', Qt::SkipEmptyParts));

    load();
}

void LyricsSourcesPageWidget::reset()
{
    m_lyricsFinder->reset();

    m_settings->reset<Settings::Lyrics::SearchTags>();
    m_settings->reset<Settings::Lyrics::Paths>();

    load();
}

LyricsSourcesPage::LyricsSourcesPage(LyricsFinder* lyricsFinder, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::LyricsSources);
    setName(tr("Sources"));
    setCategory({tr("Lyrics")});
    setWidgetCreator([lyricsFinder, settings] { return new LyricsSourcesPageWidget(lyricsFinder, settings); });
}
} // namespace Fooyin::Lyrics

#include "lyricssourcespage.moc"
#include "moc_lyricssourcespage.cpp"
