/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include <gui/configdialog.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPushButton>

namespace Fooyin {
ConfigDialog::ConfigDialog(const QString& title, QWidget* parent)
    : QDialog{parent}
    , m_contentLayout{new QGridLayout()}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(title);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(m_contentLayout);

    auto* defaultsButtons = new QDialogButtonBox(this);
    auto* saveDefaultsButton
        = defaultsButtons->addButton(tr("Save as default"), QDialogButtonBox::ButtonRole::ActionRole);
    auto* resetDefaultsButton
        = defaultsButtons->addButton(tr("Reset to default"), QDialogButtonBox::ButtonRole::ActionRole);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Apply
                                             | QDialogButtonBox::StandardButton::Cancel,
                                         this);

    if(auto* okButton = buttons->button(QDialogButtonBox::StandardButton::Ok)) {
        okButton->setDefault(true);
    }

    QObject::connect(saveDefaultsButton, &QPushButton::clicked, this, &ConfigDialog::saveDefaults);
    QObject::connect(resetDefaultsButton, &QPushButton::clicked, this, &ConfigDialog::restoreDefaults);
    QObject::connect(buttons->button(QDialogButtonBox::StandardButton::Apply), &QPushButton::clicked, this,
                     &ConfigDialog::apply);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        apply();
        accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* footerLayout = new QHBoxLayout();
    footerLayout->addWidget(defaultsButtons);
    footerLayout->addStretch();
    footerLayout->addWidget(buttons);

    mainLayout->addLayout(footerLayout);
}

QGridLayout* ConfigDialog::contentLayout() const
{
    return m_contentLayout;
}
} // namespace Fooyin
