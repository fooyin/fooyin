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
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/slidereditor.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QStyle>

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
    void loadColours();

    SettingsManager* m_settings;

    QCheckBox* m_clearOnSuccess;
    SliderEditor* m_autosearchDelay;
    QLineEdit* m_playlistName;
    QCheckBox* m_appendSearchString;
    QCheckBox* m_focusOnSuccess;
    QCheckBox* m_closeOnSuccess;

    QCheckBox* m_failBg;
    ColourButton* m_failBgColour;
    QCheckBox* m_failFg;
    ColourButton* m_failFgColour;
};

SearchPageWidget::SearchPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_clearOnSuccess{new QCheckBox(tr("Clear search string when successful"), this)}
    , m_autosearchDelay{new SliderEditor(tr("Autosearch delay"), this)}
    , m_playlistName{new QLineEdit(this)}
    , m_appendSearchString{new QCheckBox(tr("Append search string to the playlist name"), this)}
    , m_focusOnSuccess{new QCheckBox(tr("Switch focus to playlist on successful search"), this)}
    , m_closeOnSuccess{new QCheckBox(tr("Close quick search when successful"), this)}
    , m_failBg{new QCheckBox(tr("Error background") + u":", this)}
    , m_failBgColour{new ColourButton(this)}
    , m_failFg{new QCheckBox(tr("Error foreground") + u":", this)}
    , m_failFgColour{new ColourButton(this)}
{
    auto* searchGroup       = new QGroupBox(tr("Search"), this);
    auto* searchGroupLayout = new QGridLayout(searchGroup);

    m_autosearchDelay->setRange(0, 3);
    m_autosearchDelay->addSpecialValue(0, tr("Very fast"));
    m_autosearchDelay->addSpecialValue(1, tr("Fast"));
    m_autosearchDelay->addSpecialValue(2, tr("Medium"));
    m_autosearchDelay->addSpecialValue(3, tr("Slow"));

    auto* successHint = new QLabel(u"🛈 " + tr("These settings will only apply if autosearch is disabled."), this);

    int row{0};
    searchGroupLayout->addWidget(m_clearOnSuccess, row++, 0, 1, 3);
    searchGroupLayout->addWidget(m_closeOnSuccess, row++, 0, 1, 3);
    searchGroupLayout->addWidget(successHint, row++, 0, 1, 3);
    searchGroupLayout->addWidget(m_autosearchDelay, row++, 0, 1, 3);
    searchGroupLayout->addWidget(m_failBg, row, 0);
    searchGroupLayout->addWidget(m_failBgColour, row++, 1, 1, 2);
    searchGroupLayout->addWidget(m_failFg, row, 0);
    searchGroupLayout->addWidget(m_failFgColour, row++, 1, 1, 2);
    searchGroupLayout->setColumnStretch(2, 1);

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

    QObject::connect(m_failBg, &QCheckBox::toggled, m_failBgColour, &QWidget::setEnabled);
    QObject::connect(m_failFg, &QCheckBox::toggled, m_failFgColour, &QWidget::setEnabled);
}

void SearchPageWidget::load()
{
    m_clearOnSuccess->setChecked(m_settings->value<Settings::Gui::SearchSuccessClear>());
    m_closeOnSuccess->setChecked(m_settings->value<Settings::Gui::SearchSuccessClose>());
    m_autosearchDelay->setValue(m_settings->value<Settings::Gui::SearchAutoDelay>());

    loadColours();

    m_playlistName->setText(m_settings->value<Settings::Gui::SearchPlaylistName>());
    m_appendSearchString->setChecked(m_settings->value<Settings::Gui::SearchPlaylistAppendSearch>());
    m_focusOnSuccess->setChecked(m_settings->value<Settings::Gui::SearchSuccessFocus>());
}

void SearchPageWidget::apply()
{
    m_settings->set<Settings::Gui::SearchSuccessClear>(m_clearOnSuccess->isChecked());
    m_settings->set<Settings::Gui::SearchSuccessClose>(m_closeOnSuccess->isChecked());
    m_settings->set<Settings::Gui::SearchAutoDelay>(m_autosearchDelay->value());
    m_settings->set<Settings::Gui::SearchPlaylistName>(m_playlistName->text());
    m_settings->set<Settings::Gui::SearchPlaylistAppendSearch>(m_appendSearchString->isChecked());
    m_settings->set<Settings::Gui::SearchSuccessFocus>(m_focusOnSuccess->isChecked());

    if(m_failBg->isChecked()) {
        m_settings->set<Settings::Gui::SearchErrorBg>(m_failBgColour->colour());
    }
    else {
        m_settings->reset<Settings::Gui::SearchErrorBg>();
    }
    if(m_failFg->isChecked()) {
        m_settings->set<Settings::Gui::SearchErrorFg>(m_failFgColour->colour());
    }
    else {
        m_settings->reset<Settings::Gui::SearchErrorFg>();
    }

    loadColours();
}

void SearchPageWidget::reset()
{
    m_settings->reset<Settings::Gui::SearchSuccessClear>();
    m_settings->reset<Settings::Gui::SearchSuccessClose>();
    m_settings->reset<Settings::Gui::SearchAutoDelay>();
    m_settings->reset<Settings::Gui::SearchPlaylistName>();
    m_settings->reset<Settings::Gui::SearchPlaylistAppendSearch>();
    m_settings->reset<Settings::Gui::SearchSuccessFocus>();

    m_settings->reset<Settings::Gui::SearchErrorBg>();
    m_settings->reset<Settings::Gui::SearchErrorFg>();
}

void SearchPageWidget::loadColours()
{
    const auto failBg = m_settings->value<Settings::Gui::SearchErrorBg>();
    m_failBg->setChecked(!failBg.isNull());
    m_failBgColour->setEnabled(m_failBg->isChecked());

    if(!failBg.isNull()) {
        m_failBgColour->setColour(failBg.value<QColor>());
    }
    else {
        m_failBgColour->setColour(m_playlistName->style()->standardPalette().color(QPalette::Base));
    }

    const auto failFg = m_settings->value<Settings::Gui::SearchErrorFg>();
    m_failFg->setChecked(!failFg.isNull());
    m_failFgColour->setEnabled(m_failFg->isChecked());

    if(!failFg.isNull()) {
        m_failFgColour->setColour(failFg.value<QColor>());
    }
    else {
        m_failFgColour->setColour(m_playlistName->style()->standardPalette().color(QPalette::Text));
    }
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
