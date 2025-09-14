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

#include "customservicedialog.h"
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
    void populateServices();
    void toggleLogin(ScrobblerService* service);
    void updateServiceState(ScrobblerService* service);
    void updateDetails(ScrobblerService* service);

    void addService(ScrobblerService* service);
    void addCustomService();
    void editCustomService(ScrobblerService* service);
    void deleteCustomService(ScrobblerService* service);

    Scrobbler* m_scrobbler;
    SettingsManager* m_settings;

    QCheckBox* m_scrobblingEnabled;
    QCheckBox* m_preferAlbumArtist;
    QSpinBox* m_scrobbleDelay;

    QCheckBox* m_scrobbleFilterEnabled;
    ScriptLineEdit* m_scrobbleFilter;

    QGroupBox* m_serviceGroup;
    QGridLayout* m_serviceLayout;

    struct ServiceContext
    {
        ScrobblerService* service{nullptr};

        // Auth-related
        QPushButton* loginBtn{nullptr};
        QLabel* statusLabel{nullptr};
        QString error;
        // Common
        QGroupBox* groupCheck{nullptr};
        // ListenBrainz
        QLineEdit* tokenInput{nullptr};
        // Custom
        QLineEdit* url{nullptr};
        QLineEdit* token{nullptr};
    };

    std::vector<ServiceContext>::iterator findContext(ScrobblerService* service);

    std::vector<ServiceContext> m_serviceContext;
    std::vector<ServiceContext> m_pendingDelete;

    QPushButton* m_addCustomService;
};

ScrobblerPageWidget::ScrobblerPageWidget(Scrobbler* scrobbler, SettingsManager* settings)
    : m_scrobbler{scrobbler}
    , m_settings{settings}
    , m_scrobblingEnabled{new QCheckBox(tr("Enable scrobbling"), this)}
    , m_preferAlbumArtist{new QCheckBox(tr("Prefer album artist"), this)}
    , m_scrobbleDelay{new QSpinBox(this)}
    , m_scrobbleFilterEnabled{new QCheckBox(tr("Filter scrobbles"), this)}
    , m_scrobbleFilter{new ScriptLineEdit(tr("Filter"), this)}
    , m_serviceGroup{new QGroupBox(tr("Services"), this)}
    , m_serviceLayout{new QGridLayout(m_serviceGroup)}
    , m_addCustomService{new QPushButton(tr("Add Service"), this)}
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

    m_serviceLayout->setRowStretch(m_serviceLayout->rowCount(), 1);
    m_serviceLayout->setColumnStretch(0, 1);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(genralGroup, row++, 0, 1, 3);
    layout->addWidget(m_serviceGroup, row++, 0, 1, 3);
    layout->addWidget(m_addCustomService, row++, 0, 1, 3);
    layout->setRowStretch(layout->rowCount(), 1);
    layout->setColumnStretch(2, 1);

    QObject::connect(m_scrobbleFilterEnabled, &QCheckBox::clicked, m_scrobbleFilter, &QWidget::setEnabled);
    QObject::connect(m_addCustomService, &QPushButton::clicked, this, &ScrobblerPageWidget::addCustomService);
}

void ScrobblerPageWidget::load()
{
    m_scrobblingEnabled->setChecked(m_settings->value<Settings::Scrobbler::ScrobblingEnabled>());
    m_scrobbleDelay->setValue(m_settings->value<Settings::Scrobbler::ScrobblingDelay>());
    m_preferAlbumArtist->setChecked(m_settings->value<Settings::Scrobbler::PreferAlbumArtist>());

    m_scrobbleFilterEnabled->setChecked(m_settings->value<Settings::Scrobbler::EnableScrobbleFilter>());
    m_scrobbleFilter->setText(m_settings->value<Settings::Scrobbler::ScrobbleFilter>());

    m_scrobbleFilter->setEnabled(m_scrobbleFilterEnabled->isChecked());

    populateServices();
}

