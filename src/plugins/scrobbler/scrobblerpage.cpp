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

#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>

using namespace Qt::StringLiterals;

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

    QCheckBox* m_scrobbleFilterEnabled;
    ScriptLineEdit* m_scrobbleFilter;

    struct ServiceContext
    {
        ScrobblerService* service{nullptr};
        QPushButton* button{nullptr};
        QLabel* label{nullptr};
        QLabel* icon{nullptr};
        QString error;
        QString tokenSetting;
        QLineEdit* tokenInput{nullptr};

        [[nodiscard]] bool isValid() const
        {
            return service && button && label && icon;
        }
    };

    std::map<QString, ServiceContext> m_serviceContext;
};

ScrobblerPageWidget::ScrobblerPageWidget(Scrobbler* scrobbler, SettingsManager* settings)
    : m_scrobbler{scrobbler}
    , m_settings{settings}
    , m_scrobblingEnabled{new QCheckBox(tr("Enable scrobbling"), this)}
    , m_preferAlbumArtist{new QCheckBox(tr("Prefer album artist"), this)}
    , m_scrobbleDelay{new QSpinBox(this)}
    , m_scrobbleFilterEnabled{new QCheckBox(tr("Filter scrobbles"), this)}
    , m_scrobbleFilter{new ScriptLineEdit(tr("Filter"), this)}
{
    auto* genralGroup   = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(genralGroup);

    auto* delayLabel       = new QLabel(tr("Scrobble delay") + ":"_L1, this);
    const QString delayTip = tr("Time to wait before submitting scrobbles");

    delayLabel->setToolTip(delayTip);
    m_scrobbleDelay->setToolTip(delayTip);

    m_scrobbleDelay->setRange(0, 600);
    m_scrobbleDelay->setSuffix(" "_L1 + tr("seconds"));

    auto* filterLabel       = new QLabel(tr("Query") + ":"_L1, this);
    const QString filterTip = tr("Enter a query - tracks that match the query will NOT be scrobbled");
    filterLabel->setToolTip(filterTip);
    m_scrobbleFilter->setToolTip(filterTip);

    int row{0};
    generalLayout->addWidget(m_scrobblingEnabled, row++, 0, 1, 3);
    generalLayout->addWidget(m_preferAlbumArtist, row++, 0, 1, 3);
    generalLayout->addWidget(delayLabel, row, 0, 1, 2);
    generalLayout->addWidget(m_scrobbleDelay, row++, 2);

    generalLayout->addWidget(m_scrobbleFilterEnabled, row++, 0, 1, 3);
    generalLayout->addWidget(filterLabel, row, 0, 1, 1);
    generalLayout->addWidget(m_scrobbleFilter, row++, 1, 1, 3);

    generalLayout->setRowStretch(generalLayout->rowCount(), 1);
    generalLayout->setColumnStretch(3, 1);

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

    QObject::connect(m_scrobbleFilterEnabled, &QCheckBox::clicked, m_scrobbleFilter, &QWidget::setEnabled);
}

void ScrobblerPageWidget::load()
{
    m_scrobblingEnabled->setChecked(m_settings->value<Settings::Scrobbler::ScrobblingEnabled>());
    m_scrobbleDelay->setValue(m_settings->value<Settings::Scrobbler::ScrobblingDelay>());
    m_preferAlbumArtist->setChecked(m_settings->value<Settings::Scrobbler::PreferAlbumArtist>());

    m_scrobbleFilterEnabled->setChecked(m_settings->value<Settings::Scrobbler::EnableScrobbleFilter>());
    m_scrobbleFilter->setText(m_settings->value<Settings::Scrobbler::ScrobbleFilter>());

    m_scrobbleFilter->setEnabled(m_scrobbleFilterEnabled->isChecked());
}

void ScrobblerPageWidget::apply()
{
    m_settings->set<Settings::Scrobbler::ScrobblingEnabled>(m_scrobblingEnabled->isChecked());
    m_settings->set<Settings::Scrobbler::ScrobblingDelay>(m_scrobbleDelay->value());
    m_settings->set<Settings::Scrobbler::PreferAlbumArtist>(m_preferAlbumArtist->isChecked());

    m_settings->set<Settings::Scrobbler::EnableScrobbleFilter>(m_scrobbleFilterEnabled->isChecked());
    m_settings->set<Settings::Scrobbler::ScrobbleFilter>(m_scrobbleFilter->text());

    for(const auto& [_, context] : m_serviceContext) {
        if(context.tokenInput && !context.tokenSetting.isEmpty()) {
            m_settings->fileSet(context.tokenSetting, context.tokenInput->text());
        }
    }
}

