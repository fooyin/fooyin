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

#pragma once

#include "fygui_export.h"

#include <QWidget>

class QJsonObject;
class QMenu;

namespace Fooyin {
class FYGUI_EXPORT DspLayoutEditor : public QWidget
{
    Q_OBJECT

public:
    explicit DspLayoutEditor(QWidget* parent = nullptr);
    ~DspLayoutEditor() override;

    [[nodiscard]] virtual QByteArray saveSettings() const = 0;
    virtual void loadSettings(const QByteArray& settings) = 0;

    virtual void setControlsEnabled(bool enabled);
    virtual void restoreDefaults();
    virtual void populateContextMenu(QMenu* menu);
    virtual void saveLayoutData(QJsonObject& layout);
    virtual void loadLayoutData(const QJsonObject& layout);

Q_SIGNALS:
    void previewSettingsChanged(const QByteArray& settings);
};
} // namespace Fooyin
