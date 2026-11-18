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

#include "appearancepropertymodel.h"

#include <QItemSelectionModel>
#include <QJsonObject>
#include <QPalette>
#include <QPersistentModelIndex>
#include <QWidget>

#include <vector>

class QVBoxLayout;

namespace Fooyin {
class AppearancePropertyModel;
class AppearancePropertyPanel;

class AppearancePropertyPage : public QWidget
{
    Q_OBJECT

public:
    explicit AppearancePropertyPage(AppearancePropertyModel* model, QWidget* parent = nullptr);

    AppearanceProperty* addProperty(AppearanceProperty property);
    void addHint(QWidget* hint);

    void setOverrides(const QJsonObject& overrides);

    [[nodiscard]] bool hasOverrides() const;
    void resetOverrides();

    [[nodiscard]] QModelIndex rootIndex() const;
    [[nodiscard]] bool hasHints() const;

Q_SIGNALS:
    void propertiesEdited(const std::vector<Fooyin::AppearancePropertyEdit>& edits);

private:
    friend class AppearancePropertyPanel;

    struct ViewState
    {
        std::vector<QPersistentModelIndex> expanded;
        QPersistentModelIndex current;
        QItemSelection selection;
        int horizontalScroll{0};
        int verticalScroll{0};
    };

    AppearancePropertyModel* m_model;
    QPersistentModelIndex m_root;
    QVBoxLayout* m_layout;
    ViewState m_viewState;
};
} // namespace Fooyin