void ScrobblerPageWidget::reset()
{
    m_settings->reset<Settings::Scrobbler::ScrobblingEnabled>();
    m_settings->reset<Settings::Scrobbler::ScrobblingDelay>();
    m_settings->reset<Settings::Scrobbler::PreferAlbumArtist>();

    m_settings->reset<Settings::Scrobbler::EnableScrobbleFilter>();
    m_settings->reset<Settings::Scrobbler::ScrobbleFilter>();
}

void ScrobblerPageWidget::populateServices(QGridLayout* layout)
{
    const auto services = m_scrobbler->services();
    int row{0};

    for(ScrobblerService* service : services) {
        auto* serviceName   = new QLabel(u"<b>%1</b>"_s.arg(service->name()), this);
        auto* serviceStatus = new QLabel(this);
        auto* loginBtn      = new QPushButton(this);

        auto* serviceIcon = new QLabel(this);
        serviceIcon->setPixmap(style()->standardIcon(QStyle::SP_DialogApplyButton).pixmap(24, 24));

        ServiceContext context;
        context.service = service;
        context.button  = loginBtn;
        context.label   = serviceStatus;
        context.icon    = serviceIcon;

        layout->addWidget(serviceIcon, row, 0, Qt::AlignLeft);
        layout->addWidget(serviceName, row, 1, Qt::AlignLeft);
        layout->addWidget(serviceStatus, row, 2, Qt::AlignRight);
        layout->addWidget(loginBtn, row, 3, Qt::AlignRight);

        const QString token = service->tokenSetting();
        if(!token.isEmpty()) {
            auto* tokenLayout = new QGridLayout();
            auto* tokenLabel  = new QLabel(tr("User token") + ":"_L1, this);
            auto* tokenInput  = new QLineEdit(this);

            tokenInput->setReadOnly(service->isAuthenticated());

            tokenLayout->addWidget(tokenLabel, 0, 0);
            tokenLayout->addWidget(tokenInput, 0, 1);

            const QUrl tokenUrl = service->tokenUrl();
            if(tokenUrl.isValid()) {
                auto* urlLabel = new QLabel(u"🛈 "_s + tr("You can find your user token here") + ": "_L1
                                                + u"<a href=\"%1\">%1</a>"_s.arg(tokenUrl.toString()),
                                            this);
                urlLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
                urlLabel->setOpenExternalLinks(true);
                urlLabel->setTextFormat(Qt::RichText);
                tokenLayout->addWidget(urlLabel, 1, 0, 1, 2);
            }

            loginBtn->setEnabled(service->isAuthenticated() || !tokenInput->text().isEmpty());
            QObject::connect(tokenInput, &QLineEdit::textChanged, this,
                             [loginBtn](const QString& text) { loginBtn->setEnabled(!text.isEmpty()); });

            tokenLayout->setColumnStretch(1, 1);
            layout->addLayout(tokenLayout, ++row, 1, 1, 3);

            context.tokenSetting = token;
            context.tokenInput   = tokenInput;
        }

        m_serviceContext.emplace(service->name(), context);
        updateServiceState(service->name());

        QObject::connect(loginBtn, &QPushButton::clicked, this, [this, service]() { toggleLogin(service->name()); });

        row++;
    }
}

void ScrobblerPageWidget::toggleLogin(const QString& name)
{
    if(!m_serviceContext.contains(name)) {
        return;
    }

    const auto& context = m_serviceContext.at(name);
    const auto& service = context.service;

    if(service->isAuthenticated()) {
        service->logout();
        updateServiceState(name);
        return;
    }

    const auto authFinished = [this, name, context](const bool success, const QString& error) {
        m_serviceContext.at(name).error = success ? QString{} : error;
        m_settings->fileSet(context.tokenSetting, context.tokenInput->text());
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

    if(!m_serviceContext.at(name).isValid()) {
        return;
    }

    const auto& [service, button, label, icon, error, tokenSetting, tokenInput] = m_serviceContext.at(name);

    if(service->isAuthenticated()) {
        button->setText(tr("Sign Out"));
        const QString username = service->username();
        label->setText(username.isEmpty() ? tr("Signed in") : tr("Signed in as %1").arg(username));
        icon->show();
    }
    else {
        button->setText(tr("Sign In"));
        label->setText(tr("Not signed in"));
        if(tokenInput) {
            tokenInput->setReadOnly(false);
        }
        icon->hide();
    }

    if(tokenInput) {
        tokenInput->setReadOnly(service->isAuthenticated());
    }

    if(!error.isEmpty()) {
        label->setText(error);
    }

    if(tokenInput && !tokenSetting.isEmpty()) {
        tokenInput->setText(m_settings->fileValue(tokenSetting).toString());
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
