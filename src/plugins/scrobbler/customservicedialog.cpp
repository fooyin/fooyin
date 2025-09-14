/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include "customservicedialog.h"

#include "scrobbler.h"
#include "services/scrobblerservice.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

using namespace Qt::StringLiterals;

namespace Fooyin::Scrobbler {
CustomServiceDialog::CustomServiceDialog(ScrobblerService* service, Scrobbler* scrobbler, QWidget* parent)
    : QDialog{parent}
    , m_editing{service != nullptr}
    , m_scrobbler{scrobbler}
    , m_service{service}
    , m_type{new QComboBox(this)}
    , m_name{new QLineEdit(this)}
    , m_url{new QLineEdit(this)}
    , m_token{new QLineEdit(this)}
    , m_status{new QLabel(this)}
{
    setWindowTitle(tr("Edit Scrobbling Service"));

    auto* typeLabel  = new QLabel(tr("Type") + u":", this);
    auto* nameLabel  = new QLabel(tr("Name") + u":", this);
    auto* urlLabel   = new QLabel(tr("URL") + u":", this);
    auto* tokenLabel = new QLabel(tr("Token") + u":", this);

    // Only ListenBrainz supported for now
    m_type->addItem(u"ListenBrainz"_s, QVariant::fromValue(ServiceDetails::CustomType::ListenBrainz));
    // m_type->addItem(u"AudioScrobbler"_s, QVariant::fromValue(ServiceDetails::CustomType::AudioScrobbler));
    m_type->setEnabled(false);

    const auto updateButtonState = [this](QPushButton* button) {
        const bool unique = hasUniqueName();
        if(!unique) {
            updateStatus(false, tr("A service with that name already exists"));
        }
        else {
            updateStatus(true);
        }

        button->setEnabled(unique && hasValidDetails());
    };

    auto* buttonBox
        = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if(auto* okButton = buttonBox->button(QDialogButtonBox::Ok)) {
        updateButtonState(okButton);
        for(const auto& control : {m_name, m_url, m_token}) {
            QObject::connect(control, &QLineEdit::textChanged, this,
                             [updateButtonState, okButton]() { updateButtonState(okButton); });
        }
    }
    if(auto* applyButton = buttonBox->button(QDialogButtonBox::Apply)) {
        applyButton->setText(tr("Test"));
        updateButtonState(applyButton);
        for(const auto& control : {m_name, m_url, m_token}) {
            QObject::connect(control, &QLineEdit::textChanged, this,
                             [updateButtonState, applyButton]() { updateButtonState(applyButton); });
        }
        QObject::connect(applyButton, &QPushButton::clicked, this, &CustomServiceDialog::testService);
    }

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_status->setWordWrap(true);

    auto* layout = new QGridLayout(this);

    int row{0};
    layout->addWidget(nameLabel, row, 0);
    layout->addWidget(m_name, row++, 1);
    layout->addWidget(typeLabel, row, 0);
    layout->addWidget(m_type, row++, 1);
    layout->addWidget(urlLabel, row, 0);
    layout->addWidget(m_url, row++, 1);
    layout->addWidget(tokenLabel, row, 0);
    layout->addWidget(m_token, row++, 1);
    layout->addWidget(m_status, row++, 0, 2, 2);
    layout->setRowStretch(row++, 1);
    layout->addWidget(buttonBox, row, 1, 1, 1, Qt::AlignRight);

    if(service) {
        const auto details = service->details();
        // m_type->setCurrentText(details.customType == ServiceDetails::CustomType::AudioScrobbler ? u"AudioScrobbler"_s
        //                                                                                         : u"ListenBrainz"_s);
        m_name->setText(details.name);
        m_url->setText(details.url.toDisplayString());
        m_token->setText(details.token);
    }

    resize(CustomServiceDialog::sizeHint());
}

CustomServiceDialog::CustomServiceDialog(Scrobbler* scrobbler, QWidget* parent)
    : CustomServiceDialog{nullptr, scrobbler, parent}
{
    setWindowTitle(tr("Add Scrobbling Service"));
}

ServiceDetails CustomServiceDialog::serviceDetails() const
{
    return {.name       = m_name->text(),
            .url        = QUrl{m_url->text()}.adjusted(QUrl::StripTrailingSlash),
            .token      = m_token->text(),
            .customType = static_cast<ServiceDetails::CustomType>(m_type->currentData().toInt()),
            .isEnabled  = true};
}

void CustomServiceDialog::accept()
{
    QDialog::accept();
}

QSize CustomServiceDialog::sizeHint() const
{
    auto size = m_type->sizeHint();
    size.rheight() += 200;
    size.rwidth() += 500;
    return size;
}

QSize CustomServiceDialog::minimumSizeHint() const
{
    auto size = m_type->minimumSizeHint();
    size.rheight() += 200;
    size.rwidth() += 500;
    return size;
}

void CustomServiceDialog::testService()
{
    if(m_testService) {
        m_testService->updateDetails(serviceDetails());
    }
    else {
        m_testService = m_scrobbler->createCustomService(serviceDetails());
        QObject::connect(m_testService.get(), &ScrobblerService::testApiFinished, this,
                         [this](bool success, const QString& error) {
                             updateStatus(success, success ? tr("Token authenticated successfully") : error);
                         });
    }

    m_testService->testApi();
}

void CustomServiceDialog::updateStatus(bool success, const QString& message)
{
    QPalette palette = m_status->palette();

    if(success) {
        palette.setColor(QPalette::WindowText, Qt::green);
    }
    else {
        palette.setColor(QPalette::WindowText, Qt::red);
    }

    m_status->setText(message);
    m_status->setPalette(palette);
}

bool CustomServiceDialog::hasUniqueName() const
{
    auto* existingService = m_scrobbler->service(m_name->text());
    return (m_editing && m_service == existingService) || !existingService;
}

bool CustomServiceDialog::hasValidDetails() const
{
    return !m_name->text().isEmpty() && !m_url->text().isEmpty() && !m_token->text().isEmpty();
}
} // namespace Fooyin::Scrobbler

#include "moc_customservicedialog.cpp"
