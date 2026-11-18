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

#include "appearancepropertypage.h"

#include <QVBoxLayout>

namespace Fooyin {
AppearancePropertyPage::AppearancePropertyPage(AppearancePropertyModel* model, QWidget* parent)
    : QWidget{parent}
    , m_model{model}
    , m_root{model->addPage()}
    , m_layout{new QVBoxLayout(this)}
{
    m_layout->setContentsMargins({});
    QObject::connect(m_model, &AppearancePropertyModel::propertiesEdited, this,
                     [this](const QModelIndex& page, const std::vector<AppearancePropertyEdit>& edits) {
                         if(page == m_root) {
                             Q_EMIT propertiesEdited(edits);
                         }
                     });
}

AppearanceProperty* AppearancePropertyPage::addProperty(AppearanceProperty property)
{
    return m_model->addProperty(m_root, std::move(property));
}

void AppearancePropertyPage::addHint(QWidget* hint)
{
    m_layout->addWidget(hint);
}

void AppearancePropertyPage::setOverrides(const QJsonObject& overrides)
{
    m_model->setOverrides(m_root, overrides);
}

bool AppearancePropertyPage::hasOverrides() const
{
    for(int row{0}; row < m_model->rowCount(m_root); ++row) {
        if(m_model->index(row, 1, m_root).data(AppearancePropertyModel::ResetRole).toBool()) {
            return true;
        }
    }
    return false;
}

void AppearancePropertyPage::resetOverrides()
{
    m_model->resetOverrides(m_root);
}

QModelIndex AppearancePropertyPage::rootIndex() const
{
    return m_root;
}

bool AppearancePropertyPage::hasHints() const
{
    return m_layout->count() > 0;
}
} // namespace Fooyin
