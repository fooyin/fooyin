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

#include "utils/id.h"

#include <QWidget>

namespace Fy::Utils {
class SettingsDialogController;

class SettingsPageWidget : public QWidget
{
    Q_OBJECT

public:
    virtual void apply() = 0;
    virtual void finish() { }
};

class SettingsPage : public QObject
{
    Q_OBJECT

public:
    explicit SettingsPage(SettingsDialogController* controller = nullptr, QObject* parent = nullptr);
    ~SettingsPage() override = default;

    SettingsPage(const SettingsPage&) = delete;

    [[nodiscard]] Id id() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] Id category() const;
    [[nodiscard]] QString categoryName() const;
    [[nodiscard]] QIcon categoryIcon() const;

    virtual QWidget* widget();
    virtual void apply();
    virtual void finish();

protected:
    void setId(const Id& id);
    void setName(const QString& name);
    void setCategory(const Id& category);
    void setCategoryName(const QString& name);
    void setCategoryIcon(const QIcon& icon);
    void setCategoryIconPath(const QString& iconPath);

    using WidgetCreator = std::function<SettingsPageWidget*()>;
    void setWidgetCreator(const WidgetCreator& widgetCreator);

private:
    Id m_id;
    Id m_category;
    QString m_name;
    QString m_categoryName;
    QIcon m_categoryIcon;
    WidgetCreator m_widgetCreator;
    QWidget* m_widget;
};
} // namespace Fy::Utils
