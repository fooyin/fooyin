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

#include "internalguisettings.h"
#include "mainwindow.h"

#include <core/application.h>
#include <core/corepaths.h>
#include <core/coresettings.h>
#include <core/internalcoresettings.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/paths.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSystemTrayIcon>

#include <ranges>

using namespace Qt::StringLiterals;

constexpr auto SystemLanguage = "Use System Default";

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

    QCheckBox* m_showTray;
    QCheckBox* m_minimiseToTray;

    QComboBox* m_language;
    std::map<QString, QString, SortLanguages> m_languageMap;
};

GeneralPageWidget::GeneralPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_startupBehaviour{new QComboBox(this)}
    , m_waitForTracks{new QCheckBox(tr("Wait for tracks"), this)}
    , m_showTray{new QCheckBox(tr("Show system tray icon"), this)}
    , m_minimiseToTray{new QCheckBox(tr("Minimise to tray on close"), this)}
    , m_language{new QComboBox(this)}
{
    m_waitForTracks->setToolTip(tr("Delay opening fooyin until all tracks have been loaded"));

    auto* startupGroup       = new QGroupBox(tr("Startup"), this);
    auto* startupGroupLayout = new QGridLayout(startupGroup);

    int row{0};
    startupGroupLayout->addWidget(new QLabel(tr("Behaviour") + u":"_s, this), row, 0);
    startupGroupLayout->addWidget(m_startupBehaviour, row++, 1);
    startupGroupLayout->addWidget(m_waitForTracks, row++, 0, 1, 2);
    startupGroupLayout->setColumnStretch(1, 1);

    auto* dirGroup       = new QGroupBox(tr("User Directories"), this);
    auto* dirGroupLayout = new QGridLayout(dirGroup);

    auto* openConfig = new QPushButton(tr("Open Config Directory"), this);
    auto* openShare  = new QPushButton(tr("Open Share Directory"), this);

    row = 0;
    dirGroupLayout->addWidget(openConfig, row, 0);
    dirGroupLayout->addWidget(openShare, row++, 1);

    auto* mainLayout = new QGridLayout(this);

    row = 0;
    mainLayout->addWidget(new QLabel(tr("Language") + u":"_s, this), 0, 0);
    mainLayout->addWidget(m_language, row++, 1);
    mainLayout->addWidget(m_showTray, row++, 0, 1, 2);
    mainLayout->addWidget(m_minimiseToTray, row++, 0, 1, 2);
    mainLayout->addWidget(startupGroup, row++, 0, 1, 2);
    mainLayout->addWidget(dirGroup, row++, 0, 1, 2);

    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    auto addStartupBehaviour = [this](const QString& text, MainWindow::StartupBehaviour action) {
        m_startupBehaviour->addItem(text, QVariant::fromValue(action));
    };

    addStartupBehaviour(tr("Normal"), MainWindow::StartNormal);
    addStartupBehaviour(tr("Start maximised"), MainWindow::StartMaximised);
    addStartupBehaviour(tr("Start hidden to tray"), MainWindow::StartHidden);
    addStartupBehaviour(tr("Remember from last run"), MainWindow::StartPrev);

    QObject::connect(m_showTray, &QCheckBox::toggled, m_minimiseToTray, &QWidget::setEnabled);
    QObject::connect(openConfig, &QPushButton::clicked, this, []() { QDesktopServices::openUrl(Utils::configPath()); });
    QObject::connect(openShare, &QPushButton::clicked, this, []() { QDesktopServices::openUrl(Utils::sharePath()); });
}

void GeneralPageWidget::load()
{
    loadLanguage();

    m_startupBehaviour->setCurrentIndex(m_settings->value<Settings::Gui::StartupBehaviour>());
    m_waitForTracks->setChecked(m_settings->value<Settings::Gui::WaitForTracks>());
    m_showTray->setChecked(m_settings->value<Settings::Gui::Internal::ShowTrayIcon>());
    m_minimiseToTray->setChecked(m_settings->value<Settings::Gui::Internal::TrayOnClose>());

    m_showTray->setEnabled(QSystemTrayIcon::isSystemTrayAvailable());
    m_minimiseToTray->setEnabled(QSystemTrayIcon::isSystemTrayAvailable() && m_showTray->isChecked());
}

void GeneralPageWidget::apply()
{
    const QString currentLanguage = m_language->currentText();
    const auto chosenLanguage = m_languageMap.contains(currentLanguage) ? m_languageMap.at(currentLanguage) : QString{};

    if(chosenLanguage != m_settings->value<Settings::Core::Language>()) {
        m_settings->set<Settings::Core::Language>(chosenLanguage);
        QMessageBox msg{QMessageBox::Question, tr("Language changed"),
                        tr("Restart for changes to take effect. Restart now?"), QMessageBox::Yes | QMessageBox::No};
        if(msg.exec() == QMessageBox::Yes) {
            m_settings->storeSettings();
            Application::restart();
        }
    }

    m_settings->set<Settings::Gui::StartupBehaviour>(m_startupBehaviour->currentIndex());
    m_settings->set<Settings::Gui::WaitForTracks>(m_waitForTracks->isChecked());
    m_settings->set<Settings::Gui::Internal::ShowTrayIcon>(m_showTray->isChecked());
    m_settings->set<Settings::Gui::Internal::TrayOnClose>(m_minimiseToTray->isChecked());
}

void GeneralPageWidget::reset()
{
    m_settings->reset<Settings::Core::Language>();
    m_settings->reset<Settings::Gui::StartupBehaviour>();
    m_settings->reset<Settings::Gui::WaitForTracks>();
    m_settings->reset<Settings::Gui::Internal::ShowTrayIcon>();
    m_settings->reset<Settings::Gui::Internal::TrayOnClose>();
}

void GeneralPageWidget::loadLanguage()
{
    m_languageMap.clear();

    const QDir translationDir{Core::translationsPath()};
    const QStringList translations = translationDir.entryList({u"*.qm"_s});
    static const QRegularExpression translationExpr{uR"(^fooyin_(?!en\.qm$)(.*)\.qm$)"_s};

    for(const QString& translation : translations) {
        const QRegularExpressionMatch translationMatch = translationExpr.match(translation);
        if(!translationMatch.hasMatch()) {
            continue;
        }

        const QString code       = translationMatch.captured(1);
        QString language         = QLocale::languageToString(QLocale(code).language());
        const QString nativeName = QLocale(code).nativeLanguageName();
        if(!nativeName.isEmpty()) {
            language = nativeName;
        }

        const auto name     = u"%1 (%2)"_s.arg(language, code);
        m_languageMap[name] = code;
    }

    m_language->addItem(QString::fromLatin1(SystemLanguage));

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

GeneralPage::GeneralPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::GeneralCore);
    setName(tr("General"));
    setCategory({tr("General")});
    setWidgetCreator([settings] { return new GeneralPageWidget(settings); });
}

} // namespace Fooyin

#include "generalpage.moc"
#include "moc_generalpage.cpp"
