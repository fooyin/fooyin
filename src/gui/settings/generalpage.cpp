/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "gui/guiconstants.h"
#include "gui/guisettings.h"
#include "gui/mainwindow.h"

#include <utils/settings/settingsmanager.h>

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>

namespace Fy::Gui::Settings {
class GeneralPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit GeneralPageWidget(Utils::SettingsManager* settings);

    void apply() override;

private:
    Utils::SettingsManager* m_settings;

    QComboBox* m_startupBehaviour;
};

GeneralPageWidget::GeneralPageWidget(Utils::SettingsManager* settings)
    : m_settings{settings}
    , m_startupBehaviour{new QComboBox(this)}
{
    auto* startupBehaviourLabel = new QLabel("Startup behaviour: ", this);

    auto* mainLayout = new QGridLayout(this);

    mainLayout->addWidget(startupBehaviourLabel, 0, 0);
    mainLayout->addWidget(m_startupBehaviour, 0, 1);
    mainLayout->setColumnStretch(2, 1);
    mainLayout->setRowStretch(1, 1);

    auto addStartupBehaviour = [this](const QString& text, MainWindow::StartupBehaviour action) {
        m_startupBehaviour->addItem(text, QVariant::fromValue(action));
    };

    addStartupBehaviour("Show main window", MainWindow::Normal);
    addStartupBehaviour("Show main window maximised", MainWindow::Maximised);
    addStartupBehaviour("Remember from last run", MainWindow::RememberLast);

    const int currentBehaviour = m_settings->value<Settings::StartupBehaviour>();
    m_startupBehaviour->setCurrentIndex(currentBehaviour);
}

void GeneralPageWidget::apply()
{
    m_settings->set<Settings::StartupBehaviour>(m_startupBehaviour->currentIndex());
}

GeneralPage::GeneralPage(Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::GeneralCore);
    setName(tr("General"));
    setCategory({"General"});
    setWidgetCreator([settings] {
        return new GeneralPageWidget(settings);
    });
}

} // namespace Fy::Gui::Settings
