/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

namespace Fy::Gui {
class PropertiesTab
{
public:
    using WidgetBuilder = std::function<QWidget*()>;

    PropertiesTab(QString title, WidgetBuilder widgetBuilder, int index = -1);
    virtual ~PropertiesTab() = default;

    [[nodiscard]] int index() const;
    [[nodiscard]] QString title() const;
    QWidget* widget();
    [[nodiscard]] bool hasVisited() const;

    void updateIndex(int index);
    void setVisited(bool visited);

    virtual void apply();
    virtual void finish();

private:
    int m_index;
    QString m_title;
    WidgetBuilder m_widgetBuilder;
    QWidget* m_widget{nullptr};
    bool m_visited{false};
};

class PropertiesDialog : public QObject
{
    Q_OBJECT

public:
    using TabList       = std::vector<PropertiesTab>;
    using WidgetBuilder = std::function<QWidget*()>;

    explicit PropertiesDialog(QObject* parent = nullptr);
    ~PropertiesDialog() override;

    void addTab(const QString& title, const WidgetBuilder& widgetBuilder);
    void addTab(const PropertiesTab& tab);
    void insertTab(int index, const QString& title, const WidgetBuilder& widgetBuilder);

    void show();

signals:
    void apply();

private:
    void done();
    void accept();
    void reject();

    TabList m_tabs;
};
} // namespace Fy::Gui