void ScrobblerPageWidget::apply()
{
    m_settings->set<Settings::Scrobbler::ScrobblingEnabled>(m_scrobblingEnabled->isChecked());
    m_settings->set<Settings::Scrobbler::ScrobblingDelay>(m_scrobbleDelay->value());
    m_settings->set<Settings::Scrobbler::PreferAlbumArtist>(m_preferAlbumArtist->isChecked());

    m_settings->set<Settings::Scrobbler::EnableScrobbleFilter>(m_scrobbleFilterEnabled->isChecked());
    m_settings->set<Settings::Scrobbler::ScrobbleFilter>(m_scrobbleFilter->text());

    for(const auto& context : m_pendingDelete) {
        m_scrobbler->removeCustomService(context.service);
    }

    m_pendingDelete.clear();

    for(const auto& context : m_serviceContext) {
        updateDetails(context.service);
        context.service->saveSession();
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

void ScrobblerPageWidget::populateServices()
{
    m_pendingDelete.clear();

    const auto services = m_scrobbler->services();
    for(ScrobblerService* service : services) {
        addService(service);
    }
}

void ScrobblerPageWidget::toggleLogin(ScrobblerService* service)
{
    const auto context = findContext(service);
    if(context == m_serviceContext.cend()) {
        return;
    }

    if(service->isAuthenticated()) {
        service->logout();
        updateServiceState(service);
        return;
    }

    const auto authFinished = [this, service, context](const bool success, const QString& error) {
        context->error = success ? QString{} : error;
        updateDetails(service);
        updateServiceState(service);
    };

    QObject::connect(service, &ScrobblerService::authenticationFinished, this, authFinished);
    service->authenticate();
}

void ScrobblerPageWidget::updateServiceState(ScrobblerService* service)
{
    const auto context = findContext(service);
    if(context == m_serviceContext.cend()) {
        return;
    }

    if(context->loginBtn && service->requiresAuthentication()) {
        if(service->isAuthenticated()) {
            context->loginBtn->setText(tr("Sign Out"));

            if(context->statusLabel) {
                const QString username = service->username();
                context->statusLabel->setText(username.isEmpty() ? tr("Signed in")
                                                                 : tr("Signed in as %1").arg(username));
            }
        }
        else {
            context->loginBtn->setText(tr("Sign In"));

            if(context->statusLabel) {
                context->statusLabel->setText(tr("Not signed in"));
            }
            if(context->tokenInput) {
                context->tokenInput->setReadOnly(false);
            }
        }
    }

    const auto details = service->details();

    if(context->groupCheck) {
        context->groupCheck->setChecked(details.isEnabled);
    }

    if(context->groupCheck) {
        context->groupCheck->setTitle(details.name);
    }

    if(context->statusLabel && !context->error.isEmpty()) {
        context->statusLabel->setText(context->error);
    }

    if(context->url) {
        context->url->setText(details.url.toDisplayString());
    }

    if(context->token) {
        context->token->setText(details.token);
    }

    if(context->tokenInput) {
        context->tokenInput->setText(details.token);
    }
}

void ScrobblerPageWidget::updateDetails(ScrobblerService* service)
{
    const auto context = findContext(service);
    if(context == m_serviceContext.cend()) {
        return;
    }

    ServiceDetails details{service->details()};

    if(context->groupCheck) {
        details.isEnabled = context->groupCheck->isChecked();
    }
    if(context->tokenInput) {
        details.token = context->tokenInput->text();
    }

    service->updateDetails(details);
}

void ScrobblerPageWidget::addService(ScrobblerService* service)
{
    ServiceContext context;
    context.service    = service;
    context.groupCheck = new QGroupBox(service->name());
    context.groupCheck->setCheckable(true);

    auto* layout = new QGridLayout(context.groupCheck);
    layout->setColumnStretch(1, 1);

    const bool isCustom = service->details().isCustom();

    if(isCustom) {
        auto* editBtn = new QPushButton(tr("Edit"), this);
        QObject::connect(editBtn, &QPushButton::clicked, this, [this, service]() { editCustomService(service); });
        auto* deleteBtn = new QPushButton(tr("Delete"), this);
        QObject::connect(deleteBtn, &QPushButton::clicked, this, [this, service]() { deleteCustomService(service); });

        layout->addWidget(editBtn, 0, 2, Qt::AlignRight);
        layout->addWidget(deleteBtn, 1, 2, Qt::AlignRight);
    }

    if(service->requiresAuthentication()) {
        context.statusLabel = new QLabel(this);
        layout->addWidget(context.statusLabel, 0, 0, 1, 2);

        context.loginBtn = new QPushButton(this);
        QObject::connect(context.loginBtn, &QPushButton::clicked, this, [this, service]() { toggleLogin(service); });

        layout->addWidget(context.loginBtn, 0, 3, Qt::AlignRight);
    }
    else {
        auto* tokenLabel = new QLabel(tr("Token") + ":"_L1, this);

        if(isCustom) {
            auto* urlLabel = new QLabel(tr("URL") + ":"_L1, this);
            context.url    = new QLineEdit(this);
            context.token  = new QLineEdit(this);

            context.url->setReadOnly(true);
            context.token->setReadOnly(true);

            layout->addWidget(urlLabel, 0, 0);
            layout->addWidget(context.url, 0, 1);
            layout->addWidget(tokenLabel, 1, 0);
            layout->addWidget(context.token, 1, 1);
        }
        else {
            context.tokenInput = new QLineEdit(this);

            layout->addWidget(tokenLabel, 0, 0);
            layout->addWidget(context.tokenInput, 0, 1);
        }

        const QUrl tokenUrl = service->tokenUrl();
        if(!isCustom && tokenUrl.isValid()) {
            auto* tokenHintLabel = new QLabel(u"🛈 "_s + tr("You can find your token here") + ": "_L1
                                                  + u"<a href=\"%1\">%1</a>"_s.arg(tokenUrl.toString()),
                                              this);
            tokenHintLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
            tokenHintLabel->setOpenExternalLinks(true);
            tokenHintLabel->setTextFormat(Qt::RichText);
            layout->addWidget(tokenHintLabel, 2, 0, 1, 2);
        }
    }

    m_serviceLayout->addWidget(context.groupCheck, m_serviceLayout->rowCount(), 0);

    m_serviceContext.emplace_back(context);
    updateServiceState(service);
}

void ScrobblerPageWidget::addCustomService()
{
    auto* dialog = new CustomServiceDialog(m_scrobbler, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QDialog::accepted, this, [this, dialog]() {
        const auto details = dialog->serviceDetails();
        if(auto* service = m_scrobbler->addCustomService(details)) {
            addService(service);
        }
    });

    dialog->show();
}

void ScrobblerPageWidget::editCustomService(ScrobblerService* service)
{
    auto* dialog = new CustomServiceDialog(service, m_scrobbler, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QDialog::accepted, this, [this, service, dialog]() {
        const auto details = dialog->serviceDetails();
        service->updateDetails(details);
        service->saveSession();
        updateServiceState(service);
    });

    dialog->show();
}

void ScrobblerPageWidget::deleteCustomService(ScrobblerService* service)
{
    const auto context = findContext(service);
    if(context != m_serviceContext.cend()) {
        m_pendingDelete.emplace_back(*context);
        context->groupCheck->hide();
        m_serviceContext.erase(context);
    }
}

std::vector<ScrobblerPageWidget::ServiceContext>::iterator ScrobblerPageWidget::findContext(ScrobblerService* service)
{
    return std::ranges::find_if(m_serviceContext,
                                [service](const auto& context) { return context.service == service; });
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
