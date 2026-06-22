/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <gui/fylayout.h>

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace Fooyin {
class LayoutProvider;

class ImportLayoutDialog : public QDialog
{
    Q_OBJECT

public:
    ImportLayoutDialog(FyLayout layout, const QString& path, LayoutProvider* layoutProvider, QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] FyLayout selectedLayout() const;
    [[nodiscard]] bool switchToLayout() const;

    void accept() override;

private:
    bool loadLayout(const QString& path, bool updateName);
    void browse();
    void updateImportButton();
    void updateOptions();

    FyLayout m_layout;
    LayoutProvider* m_layoutProvider;
    QString m_loadedPath;
    QLineEdit* m_nameEdit;
    QLineEdit* m_pathEdit;
    QCheckBox* m_importColours;
    QCheckBox* m_importFonts;
    QCheckBox* m_importWindowSize;
    QCheckBox* m_switchToLayout;
    QLabel* m_errorLabel;
    QPushButton* m_importButton;
};
} // namespace Fooyin
