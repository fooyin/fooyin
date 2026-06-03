/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <QByteArray>
#include <QIcon>
#include <QWidget>

namespace Fooyin {
class SettingsDialogController;

enum class SettingsPagePosition : uint8_t
{
    Default = 0,
    First,
    Last,
};

enum class SettingsPageRelativePosition : uint8_t
{
    None = 0,
    Before,
    After,
};

class FYUTILS_EXPORT SettingsPageWidget : public QWidget
{
    Q_OBJECT

public:
    virtual void load()  = 0;
    virtual void apply() = 0;
    virtual void finish() { }
    virtual void reset() = 0;

    [[nodiscard]] virtual QByteArray saveState() const
    {
        return {};
    }
    virtual void restoreState(const QByteArray& /*state*/) { }

    [[nodiscard]] virtual QString validationError() const
    {
        return {};
    }
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
    [[nodiscard]] SettingsPagePosition position() const;
    [[nodiscard]] SettingsPageRelativePosition relativePosition() const;
    [[nodiscard]] Id positionPage() const;

    QWidget* widget();

    void load();
    void apply();
    void finish();
    void reset();

    [[nodiscard]] QByteArray state() const;
    void setState(const QByteArray& state);

    [[nodiscard]] QString validationError() const;

protected:
    void setId(const Id& id);
    void setName(const QString& name);
    void setCategory(const QStringList& category);
    void setPosition(SettingsPagePosition position);
    void setRelativePosition(SettingsPageRelativePosition position, const Id& page);

    using WidgetCreator = std::function<SettingsPageWidget*()>;
    void setWidgetCreator(const WidgetCreator& widgetCreator);

private:
    Id m_id;
    QStringList m_category;
    QString m_name;
    SettingsPagePosition m_position;
    SettingsPageRelativePosition m_relativePosition;
    Id m_positionPage;
    QIcon m_categoryIcon;
    WidgetCreator m_widgetCreator;
    QWidget* m_widget;
    QByteArray m_state;
};
} // namespace Fooyin
