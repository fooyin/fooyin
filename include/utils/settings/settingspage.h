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

#include "fyutils_export.h"

#include "utils/id.h"

#include <QWidget>

namespace Fy::Utils {
class SettingsDialogController;

class FYUTILS_EXPORT SettingsPageWidget : public QWidget
{
    Q_OBJECT

public:
    virtual void apply() = 0;
    virtual void finish() { }
    virtual void reset() = 0;
};

class FYUTILS_EXPORT SettingsPage : public QObject
{
    Q_OBJECT

public:
    explicit SettingsPage(SettingsDialogController* controller = nullptr, QObject* parent = nullptr);

    SettingsPage(const SettingsPage&) = delete;

    [[nodiscard]] Id id() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] QStringList category() const;

    virtual QWidget* widget();
    virtual void apply();
    virtual void finish();
    virtual void reset();

protected:
    void setId(const Id& id);
    void setName(const QString& name);
    void setCategory(const QStringList& category);

    using WidgetCreator = std::function<SettingsPageWidget*()>;
    void setWidgetCreator(const WidgetCreator& widgetCreator);

private:
    Id m_id;
    QStringList m_category;
    QString m_name;
    QIcon m_categoryIcon;
    WidgetCreator m_widgetCreator;
    QWidget* m_widget;
};
} // namespace Fy::Utils
