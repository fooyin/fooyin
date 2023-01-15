/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include <QDialog>

class QVBoxLayout;
class QListWidget;
class QPushButton;

namespace Core {
class QuickSetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuickSetupDialog(QWidget* parent = nullptr);
    ~QuickSetupDialog() override;

signals:
    void layoutChanged(const QByteArray& layout);

protected:
    void setupUi();
    void setupList();
    void changeLayout();

    void showEvent(QShowEvent* event) override;

private:
    QVBoxLayout* m_layout;
    QListWidget* m_layoutList;
    QPushButton* m_accept;
};
} // namespace Core
