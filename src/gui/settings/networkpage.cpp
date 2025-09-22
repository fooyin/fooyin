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

#include <core/coresettings.h>
#include <core/internalcoresettings.h>
#include <core/network/networkaccessmanager.h>
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

using namespace Qt::StringLiterals;

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
    QLabel* m_typeLabel;
    QLabel* m_type;
    QComboBox* m_proxyType;
    QLabel* m_hostLabel;
    QLineEdit* m_host;
    QLabel* m_portLabel;
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
    , m_typeLabel{new QLabel(tr("Type") + ":"_L1, this)}
    , m_proxyType{new QComboBox(this)}
    , m_hostLabel{new QLabel(tr("Host") + ":"_L1, this)}
    , m_host{new QLineEdit(this)}
    , m_portLabel{new QLabel(tr("Port") + ":"_L1, this)}
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

    m_proxyType->addItem(u"HTTP"_s);
    m_proxyType->addItem(u"SOCKS5"_s);

    m_auth->setCheckable(true);
    auto* authLayout = new QGridLayout(m_auth);

    int row{0};
    authLayout->addWidget(new QLabel(tr("Username") + ":"_L1, this), row, 0);
    authLayout->addWidget(m_username, row++, 1);
    authLayout->addWidget(new QLabel(tr("Password") + ":"_L1, this), row, 0);
    authLayout->addWidget(m_password, row++, 1);
    proxyLayout->setColumnStretch(1, 1);

    m_port->setRange(0, 65535);

    row = 0;
    proxyLayout->addWidget(m_noProxy, row++, 0, 1, 3);
    proxyLayout->addWidget(m_useSystemProxy, row++, 0, 1, 3);
    proxyLayout->addWidget(m_manualProxy, row++, 0, 1, 3);
    proxyLayout->addWidget(m_typeLabel, row, 0);
    proxyLayout->addWidget(m_proxyType, row++, 1);
    proxyLayout->addWidget(m_hostLabel, row, 0);
    proxyLayout->addWidget(m_host, row, 1, 1, 2);
    proxyLayout->addWidget(m_portLabel, row, 3);
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
    const auto mode = static_cast<NetworkAccessManager::Mode>(m_settings->value<Settings::Core::Internal::ProxyMode>());
    m_noProxy->setChecked(mode == NetworkAccessManager::Mode::None);
    m_useSystemProxy->setChecked(mode == NetworkAccessManager::Mode::System);
    m_manualProxy->setChecked(mode == NetworkAccessManager::Mode::Manual);

    m_host->setText(m_settings->value<Settings::Core::Internal::ProxyHost>());
    m_port->setValue(m_settings->value<Settings::Core::Internal::ProxyPort>());
    m_auth->setChecked(m_settings->value<Settings::Core::Internal::ProxyAuth>());
    m_username->setText(m_settings->value<Settings::Core::Internal::ProxyUsername>());
    m_password->setText(m_settings->value<Settings::Core::Internal::ProxyPassword>());

    updateWidgetState();
}

void NetworkPageWidget::apply()
{
    const auto mode = m_proxyActionGroup->checkedId();
    m_settings->set<Settings::Core::Internal::ProxyMode>(mode);
    const auto type = m_proxyType->currentIndex() == 0 ? QNetworkProxy::HttpProxy : QNetworkProxy::Socks5Proxy;
    m_settings->set<Settings::Core::Internal::ProxyType>(static_cast<int>(type));
    m_settings->set<Settings::Core::Internal::ProxyHost>(m_host->text());
    m_settings->set<Settings::Core::Internal::ProxyPort>(m_port->value());
    m_settings->set<Settings::Core::Internal::ProxyAuth>(m_auth->isChecked());
    m_settings->set<Settings::Core::Internal::ProxyUsername>(m_username->text());
    m_settings->set<Settings::Core::Internal::ProxyPassword>(m_password->text());
}

void NetworkPageWidget::reset()
{
    m_settings->reset<Settings::Core::Internal::ProxyMode>();
    m_settings->reset<Settings::Core::Internal::ProxyType>();
    m_settings->reset<Settings::Core::Internal::ProxyHost>();
    m_settings->reset<Settings::Core::Internal::ProxyPort>();
    m_settings->reset<Settings::Core::Internal::ProxyAuth>();
    m_settings->reset<Settings::Core::Internal::ProxyUsername>();
    m_settings->reset<Settings::Core::Internal::ProxyPassword>();
}

void NetworkPageWidget::updateWidgetState()
{
    const bool manualProxy = m_manualProxy->isChecked();

    m_typeLabel->setVisible(manualProxy);
    m_proxyType->setVisible(manualProxy);
    m_hostLabel->setVisible(manualProxy);
    m_host->setVisible(manualProxy);
    m_portLabel->setVisible(manualProxy);
    m_port->setVisible(manualProxy);
    m_auth->setVisible(manualProxy);
    m_username->setVisible(manualProxy);
    m_password->setVisible(manualProxy);

    const bool useAuth = m_auth->isChecked();

    m_username->setEnabled(useAuth);
    m_password->setEnabled(useAuth);
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
