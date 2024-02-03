/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "generalpage.h"

#include "mainwindow.h"

#include <core/coresettings.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

#include <ranges>

constexpr auto SystemLanguage = "Use System Default";

using namespace Qt::Literals::StringLiterals;

namespace {
struct SortLanguages
{
    bool operator()(const QString& lhs, const QString& rhs) const
    {
        return lhs.localeAwareCompare(rhs) < 0;
    }
};
} // namespace

namespace Fooyin {
class GeneralPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit GeneralPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void loadLanguage();

    SettingsManager* m_settings;

    QComboBox* m_startupBehaviour;
    QCheckBox* m_waitForTracks;

    QComboBox* m_language;
    std::map<QString, QString, SortLanguages> m_languageMap;
};

GeneralPageWidget::GeneralPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_startupBehaviour{new QComboBox(this)}
    , m_waitForTracks{new QCheckBox(tr("Wait for tracks"), this)}
    , m_language{new QComboBox(this)}
{
    auto* startupBehaviourLabel = new QLabel(tr("Behaviour: "), this);

    m_waitForTracks->setToolTip(tr("Delay opening fooyin until all tracks have been loaded"));
    m_waitForTracks->setChecked(m_settings->value<Settings::Gui::WaitForTracks>());

    auto* startupGroup       = new QGroupBox(tr("Startup"), this);
    auto* startupGroupLayout = new QGridLayout(startupGroup);

    startupGroupLayout->addWidget(startupBehaviourLabel, 0, 0);
    startupGroupLayout->addWidget(m_startupBehaviour, 0, 1);
    startupGroupLayout->addWidget(m_waitForTracks, 1, 0, 1, 2);
    startupGroupLayout->setColumnStretch(1, 1);

    auto* languageLabel = new QLabel(tr("Language") + u":"_s, this);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(languageLabel, 0, 0);
    mainLayout->addWidget(m_language, 0, 1);
    mainLayout->addWidget(startupGroup, 1, 0, 1, 2);

    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    auto addStartupBehaviour = [this](const QString& text, MainWindow::StartupBehaviour action) {
        m_startupBehaviour->addItem(text, QVariant::fromValue(action));
    };

    addStartupBehaviour(tr("Show main window"), MainWindow::Normal);
    addStartupBehaviour(tr("Show main window maximised"), MainWindow::Maximised);
    addStartupBehaviour(tr("Remember from last run"), MainWindow::RememberLast);
}

void GeneralPageWidget::load()
{
    m_startupBehaviour->setCurrentIndex(m_settings->value<Settings::Gui::StartupBehaviour>());
    loadLanguage();
}

void GeneralPageWidget::apply()
{
    m_settings->set<Settings::Gui::StartupBehaviour>(m_startupBehaviour->currentIndex());
    m_settings->set<Settings::Gui::WaitForTracks>(m_waitForTracks->isChecked());

    const QString currentLanguage = m_language->currentText();
    m_settings->set<Settings::Core::Language>(
        m_languageMap.contains(currentLanguage) ? m_languageMap.at(currentLanguage) : QString{});
}

void GeneralPageWidget::reset()
{
    m_settings->reset<Settings::Gui::StartupBehaviour>();
    m_settings->reset<Settings::Gui::WaitForTracks>();
    m_settings->reset<Settings::Core::Language>();
}

void GeneralPageWidget::loadLanguage()
{
    m_languageMap.clear();
    m_languageMap["English (en_GB)"] = "en_GB";

    QDir translationDir{u"://translations"_s};
    QStringList translations = translationDir.entryList(QStringList{} << u"*.qm"_s);
    static QRegularExpression translationExpr(u"^fooyin_(.*).qm$"_s);

    for(const QString& translation : translations) {
        QRegularExpressionMatch translationMatch = translationExpr.match(translation);
        if(!translationMatch.hasMatch()) {
            continue;
        }

        const QString code       = translationMatch.captured(1);
        QString language         = QLocale::languageToString(QLocale(code).language());
        const QString nativeName = QLocale(code).nativeLanguageName();
        if(!nativeName.isEmpty()) {
            language = nativeName;
        }

        const auto name     = QString{"%1 (%2)"}.arg(language, code);
        m_languageMap[name] = code;
    }

    m_language->addItem(SystemLanguage);

    for(const QString& language : m_languageMap | std::views::keys) {
        m_language->addItem(language);
    }

    const QString currentLang = m_settings->value<Settings::Core::Language>();

    auto langIt = std::ranges::find_if(std::as_const(m_languageMap),
                                       [currentLang](const auto& lang) { return lang.second == currentLang; });
    if(langIt == m_languageMap.cend()) {
        m_language->setCurrentIndex(0);
    }
    else {
        m_language->setCurrentIndex(m_language->findText(langIt->first));
    }
}

GeneralPage::GeneralPage(SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::GeneralCore);
    setName(tr("General"));
    setCategory({tr("General")});
    setWidgetCreator([settings] { return new GeneralPageWidget(settings); });
}

} // namespace Fooyin

#include "generalpage.moc"
#include "moc_generalpage.cpp"
