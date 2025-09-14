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

#pragma once

#include "services/servicedetails.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;

namespace Fooyin::Scrobbler {
class Scrobbler;
class ScrobblerService;

class CustomServiceDialog : public QDialog
{
    Q_OBJECT

public:
    CustomServiceDialog(ScrobblerService* service, Scrobbler* scrobbler, QWidget* parent = nullptr);
    explicit CustomServiceDialog(Scrobbler* scrobbler, QWidget* parent = nullptr);

    [[nodiscard]] ServiceDetails serviceDetails() const;

    void accept() override;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

private:
    void testService();
    void updateStatus(bool success, const QString& message = {});
    [[nodiscard]] bool hasUniqueName() const;
    [[nodiscard]] bool hasValidDetails() const;

    bool m_editing;

    Scrobbler* m_scrobbler;
    ScrobblerService* m_service;
    std::unique_ptr<ScrobblerService> m_testService;

    QComboBox* m_type;
    QLineEdit* m_name;
    QLineEdit* m_url;
    QLineEdit* m_token;
    QLabel* m_status;
};
} // namespace Fooyin::Scrobbler
