/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <gui/dsp/dspsettingsdialog.h>

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>

namespace Fooyin {
DspSettingsDialog::DspSettingsDialog(QWidget* parent)
    : QDialog{parent}
    , m_mainLayout{new QVBoxLayout(this)}
    , m_contentLayout{new QVBoxLayout()}
    , m_restoreButtonBox{new QDialogButtonBox(QDialogButtonBox::RestoreDefaults, this)}
    , m_buttonBox{new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this)}
{
    m_mainLayout->setContentsMargins(8, 8, 8, 8);
    m_mainLayout->setSpacing(10);

    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(10);
    m_mainLayout->addLayout(m_contentLayout, 1);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(6);
    buttonRow->addWidget(m_restoreButtonBox);
    buttonRow->addStretch();
    buttonRow->addWidget(m_buttonBox);
    m_mainLayout->addLayout(buttonRow);

    QObject::connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(m_restoreButtonBox, &QDialogButtonBox::clicked, this,
                     [this](QAbstractButton*) { restoreDefaults(); });
}

DspSettingsDialog::~DspSettingsDialog() = default;

QVBoxLayout* DspSettingsDialog::contentLayout() const
{
    return m_contentLayout;
}
} // namespace Fooyin
