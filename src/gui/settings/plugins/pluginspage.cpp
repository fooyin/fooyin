/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "plugininstallhandler.h"
#include "pluginsettingsregistry.h"
#include "pluginsmodel.h"
#include "settings/plugins/pluginaboutdialog.h"

#include <core/internalcoresettings.h>
#include <core/plugins/plugin.h>
#include <core/plugins/plugininfo.h>
#include <core/plugins/plugininstaller.h>
#include <core/plugins/pluginmanager.h>
#include <gui/guiconstants.h>
#include <gui/widgets/checkboxdelegate.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QMenu>
#include <QProcess>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStyle>
#include <QStyleOptionViewItem>
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
    explicit PluginPageWidget(PluginManager* pluginManager, PluginSettingsRegistry* pluginSettingsRegistry,
                              SettingsManager* settings, PluginsModel* model);

    void load() override;
    void apply() override;
    void finish() override;
    void reset() override;

Q_SIGNALS:
    void pluginInstallRequested(const QString& filepath);

protected:
    void changeEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    [[nodiscard]] PluginInfo* currentPlugin() const;

    void selectionChanged();
    void updateStatusWidth();

    void aboutPlugin();
    void configurePlugin();
    void installPlugin();
    void removePlugin();

    void showContextMenu(const QPoint& position);
    [[nodiscard]] bool hasConfigProvider(const PluginInfo* pluginInfo) const;

    PluginManager* m_pluginManager;
    PluginSettingsRegistry* m_pluginSettingsRegistry;
    SettingsManager* m_settings;

    QTreeView* m_pluginList;
    PluginsModel* m_model;

    QPushButton* m_configurePlugin;
    QPushButton* m_aboutPlugin;
    QPushButton* m_installPlugin;
    QPushButton* m_removePlugin;
    QAction* m_removeAction;
};

PluginPageWidget::PluginPageWidget(PluginManager* pluginManager, PluginSettingsRegistry* pluginSettingsRegistry,
                                   SettingsManager* settings, PluginsModel* model)
    : m_pluginManager{pluginManager}
    , m_pluginSettingsRegistry{pluginSettingsRegistry}
    , m_settings{settings}
    , m_pluginList{new QTreeView(this)}
    , m_model{model}
    , m_configurePlugin{new QPushButton(tr("Configure"), this)}
    , m_aboutPlugin{new QPushButton(tr("About"), this)}
    , m_installPlugin{new QPushButton(tr("Install…"), this)}
    , m_removePlugin{new QPushButton(this)}
    , m_removeAction{new QAction(tr("Remove"), this)}
{
    setAcceptDrops(true);

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
    m_pluginList->header()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_pluginList->setSortingEnabled(true);
    m_pluginList->setContextMenuPolicy(Qt::CustomContextMenu);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(m_pluginList, 0, 0, 1, 5);
    mainLayout->addWidget(m_aboutPlugin, 1, 0);
    mainLayout->addWidget(m_configurePlugin, 1, 1);
    mainLayout->addWidget(m_removePlugin, 1, 2);
    mainLayout->addWidget(m_installPlugin, 1, 4);
    mainLayout->setColumnStretch(3, 1);
    mainLayout->setRowStretch(0, 1);

    QObject::connect(m_model, &QAbstractItemModel::modelReset, m_pluginList, &QTreeView::expandAll);
    QObject::connect(m_pluginList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &PluginPageWidget::selectionChanged);
    QObject::connect(m_configurePlugin, &QPushButton::pressed, this, &PluginPageWidget::configurePlugin);
    QObject::connect(m_aboutPlugin, &QPushButton::pressed, this, &PluginPageWidget::aboutPlugin);
    QObject::connect(m_installPlugin, &QPushButton::pressed, this, &PluginPageWidget::installPlugin);
    QObject::connect(m_removePlugin, &QPushButton::pressed, m_removeAction, &QAction::trigger);
    QObject::connect(m_removeAction, &QAction::triggered, this, &PluginPageWidget::removePlugin);
    QObject::connect(m_removeAction, &QAction::changed, this, [this] {
        m_removePlugin->setText(m_removeAction->text());
        m_removePlugin->setEnabled(m_removeAction->isEnabled());
        m_removePlugin->setToolTip(m_removeAction->toolTip());
    });
    QObject::connect(m_pluginList, &QTreeView::customContextMenuRequested, this, &PluginPageWidget::showContextMenu);
    QObject::connect(m_model, &QAbstractItemModel::dataChanged, this, &PluginPageWidget::selectionChanged);

    selectionChanged();
    updateStatusWidth();
}

