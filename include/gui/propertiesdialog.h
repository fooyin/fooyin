/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/track.h>
#include <gui/fywidget.h>

#include <QPointer>

namespace Fooyin {
class SettingsManager;

class FYGUI_EXPORT PropertiesTabWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit PropertiesTabWidget(QWidget* parent)
        : FyWidget{parent}
    { }

    [[nodiscard]] virtual bool canApply() const;
    [[nodiscard]] virtual bool hasTools() const;

    virtual void apply();
    virtual void addTools(QMenu* menu);
};

using WidgetBuilder = std::function<PropertiesTabWidget*(const TrackList& tracks)>;

class PropertiesTab
{
public:
    PropertiesTab(QString title, WidgetBuilder widgetBuilder, int index = -1);
    virtual ~PropertiesTab() = default;

    [[nodiscard]] int index() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] PropertiesTabWidget* widget(const TrackList& tracks) const;
    [[nodiscard]] bool hasVisited() const;

    void updateIndex(int index);
    void setVisited(bool visited);

    virtual void apply();
    virtual void finish();

private:
    int m_index;
    QString m_title;
    WidgetBuilder m_widgetBuilder;
    mutable QPointer<PropertiesTabWidget> m_widget;
    bool m_visited;
};

class FYGUI_EXPORT PropertiesDialog : public QObject
{
    Q_OBJECT

public:
    using TabList = std::vector<PropertiesTab>;

    explicit PropertiesDialog(SettingsManager* settings, QObject* parent = nullptr);
    ~PropertiesDialog() override;

    void addTab(const QString& title, const WidgetBuilder& widgetBuilder);
    void addTab(const PropertiesTab& tab);
    void insertTab(int index, const QString& title, const WidgetBuilder& widgetBuilder);

    void show(const TrackList& tracks);

private:
    SettingsManager* m_settings;

    TabList m_tabs;
};
} // namespace Fooyin
