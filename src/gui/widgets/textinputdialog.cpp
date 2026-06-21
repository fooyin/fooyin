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
 */

#include <gui/widgets/textinputdialog.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>

namespace Fooyin {
TextInputDialog::TextInputDialog(QWidget* parent)
    : QDialog{parent}
    , m_label{new QLabel(this)}
    , m_input{new QLineEdit(this)}
    , m_errorLabel{new QLabel(this)}
    , m_acceptButton{nullptr}
{
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_acceptButton  = buttonBox->button(QDialogButtonBox::Ok);

    m_label->setBuddy(m_input);
    m_errorLabel->setWordWrap(true);

    QPalette errorPalette{m_errorLabel->palette()};
    errorPalette.setColor(QPalette::WindowText, Qt::red);
    m_errorLabel->setPalette(errorPalette);
    m_errorLabel->hide();

    auto* layout = new QGridLayout(this);
    layout->addWidget(m_label, 0, 0, 1, 2);
    layout->addWidget(m_input, 1, 0, 1, 2);
    layout->addWidget(m_errorLabel, 2, 0);
    layout->addWidget(buttonBox, 2, 1);
    layout->setColumnStretch(0, 1);

    QObject::connect(m_input, &QLineEdit::textChanged, m_errorLabel, &QWidget::hide);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &TextInputDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setMinimumWidth(450);
}

TextInputDialog::TextInputDialog(const QString& title, const QString& label, QWidget* parent)
    : TextInputDialog{parent}
{
    setWindowTitle(title);
    setLabelText(label);
}

QString TextInputDialog::text() const
{
    return m_input->text();
}

void TextInputDialog::setText(const QString& text)
{
    m_input->setText(text);
    m_input->selectAll();
}

void TextInputDialog::setLabelText(const QString& text)
{
    m_label->setText(text);
}

void TextInputDialog::setPlaceholderText(const QString& text)
{
    m_input->setPlaceholderText(text);
}

void TextInputDialog::setEchoMode(QLineEdit::EchoMode mode)
{
    m_input->setEchoMode(mode);
}

void TextInputDialog::setAcceptButtonText(const QString& text)
{
    m_acceptButton->setText(text);
}

void TextInputDialog::setValidator(Validator validator)
{
    m_validator = std::move(validator);
}

void TextInputDialog::accept()
{
    const QString error = m_validator ? m_validator(text()) : QString{};
    if(!error.isEmpty()) {
        m_errorLabel->setText(error);
        m_errorLabel->show();
        m_input->setFocus();
        return;
    }

    QDialog::accept();
}
} // namespace Fooyin