void PluginPageWidget::load()
{
    m_model->reset(m_settings->value<Settings::Core::Internal::DisabledPlugins>(), PluginInstaller::pendingRemovals());
}

void PluginPageWidget::apply()
{
    const auto disabledPlugins = m_model->disabledPlugins();
    const auto enabledPlugins  = m_model->enabledPlugins();

    if(!m_model->hasChanges()) {
        return;
    }

    QStringList disabledIdentifiers = m_settings->value<Settings::Core::Internal::DisabledPlugins>();
    for(const QString& identifier : enabledPlugins) {
        disabledIdentifiers.removeAll(identifier);
    }
    for(const QString& identifier : disabledPlugins) {
        if(!disabledIdentifiers.contains(identifier)) {
            disabledIdentifiers.emplace_back(identifier);
        }
    }

    m_settings->set<Settings::Core::Internal::DisabledPlugins>(disabledIdentifiers);
    PluginInstaller::setPendingRemovals(m_model->removedPlugins());
    for(const auto& [identifier, plugin] : m_pluginManager->allPluginInfo()) {
        plugin->setDisabled(disabledIdentifiers.contains(identifier));
    }
    m_model->apply(disabledIdentifiers);

    PluginInstall::promptForRestart(this, tr("Plugins Changed"));
}

void PluginPageWidget::finish()
{
    load();
}

void PluginPageWidget::reset() { }

void PluginPageWidget::changeEvent(QEvent* event)
{
    SettingsPageWidget::changeEvent(event);
    if(event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        updateStatusWidth();
    }
}

void PluginPageWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if(!PluginInstall::fileFromMimeData(event->mimeData(), PluginInstall::DropTarget::PluginsPage).isEmpty()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
}

void PluginPageWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if(!PluginInstall::fileFromMimeData(event->mimeData(), PluginInstall::DropTarget::PluginsPage).isEmpty()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
}

void PluginPageWidget::dropEvent(QDropEvent* event)
{
    const QString filepath = PluginInstall::fileFromMimeData(event->mimeData(), PluginInstall::DropTarget::PluginsPage);
    if(filepath.isEmpty()) {
        event->ignore();
        return;
    }

    event->setDropAction(Qt::CopyAction);
    event->accept();
    Q_EMIT pluginInstallRequested(filepath);
}

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
    const auto* plugin        = currentPlugin();
    const bool pendingRemoval = plugin && m_model->isPendingRemoval(plugin->identifier());

    m_removeAction->setText(pendingRemoval ? tr("Undo removal") : tr("Remove"));
    m_removeAction->setEnabled(m_model->canRemove(plugin));
    m_removeAction->setToolTip(plugin && !m_removeAction->isEnabled()
                                   ? tr("This plugin cannot be removed here. Use its installer or package manager.")
                                   : QString{});

    if(plugin) {
        m_configurePlugin->setEnabled(!pendingRemoval && plugin->plugin() && hasConfigProvider(plugin));
        m_aboutPlugin->setEnabled(true);
        return;
    }

    m_configurePlugin->setDisabled(true);
    m_aboutPlugin->setDisabled(true);
}

