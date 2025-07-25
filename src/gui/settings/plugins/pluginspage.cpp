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

#include "pluginsmodel.h"
#include "settings/plugins/pluginaboutdialog.h"

#include <core/application.h>
#include <core/internalcoresettings.h>
#include <core/plugins/plugin.h>
#include <core/plugins/plugininfo.h>
#include <core/plugins/pluginmanager.h>
#include <gui/guiconstants.h>
#include <gui/widgets/checkboxdelegate.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QFileDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTreeView>

using namespace Qt::StringLiterals;

namespace Fooyin {
class CheckSortProxyModel : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    [[nodiscard]] bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
    {
        if(left.column() == 4 && right.column() == 4) {
            const bool leftChecked  = sourceModel()->data(left, Qt::CheckStateRole).toBool();
            const bool rightChecked = sourceModel()->data(right, Qt::CheckStateRole).toBool();

            return leftChecked && !rightChecked;
        }

        return QSortFilterProxyModel::lessThan(left, right);
    }
};

class PluginPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit PluginPageWidget(PluginManager* pluginManager, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    [[nodiscard]] PluginInfo* currentPlugin() const;

    void selectionChanged();

    void aboutPlugin();
    void configurePlugin();
    void installPlugin();

    PluginManager* m_pluginManager;
    SettingsManager* m_settings;

    QTreeView* m_pluginList;
    PluginsModel* m_model;

    QPushButton* m_configurePlugin;
    QPushButton* m_aboutPlugin;
    QPushButton* m_installPlugin;
};

PluginPageWidget::PluginPageWidget(PluginManager* pluginManager, SettingsManager* settings)
    : m_pluginManager{pluginManager}
    , m_settings{settings}
    , m_pluginList{new QTreeView(this)}
    , m_model{new PluginsModel(m_pluginManager)}
    , m_configurePlugin{new QPushButton(tr("Configure"), this)}
    , m_aboutPlugin{new QPushButton(tr("About"), this)}
    , m_installPlugin{new QPushButton(tr("Install…"), this)}
{
    auto* proxyModel = new CheckSortProxyModel(this);
    proxyModel->setSourceModel(m_model);

    m_configurePlugin->setDisabled(true);
    m_aboutPlugin->setDisabled(true);

    m_pluginList->setModel(proxyModel);
    m_pluginList->setItemDelegateForColumn(3, new CheckBoxDelegate(this));

    m_pluginList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pluginList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pluginList->header()->setSortIndicator(0, Qt::AscendingOrder);
    m_pluginList->header()->setStretchLastSection(false);
    m_pluginList->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_pluginList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_pluginList->setSortingEnabled(true);

    auto* mainLayout = new QGridLayout(this);

    mainLayout->addWidget(m_pluginList, 0, 0, 1, 4);
    mainLayout->addWidget(m_aboutPlugin, 1, 0);
    mainLayout->addWidget(m_configurePlugin, 1, 1);
    mainLayout->addWidget(m_installPlugin, 1, 3);

    mainLayout->setColumnStretch(2, 1);
    mainLayout->setRowStretch(0, 1);

    QObject::connect(m_model, &QAbstractItemModel::modelReset, m_pluginList, &QTreeView::expandAll);
    QObject::connect(m_pluginList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &PluginPageWidget::selectionChanged);
    QObject::connect(m_configurePlugin, &QPushButton::pressed, this, &PluginPageWidget::configurePlugin);
    QObject::connect(m_aboutPlugin, &QPushButton::pressed, this, &PluginPageWidget::aboutPlugin);
    QObject::connect(m_installPlugin, &QPushButton::pressed, this, &PluginPageWidget::installPlugin);
}

void PluginPageWidget::load()
{
    m_model->reset();
}

void PluginPageWidget::apply()
{
    const auto disabledPlugins = m_model->disabledPlugins();
    const auto enabledPlugins  = m_model->enabledPlugins();

    if(disabledPlugins.empty() && enabledPlugins.empty()) {
        return;
    }

    for(const auto& pluginIdentifier : enabledPlugins) {
        if(auto* plugin = m_pluginManager->pluginInfo(pluginIdentifier)) {
            plugin->setDisabled(false);
        }
    }

    QStringList disabledIdentifiers;
    const auto& plugins = m_pluginManager->allPluginInfo();
    for(const auto& [identifier, plugin] : plugins) {
        if(plugin->isDisabled() || disabledPlugins.contains(identifier)) {
            disabledIdentifiers.emplace_back(identifier);
        }
    }

    m_settings->set<Settings::Core::Internal::DisabledPlugins>(disabledIdentifiers);

    QMessageBox msg{QMessageBox::Question, tr("Plugins Changed"),
                    tr("Restart for changes to take effect. Restart now?"), QMessageBox::Yes | QMessageBox::No};
    if(msg.exec() == QMessageBox::Yes) {
        Application::restart();
    }
}

void PluginPageWidget::reset() { }

PluginInfo* PluginPageWidget::currentPlugin() const
{
    const auto indexes = m_pluginList->selectionModel()->selectedRows();
    if(indexes.empty()) {
        return nullptr;
    }

    return indexes.front().data(PluginItem::Plugin).value<PluginInfo*>();
}

void PluginPageWidget::selectionChanged()
{
    if(const auto* plugin = currentPlugin()) {
        if(plugin->plugin()) {
            m_configurePlugin->setEnabled(plugin->plugin()->hasSettings());
            m_aboutPlugin->setEnabled(true);
            return;
        }
    }

    m_configurePlugin->setDisabled(true);
    m_aboutPlugin->setDisabled(true);
}

void PluginPageWidget::aboutPlugin()
{
    if(auto* plugin = currentPlugin()) {
        auto* aboutDialog = new PluginAboutDialog(plugin, this);
        aboutDialog->setAttribute(Qt::WA_DeleteOnClose);
        aboutDialog->setModal(true);
        aboutDialog->show();
    }
}

void PluginPageWidget::configurePlugin()
{
    if(auto* plugin = currentPlugin()) {
        plugin->plugin()->showSettings(this);
    }
}

void PluginPageWidget::installPlugin()
{
    const QString filepath
        = QFileDialog::getOpenFileName(this, tr("Install Plugin"), {}, tr("%1 Plugin").arg("fooyin"_L1) + " (*.fyl)"_L1,
                                       nullptr, QFileDialog::DontResolveSymlinks);

    if(filepath.isEmpty()) {
        return;
    }

    if(PluginManager::installPlugin(filepath)) {
        QMessageBox msg{QMessageBox::Question, tr("Plugin Installed"),
                        tr("Restart for changes to take effect. Restart now?"), QMessageBox::Yes | QMessageBox::No};
        if(msg.exec() == QMessageBox::Yes) {
            Application::restart();
        }
    }
}

PluginPage::PluginPage(PluginManager* pluginManager, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::Plugins);
    setName(tr("General"));
    setCategory({tr("Plugins")});
    setWidgetCreator([pluginManager, settings] { return new PluginPageWidget(pluginManager, settings); });
}
} // namespace Fooyin

#include "moc_pluginspage.cpp"
#include "pluginspage.moc"
