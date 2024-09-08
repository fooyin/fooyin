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

#include "networkpage.h"
#include "core/network/networkaccessmanager.h"

#include <core/coresettings.h>
#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QSpinBox>

namespace Fooyin {
class NetworkPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit NetworkPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void updateWidgetState();

    SettingsManager* m_settings;

    QButtonGroup* m_proxyActionGroup;
    QRadioButton* m_noProxy;
    QRadioButton* m_useSystemProxy;
    QRadioButton* m_manualProxy;
    QComboBox* m_proxyType;
    QLineEdit* m_host;
    QSpinBox* m_port;
    QGroupBox* m_auth;
    QLineEdit* m_username;
    QLineEdit* m_password;
};

NetworkPageWidget::NetworkPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_proxyActionGroup{new QButtonGroup(this)}
    , m_noProxy{new QRadioButton(tr("No proxy"), this)}
    , m_useSystemProxy{new QRadioButton(tr("Use system proxy"), this)}
    , m_manualProxy{new QRadioButton(tr("Manual proxy"), this)}
    , m_proxyType{new QComboBox(this)}
    , m_host{new QLineEdit(this)}
    , m_port{new QSpinBox(this)}
    , m_auth{new QGroupBox(tr("Authentication"), this)}
    , m_username{new QLineEdit(this)}
    , m_password{new QLineEdit(this)}
{
    m_password->setEchoMode(QLineEdit::Password);

    auto* proxyGroup  = new QGroupBox(tr("Proxy"), this);
    auto* proxyLayout = new QGridLayout(proxyGroup);

    m_proxyActionGroup->addButton(m_noProxy, 0);
    m_proxyActionGroup->addButton(m_useSystemProxy, 1);
    m_proxyActionGroup->addButton(m_manualProxy, 2);

    m_proxyType->addItem(QStringLiteral("HTTP"));
    m_proxyType->addItem(QStringLiteral("SOCKS5"));

    m_auth->setCheckable(true);
    auto* authLayout = new QGridLayout(m_auth);

    int row{0};
    authLayout->addWidget(new QLabel(tr("Username") + u":", this), row, 0);
    authLayout->addWidget(m_username, row++, 1);
    authLayout->addWidget(new QLabel(tr("Password") + u":", this), row, 0);
    authLayout->addWidget(m_password, row++, 1);
    proxyLayout->setColumnStretch(1, 1);

    m_port->setRange(0, 65535);

    row = 0;
    proxyLayout->addWidget(m_noProxy, row++, 0, 1, 3);
    proxyLayout->addWidget(m_useSystemProxy, row++, 0, 1, 3);
    proxyLayout->addWidget(m_manualProxy, row++, 0, 1, 3);
    proxyLayout->addWidget(new QLabel(tr("Type") + u":", this), row, 0);
    proxyLayout->addWidget(m_proxyType, row++, 1);
    proxyLayout->addWidget(new QLabel(tr("Host") + u":", this), row, 0);
    proxyLayout->addWidget(m_host, row, 1, 1, 2);
    proxyLayout->addWidget(new QLabel(tr("Port") + u":", this), row, 3);
    proxyLayout->addWidget(m_port, row++, 4);
    proxyLayout->addWidget(m_auth, row++, 0, 1, 5);
    proxyLayout->setColumnStretch(2, 1);

    auto* layout = new QGridLayout(this);
    layout->addWidget(proxyGroup);
    layout->setRowStretch(layout->rowCount(), 1);

    QObject::connect(m_proxyActionGroup, &QButtonGroup::buttonToggled, this, &NetworkPageWidget::updateWidgetState);
}

void NetworkPageWidget::load()
{
    const auto proxyMode = static_cast<NetworkAccessManager::Mode>(m_settings->value<Settings::Core::ProxyMode>());
    m_noProxy->setChecked(proxyMode == NetworkAccessManager::Mode::None);
    m_useSystemProxy->setChecked(proxyMode == NetworkAccessManager::Mode::System);
    m_manualProxy->setChecked(proxyMode == NetworkAccessManager::Mode::Manual);

    const auto config = m_settings->value<Settings::Core::ProxyConfig>().value<NetworkAccessManager::ProxyConfig>();
    m_host->setText(config.host);
    m_port->setValue(config.port);
    m_auth->setChecked(config.useAuth);
    m_username->setText(config.username);
    m_password->setText(config.password);

    updateWidgetState();
}

void NetworkPageWidget::apply()
{
    const auto id = m_proxyActionGroup->checkedId();
    m_settings->set<Settings::Core::ProxyMode>(id);

    NetworkAccessManager::ProxyConfig config;
    if(m_proxyType->currentIndex() == 0) {
        config.type = QNetworkProxy::HttpProxy;
    }
    else {
        config.type = QNetworkProxy::Socks5Proxy;
    }
    config.host     = m_host->text();
    config.port     = m_port->value();
    config.useAuth  = m_auth->isChecked();
    config.username = m_username->text();
    config.password = m_password->text();
}

void NetworkPageWidget::reset()
{
    m_settings->reset<Settings::Core::ProxyMode>();
    m_settings->reset<Settings::Core::ProxyConfig>();
}

void NetworkPageWidget::updateWidgetState()
{
    const bool manualProxy = m_manualProxy->isChecked();
    m_proxyType->setEnabled(manualProxy);
    m_host->setEnabled(manualProxy);
    m_port->setEnabled(manualProxy);
    m_auth->setEnabled(manualProxy);
    m_username->setEnabled(manualProxy);
    m_password->setEnabled(manualProxy);
}

NetworkPage::NetworkPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::Network);
    setName(tr("General"));
    setCategory({tr("Networking")});
    setWidgetCreator([settings] { return new NetworkPageWidget(settings); });
}
} // namespace Fooyin

#include "moc_networkpage.cpp"
#include "networkpage.moc"
