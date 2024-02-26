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

#include "core/plugins/pluginmanager.h"
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
    explicit PluginPageWidget(PluginManager* pluginManager);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void installPlugin();

    PluginManager* m_pluginManager;

    QTableView* m_pluginList;
    PluginsModel* m_model;

    QPushButton* m_installPlugin;
};

PluginPageWidget::PluginPageWidget(PluginManager* pluginManager)
    : m_pluginManager{pluginManager}
    , m_pluginList{new QTableView(this)}
    , m_model{new PluginsModel(m_pluginManager)}
    , m_installPlugin{new QPushButton(tr("Install…"), this)}
{
    m_pluginList->setModel(m_model);

    m_pluginList->verticalHeader()->hide();
    m_pluginList->horizontalHeader()->setStretchLastSection(true);
    m_pluginList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* mainLayout = new QGridLayout(this);

    mainLayout->addWidget(m_pluginList, 0, 0, 1, 2);
    mainLayout->addWidget(m_installPlugin, 1, 1);

    mainLayout->setColumnStretch(0, 1);
    mainLayout->setRowStretch(0, 1);

    QObject::connect(m_installPlugin, &QPushButton::pressed, this, &PluginPageWidget::installPlugin);
}

void PluginPageWidget::load() { }

void PluginPageWidget::apply() { }

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
            QCoreApplication::quit();
            QProcess::startDetached(QApplication::applicationFilePath(), {QStringLiteral("-s")});
        }
    }
}

PluginPage::PluginPage(SettingsManager* settings, PluginManager* pluginManager)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::Plugins);
    setName(tr("General"));
    setCategory({tr("Plugins")});
    setWidgetCreator([pluginManager] { return new PluginPageWidget(pluginManager); });
}
} // namespace Fooyin

#include "moc_pluginspage.cpp"
#include "pluginspage.moc"
