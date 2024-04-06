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

#include "pluginspage.h"

#include "core/application.h"
#include "core/internalcoresettings.h"
#include "core/plugins/pluginmanager.h"
#include "pluginsdelegate.h"
#include "pluginsmodel.h"

#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QFileDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTableView>

namespace Fooyin {
class PluginPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit PluginPageWidget(PluginManager* pluginManager, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void installPlugin();
    void pluginsChanged();

    PluginManager* m_pluginManager;
    SettingsManager* m_settings;

    QTableView* m_pluginList;
    PluginsModel* m_model;

    QPushButton* m_installPlugin;
    bool m_pluginsChanged{false};
};

PluginPageWidget::PluginPageWidget(PluginManager* pluginManager, SettingsManager* settings)
    : m_pluginManager{pluginManager}
    , m_settings{settings}
    , m_pluginList{new QTableView(this)}
    , m_model{new PluginsModel(m_pluginManager)}
    , m_installPlugin{new QPushButton(tr("Install…"), this)}
{
    m_pluginList->setModel(m_model);
    m_pluginList->setItemDelegateForColumn(4, new PluginsDelegate(this));

    m_pluginList->verticalHeader()->hide();
    m_pluginList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* mainLayout = new QGridLayout(this);

    mainLayout->addWidget(m_pluginList, 0, 0, 1, 2);
    mainLayout->addWidget(m_installPlugin, 1, 1);

    mainLayout->setColumnStretch(0, 1);
    mainLayout->setRowStretch(0, 1);

    QObject::connect(m_installPlugin, &QPushButton::pressed, this, &PluginPageWidget::installPlugin);
    QObject::connect(m_model, &PluginsModel::pluginsChanged, this, &PluginPageWidget::pluginsChanged);

    m_pluginList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
}

void PluginPageWidget::load() { }

void PluginPageWidget::apply()
{
    if(m_pluginsChanged) {
        QStringList disabledPlugins;

        const auto& plugins = m_pluginManager->allPluginInfo();
        for(const auto& [name, plugin] : plugins) {
            if(plugin->isDisabled()) {
                disabledPlugins.emplace_back(plugin->identifier());
            }
        }

        m_settings->set<Settings::Core::Internal::DisabledPlugins>(disabledPlugins);

        QMessageBox msg{QMessageBox::Question, tr("Plugins Changed"),
                        tr("Restart for changes to take effect. Restart now?"), QMessageBox::Yes | QMessageBox::No};
        if(msg.exec() == QMessageBox::Yes) {
            Application::restart();
        }
    }
}

void PluginPageWidget::reset() { }

void PluginPageWidget::installPlugin()
{
    const QString filepath = QFileDialog::getOpenFileName(this, QStringLiteral("Install Plugin"), QStringLiteral(""),
                                                          QStringLiteral("Fooyin Plugin (*.so)"));

    if(filepath.isEmpty()) {
        return;
    }

    if(m_pluginManager->installPlugin(filepath)) {
        QMessageBox msg{QMessageBox::Question, tr("Plugin Installed"),
                        tr("Restart for changes to take effect. Restart now?"), QMessageBox::Yes | QMessageBox::No};
        if(msg.exec() == QMessageBox::Yes) {
            Application::restart();
        }
    }
}

void PluginPageWidget::pluginsChanged()
{
    m_pluginsChanged = true;
}

PluginPage::PluginPage(SettingsManager* settings, PluginManager* pluginManager)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::Plugins);
    setName(tr("General"));
    setCategory({tr("Plugins")});
    setWidgetCreator([pluginManager, settings] { return new PluginPageWidget(pluginManager, settings); });
}
} // namespace Fooyin

#include "moc_pluginspage.cpp"
#include "pluginspage.moc"
