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

#include "searchpage.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/slidereditor.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>

namespace Fooyin {
class SearchPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit SearchPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QCheckBox* m_clearOnSuccess;
    SliderEditor* m_autosearchDelay;
    QLineEdit* m_playlistName;
    QCheckBox* m_appendSearchString;
    QCheckBox* m_focusOnSuccess;
};

SearchPageWidget::SearchPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_clearOnSuccess{new QCheckBox(tr("Clear search string when successful"), this)}
    , m_autosearchDelay{new SliderEditor(tr("Autosearch delay"), this)}
    , m_playlistName{new QLineEdit(this)}
    , m_appendSearchString{new QCheckBox(tr("Append search string to the playlist name"), this)}
    , m_focusOnSuccess{new QCheckBox(tr("Switch focus to playlist on successful search"), this)}
{
    auto* searchGroup       = new QGroupBox(tr("Search"), this);
    auto* searchGroupLayout = new QGridLayout(searchGroup);

    m_autosearchDelay->setRange(0, 3);
    m_autosearchDelay->addSpecialValue(0, tr("Very fast"));
    m_autosearchDelay->addSpecialValue(1, tr("Fast"));
    m_autosearchDelay->addSpecialValue(2, tr("Medium"));
    m_autosearchDelay->addSpecialValue(3, tr("Slow"));

    int row{0};
    searchGroupLayout->addWidget(m_clearOnSuccess, row++, 0);
    searchGroupLayout->addWidget(m_autosearchDelay, row++, 0);
    searchGroupLayout->setColumnStretch(1, 1);

    auto* resultsGroup       = new QGroupBox(tr("Search Results"), this);
    auto* resultsGroupLayout = new QGridLayout(resultsGroup);

    auto* playlistNameLabel = new QLabel(tr("Playlist name") + u":", this);

    row = 0;
    resultsGroupLayout->addWidget(playlistNameLabel, row, 0);
    resultsGroupLayout->addWidget(m_playlistName, row++, 1);
    resultsGroupLayout->addWidget(m_appendSearchString, row++, 0, 1, 2);
    resultsGroupLayout->addWidget(m_focusOnSuccess, row++, 0, 1, 2);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(searchGroup, row++, 0);
    layout->addWidget(resultsGroup, row++, 0);
    layout->setRowStretch(layout->rowCount(), 1);
}

void SearchPageWidget::load()
{
    m_clearOnSuccess->setChecked(m_settings->value<Settings::Gui::SearchSuccessClear>());
    m_autosearchDelay->setValue(m_settings->value<Settings::Gui::SearchAutoDelay>());

    m_playlistName->setText(m_settings->value<Settings::Gui::SearchPlaylistName>());
    m_appendSearchString->setChecked(m_settings->value<Settings::Gui::SearchPlaylistAppendSearch>());
    m_focusOnSuccess->setChecked(m_settings->value<Settings::Gui::SearchSuccessFocus>());
}

void SearchPageWidget::apply()
{
    m_settings->set<Settings::Gui::SearchSuccessClear>(m_clearOnSuccess->isChecked());
    m_settings->set<Settings::Gui::SearchAutoDelay>(m_autosearchDelay->value());
    m_settings->set<Settings::Gui::SearchPlaylistName>(m_playlistName->text());
    m_settings->set<Settings::Gui::SearchPlaylistAppendSearch>(m_appendSearchString->isChecked());
    m_settings->set<Settings::Gui::SearchSuccessFocus>(m_focusOnSuccess->isChecked());
}

void SearchPageWidget::reset()
{
    m_settings->reset<Settings::Gui::SearchSuccessClear>();
    m_settings->reset<Settings::Gui::SearchAutoDelay>();
    m_settings->reset<Settings::Gui::SearchPlaylistName>();
    m_settings->reset<Settings::Gui::SearchPlaylistAppendSearch>();
    m_settings->reset<Settings::Gui::SearchSuccessFocus>();
}

SearchPage::SearchPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::LibrarySearching);
    setName(tr("General"));
    setCategory({tr("Library"), tr("Searching")});
    setWidgetCreator([settings] { return new SearchPageWidget(settings); });
}
} // namespace Fooyin

#include "moc_searchpage.cpp"
#include "searchpage.moc"
