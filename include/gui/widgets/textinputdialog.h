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

#pragma once

#include "fygui_export.h"

#include <QDialog>
#include <QLineEdit>

#include <functional>
#include <utility>

class QLabel;
class QPushButton;

namespace Fooyin {
class FYGUI_EXPORT TextInputDialog : public QDialog
{
    Q_OBJECT

public:
    using Validator = std::function<QString(const QString&)>;

    explicit TextInputDialog(QWidget* parent = nullptr);
    TextInputDialog(const QString& title, const QString& label, QWidget* parent = nullptr);

    [[nodiscard]] QString text() const;

    void setText(const QString& text);
    void setLabelText(const QString& text);
    void setPlaceholderText(const QString& text);
    void setEchoMode(QLineEdit::EchoMode mode);
    void setAcceptButtonText(const QString& text);
    void setValidator(Validator validator);

    void accept() override;

private:
    QLabel* m_label;
    QLineEdit* m_input;
    QLabel* m_errorLabel;
    QPushButton* m_acceptButton;
    Validator m_validator;
};
} // namespace Fooyin
