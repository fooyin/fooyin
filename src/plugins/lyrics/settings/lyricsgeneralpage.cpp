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
#include <gui/widgets/scriptlineedit.h>
#include <gui/widgets/slidereditor.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>

using namespace Qt::StringLiterals;

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
    [[nodiscard]] ScrollMode getScrollMode() const;
    void setScrollMode(ScrollMode mode);

    SettingsManager* m_settings;

    QCheckBox* m_seekOnClick;
    ScriptLineEdit* m_noLyricsScript;

    SliderEditor* m_scrollDuration;
    QRadioButton* m_scrollManual;
    QRadioButton* m_scrollSynced;
    QRadioButton* m_scrollAutomatic;
};

LyricsGeneralPageWidget::LyricsGeneralPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_seekOnClick{new QCheckBox(tr("Seek on click"), this)}
    , m_noLyricsScript{new ScriptLineEdit(this)}
    , m_scrollDuration{new SliderEditor(tr("Synced scroll duration"), this)}
    , m_scrollManual{new QRadioButton(tr("Manual"), this)}
    , m_scrollSynced{new QRadioButton(tr("Synced"), this)}
    , m_scrollAutomatic{new QRadioButton(tr("Automatic"), this)}
{
    auto* generalGroup  = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalGroup);

    auto* seekHint = new QLabel(u"🛈 "_s + tr("This will only function with synced lyrics."), this);
    auto* noLyricsHint
        = new QLabel(u"🛈 "_s + tr("This will be displayed if lyrics for the current track can't be found."), this);

    int row{0};
    generalLayout->addWidget(m_seekOnClick, row++, 0, 1, 2);
    generalLayout->addWidget(seekHint, row++, 0, 1, 2);
    generalLayout->addWidget(new QLabel(tr("No lyrics script") + ":"_L1, this), row, 0);
    generalLayout->addWidget(m_noLyricsScript, row++, 1);
    generalLayout->addWidget(noLyricsHint, row++, 0, 1, 2);

    auto* scrollingGroup  = new QGroupBox(tr("Scrolling"), this);
    auto* scrollingLayout = new QGridLayout(scrollingGroup);

    m_scrollDuration->setRange(0, 2000);
    m_scrollDuration->setSuffix(u" ms"_s);

    auto* scrollModeGroup  = new QGroupBox(tr("Scroll Mode"), this);
    auto* scrollModeLayout = new QGridLayout(scrollModeGroup);

    m_scrollManual->setToolTip(tr("No automatic scrolling will take place"));
    m_scrollSynced->setToolTip(tr("Synced lyrics will be scrolled"));
    m_scrollAutomatic->setToolTip(tr("All lyrics will be scrolled"));

    row = 0;
    scrollModeLayout->addWidget(m_scrollManual, row++, 0);
    scrollModeLayout->addWidget(m_scrollSynced, row++, 0);
    scrollModeLayout->addWidget(m_scrollAutomatic, row++, 0);

    row = 0;
    scrollingLayout->addWidget(m_scrollDuration, row++, 0);
    scrollingLayout->addWidget(scrollModeGroup, row++, 0);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(generalGroup, row++, 0);
    layout->addWidget(scrollingGroup, row++, 0);
    layout->setRowStretch(layout->rowCount(), 1);
}

void LyricsGeneralPageWidget::load()
{
    m_seekOnClick->setChecked(m_settings->value<Settings::Lyrics::SeekOnClick>());
    m_scrollDuration->setValue(m_settings->value<Settings::Lyrics::ScrollDuration>());
    setScrollMode(static_cast<ScrollMode>(m_settings->value<Settings::Lyrics::ScrollMode>()));
    m_noLyricsScript->setText(m_settings->value<Settings::Lyrics::NoLyricsScript>());
}

void LyricsGeneralPageWidget::apply()
{
    m_settings->set<Settings::Lyrics::SeekOnClick>(m_seekOnClick->isChecked());
    m_settings->set<Settings::Lyrics::ScrollDuration>(m_scrollDuration->value());
    m_settings->set<Settings::Lyrics::ScrollMode>(static_cast<int>(getScrollMode()));
    m_settings->set<Settings::Lyrics::NoLyricsScript>(m_noLyricsScript->text());
}

void LyricsGeneralPageWidget::reset()
{
    m_settings->reset<Settings::Lyrics::SeekOnClick>();
    m_settings->reset<Settings::Lyrics::ScrollDuration>();
    m_settings->reset<Settings::Lyrics::ScrollMode>();
    m_settings->reset<Settings::Lyrics::NoLyricsScript>();
}

ScrollMode LyricsGeneralPageWidget::getScrollMode() const
{
    if(m_scrollManual->isChecked()) {
        return ScrollMode::Manual;
    }
    if(m_scrollSynced->isChecked()) {
        return ScrollMode::Synced;
    }
    return ScrollMode::Automatic;
}

void LyricsGeneralPageWidget::setScrollMode(ScrollMode mode)
{
    m_scrollManual->setChecked(mode == ScrollMode::Manual);
    m_scrollSynced->setChecked(mode == ScrollMode::Synced);
    m_scrollAutomatic->setChecked(mode == ScrollMode::Automatic);
}

LyricsGeneralPage::LyricsGeneralPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::LyricsGeneral);
    setName(tr("General"));
    setCategory({tr("Lyrics")});
    setWidgetCreator([settings] { return new LyricsGeneralPageWidget(settings); });
}
} // namespace Fooyin::Lyrics

#include "lyricsgeneralpage.moc"
#include "moc_lyricsgeneralpage.cpp"