void PluginPageWidget::updateStatusWidth()
{
    QStyleOptionViewItem option;
    option.initFrom(m_pluginList);
    option.font     = m_pluginList->font();
    option.widget   = m_pluginList;
    option.features = QStyleOptionViewItem::HasDisplay;

    int width = m_pluginList->header()->sectionSizeHint(4);
    for(int i{0}; i < static_cast<int>(PluginsModel::Status::Count); ++i) {
        option.text = PluginsModel::statusText(static_cast<PluginsModel::Status>(i));
        width       = std::max(
            width, m_pluginList->style()->sizeFromContents(QStyle::CT_ItemViewItem, &option, {}, m_pluginList).width());
    }
    m_pluginList->header()->resizeSection(4, width);
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
        if(auto* provider = m_pluginSettingsRegistry->providerFor(plugin->identifier())) {
            provider->showSettings(this);
        }
    }
}

void PluginPageWidget::installPlugin()
{
#ifdef Q_OS_WIN
    const QString pluginFilter = tr("fooyin Plugins (*.fyplugin *.zip *.dll)");
#else
    const QString pluginFilter = tr("fooyin Plugins (*.fyplugin *.zip *.so)");
#endif
    const QString filepath = QFileDialog::getOpenFileName(this, tr("Install Plugin"), {}, pluginFilter, nullptr,
                                                          QFileDialog::DontResolveSymlinks);

    if(filepath.isEmpty()) {
        return;
    }

    Q_EMIT pluginInstallRequested(filepath);
}

void PluginPageWidget::removePlugin()
{
    if(const auto* plugin = currentPlugin()) {
        m_model->toggleRemoval(plugin->identifier());
    }
}

void PluginPageWidget::showContextMenu(const QPoint& position)
{
    const QModelIndex index = m_pluginList->indexAt(position);
    if(!index.isValid() || !index.data(PluginItem::Plugin).value<PluginInfo*>()) {
        return;
    }

    m_pluginList->selectionModel()->setCurrentIndex(index,
                                                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* aboutAction = menu->addAction(m_aboutPlugin->text(), m_aboutPlugin, &QPushButton::click);
    aboutAction->setEnabled(m_aboutPlugin->isEnabled());

    auto* configureAction = menu->addAction(m_configurePlugin->text(), m_configurePlugin, &QPushButton::click);
    configureAction->setEnabled(m_configurePlugin->isEnabled());

    menu->addSeparator();
    menu->addAction(m_removeAction);

    menu->popup(m_pluginList->viewport()->mapToGlobal(position));
}

bool PluginPageWidget::hasConfigProvider(const PluginInfo* pluginInfo) const
{
    return pluginInfo && m_pluginSettingsRegistry && m_pluginSettingsRegistry->hasProvider(pluginInfo->identifier());
}

PluginPage::PluginPage(PluginManager* pluginManager, PluginSettingsRegistry* pluginSettingsRegistry,
                       SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
    , m_settings{settings}
    , m_model{new PluginsModel(pluginManager, this)}
{
    setId(Constants::Page::Plugins);
    setName(tr("General"));
    setCategory({tr("Plugins")});
    setWidgetCreator([this, pluginManager, pluginSettingsRegistry, settings] {
        auto* widget = new PluginPageWidget(pluginManager, pluginSettingsRegistry, settings, m_model);
        QObject::connect(widget, &PluginPageWidget::pluginInstallRequested, this,
                         [this, widget](const QString& filepath) { installPlugin(filepath, widget, false); });
        return widget;
    });
}

void PluginPage::installPlugin(const QString& filepath, QWidget* parent, bool promptRestart)
{
    auto plugin = PluginInstall::install(filepath, parent, promptRestart);
    if(!plugin) {
        return;
    }

    auto removals = PluginInstaller::pendingRemovals();
    if(removals.removeAll(plugin->identifier())) {
        PluginInstaller::setPendingRemovals(removals);
    }
    plugin->setDisabled(m_settings->value<Settings::Core::Internal::DisabledPlugins>().contains(plugin->identifier()));
    m_model->addInstalledPlugin(std::move(plugin));
}
} // namespace Fooyin

#include "moc_pluginspage.cpp"
#include "pluginspage.moc"
