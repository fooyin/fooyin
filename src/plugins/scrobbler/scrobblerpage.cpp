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

#include "scrobblerpage.h"

#include "scrobbler.h"
#include "scrobblersettings.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>

namespace Fooyin::Scrobbler {
class ScrobblerPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit ScrobblerPageWidget(Scrobbler* scrobbler, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void populateServices(QGridLayout* layout);
    void toggleLogin(const QString& name);
    void updateServiceState(const QString& name);

    Scrobbler* m_scrobbler;
    SettingsManager* m_settings;

    QCheckBox* m_scrobblingEnabled;
    QCheckBox* m_preferAlbumArtist;
    QSpinBox* m_scrobbleDelay;

    struct ServiceContext
    {
        ScrobblerService* service;
        QPushButton* button;
        QLabel* label;
        QLabel* icon;
        QString error;
    };

    std::map<QString, ServiceContext> m_serviceContext;
};

ScrobblerPageWidget::ScrobblerPageWidget(Scrobbler* scrobbler, SettingsManager* settings)
    : m_scrobbler{scrobbler}
    , m_settings{settings}
    , m_scrobblingEnabled{new QCheckBox(tr("Enable scrobbling"), this)}
    , m_preferAlbumArtist{new QCheckBox(tr("Prefer album artist"), this)}
    , m_scrobbleDelay{new QSpinBox(this)}
{
    auto* genralGroup   = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(genralGroup);

    auto* delayLabel = new QLabel(tr("Scrobble delay") + u":", this);

    const QString delayTip = tr("Time to wait before submitting scrobbles");

    delayLabel->setToolTip(delayTip);
    m_scrobbleDelay->setToolTip(delayTip);

    m_scrobbleDelay->setRange(0, 600);
    m_scrobbleDelay->setSuffix(u" " + tr("seconds"));

    int row{0};
    generalLayout->addWidget(m_scrobblingEnabled, row++, 0, 1, 2);
    generalLayout->addWidget(m_preferAlbumArtist, row++, 0, 1, 2);
    generalLayout->addWidget(delayLabel, row, 0);
    generalLayout->addWidget(m_scrobbleDelay, row++, 1);
    generalLayout->setRowStretch(generalLayout->rowCount(), 1);
    generalLayout->setColumnStretch(2, 1);

    auto* serviceGroup  = new QGroupBox(tr("Services"), this);
    auto* serviceLayout = new QGridLayout(serviceGroup);

    populateServices(serviceLayout);

    serviceLayout->setRowStretch(serviceLayout->rowCount(), 1);
    serviceLayout->setColumnStretch(1, 1);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(genralGroup, row++, 0, 1, 3);
    layout->addWidget(serviceGroup, row++, 0, 1, 3);
    layout->setRowStretch(layout->rowCount(), 1);
    layout->setColumnStretch(2, 1);
}

void ScrobblerPageWidget::load()
{
    m_scrobblingEnabled->setChecked(m_settings->value<Settings::Scrobbler::ScrobblingEnabled>());
    m_scrobbleDelay->setValue(m_settings->value<Settings::Scrobbler::ScrobblingDelay>());
    m_preferAlbumArtist->setChecked(m_settings->value<Settings::Scrobbler::PreferAlbumArtist>());
}

void ScrobblerPageWidget::apply()
{
    m_settings->set<Settings::Scrobbler::ScrobblingEnabled>(m_scrobblingEnabled->isChecked());
    m_settings->set<Settings::Scrobbler::ScrobblingDelay>(m_scrobbleDelay->value());
    m_settings->set<Settings::Scrobbler::PreferAlbumArtist>(m_preferAlbumArtist->isChecked());
}

void ScrobblerPageWidget::reset()
{
    m_settings->reset<Settings::Scrobbler::ScrobblingEnabled>();
    m_settings->reset<Settings::Scrobbler::ScrobblingDelay>();
    m_settings->reset<Settings::Scrobbler::PreferAlbumArtist>();
}

void ScrobblerPageWidget::populateServices(QGridLayout* layout)
{
    const auto services = m_scrobbler->services();
    int row{0};

    for(ScrobblerService* service : services) {
        auto* serviceName   = new QLabel(QStringLiteral("<b>%1</b>").arg(service->name()), this);
        auto* serviceStatus = new QLabel(this);
        auto* loginBtn      = new QPushButton(this);

        auto* serviceIcon = new QLabel(this);
        serviceIcon->setPixmap(style()->standardIcon(QStyle::SP_DialogApplyButton).pixmap(24, 24));

        m_serviceContext.emplace(service->name(),
                                 ServiceContext{service, loginBtn, serviceStatus, serviceIcon, QString{}});

        updateServiceState(service->name());

        layout->addWidget(serviceIcon, row, 0, Qt::AlignLeft);
        layout->addWidget(serviceName, row, 1, Qt::AlignLeft);
        layout->addWidget(serviceStatus, row, 2, Qt::AlignLeft);
        layout->addWidget(loginBtn, row, 3, Qt::AlignRight);

        QObject::connect(loginBtn, &QPushButton::clicked, this, [this, service]() { toggleLogin(service->name()); });

        row++;
    }
}

void ScrobblerPageWidget::toggleLogin(const QString& name)
{
    if(!m_serviceContext.contains(name)) {
        return;
    }

    const auto& service = m_serviceContext.at(name).service;

    if(service->isAuthenticated()) {
        service->logout();
        updateServiceState(name);
        return;
    }

    const auto authFinished = [this, name](const bool success, const QString& error) {
        m_serviceContext.at(name).error = success ? QString{} : error;
        updateServiceState(name);
    };

    QObject::connect(service, &ScrobblerService::authenticationFinished, this, authFinished);
    service->authenticate();
}

void ScrobblerPageWidget::updateServiceState(const QString& name)
{
    if(!m_serviceContext.contains(name)) {
        return;
    }

    const auto& [service, button, label, icon, error] = m_serviceContext.at(name);

    if(service->isAuthenticated()) {
        button->setText(tr("Sign-out"));
        label->setText(tr("Signed in as %1").arg(service->username()));
        icon->show();
    }
    else {
        button->setText(tr("Login"));
        label->setText(tr("Not signed in"));
        icon->hide();
    }

    if(!error.isEmpty()) {
        label->setText(error);
    }
}

ScrobblerPage::ScrobblerPage(Scrobbler* scrobbler, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId({"Fooyin.Page.Network.Scrobbling"});
    setName(tr("General"));
    setCategory({tr("Networking"), tr("Scrobbling")});
    setWidgetCreator([scrobbler, settings] { return new ScrobblerPageWidget(scrobbler, settings); });
}
} // namespace Fooyin::Scrobbler

#include "moc_scrobblerpage.cpp"
#include "scrobblerpage.moc"
