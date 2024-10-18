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

#include "lyricsgeneralpage.h"

#include "lyricsconstants.h"
#include "lyricssettings.h"

#include <gui/guisettings.h>
#include <gui/widgets/slidereditor.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>

namespace Fooyin::Lyrics {
class LyricsGeneralPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit LyricsGeneralPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QLineEdit* m_lyricTags;
    QPlainTextEdit* m_lyricPaths;
    SliderEditor* m_scrollDuration;
};

LyricsGeneralPageWidget::LyricsGeneralPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_lyricTags{new QLineEdit(this)}
    , m_lyricPaths{new QPlainTextEdit(this)}
    , m_scrollDuration{new SliderEditor(tr("Synced lyric scroll"), this)}
{
    auto* scrollingGroup  = new QGroupBox(tr("Scrolling"), this);
    auto* scrollingLayout = new QGridLayout(scrollingGroup);

    m_scrollDuration->setRange(0, 2000);
    m_scrollDuration->setSuffix(QStringLiteral(" ms"));

    int row{0};
    scrollingLayout->addWidget(m_scrollDuration, row++, 0);

    auto* searchingGroup  = new QGroupBox(tr("Searching"), this);
    auto* searchingLayout = new QGridLayout(searchingGroup);

    auto* tagsLabel  = new QLabel(tr("Tags") + u":", this);
    auto* pathsLabel = new QLabel(tr("Local paths") + u":", this);

    row = 0;
    searchingLayout->addWidget(tagsLabel, row++, 0);
    searchingLayout->addWidget(m_lyricTags, row++, 0);
    searchingLayout->addWidget(pathsLabel, row++, 0);
    searchingLayout->addWidget(m_lyricPaths, row++, 0);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(scrollingGroup, row++, 0);
    layout->addWidget(searchingGroup, row++, 0);
    layout->setRowStretch(layout->rowCount(), 1);
}

void LyricsGeneralPageWidget::load()
{
    m_scrollDuration->setValue(m_settings->value<Settings::Lyrics::ScrollDuration>());
    m_lyricTags->setText(m_settings->value<Settings::Lyrics::SearchTags>().join(u';'));
    const auto paths = m_settings->value<Settings::Lyrics::Paths>();
    m_lyricPaths->setPlainText(paths.join(QStringLiteral("\n")));
}

void LyricsGeneralPageWidget::apply()
{
    m_settings->set<Settings::Lyrics::ScrollDuration>(m_scrollDuration->value());
    m_settings->set<Settings::Lyrics::SearchTags>(m_lyricTags->text().split(u';', Qt::SkipEmptyParts));
    m_settings->set<Settings::Lyrics::Paths>(m_lyricPaths->toPlainText().split(u'\n', Qt::SkipEmptyParts));
}

void LyricsGeneralPageWidget::reset()
{
    m_settings->reset<Settings::Lyrics::ScrollDuration>();
    m_settings->reset<Settings::Lyrics::SearchTags>();
    m_settings->reset<Settings::Lyrics::Paths>();
}

LyricsGeneralPage::LyricsGeneralPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::LyricsGeneral);
    setName(tr("General"));
    setCategory({tr("Widgets"), tr("Lyrics")});
    setWidgetCreator([settings] { return new LyricsGeneralPageWidget(settings); });
}
} // namespace Fooyin::Lyrics

#include "lyricsgeneralpage.moc"
#include "moc_lyricsgeneralpage.cpp"
