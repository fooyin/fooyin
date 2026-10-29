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

#include "tageditorautocompletedelegate.h"

#include "tageditoritem.h"

#include <gui/widgets/metadatacompleter.h>

#include <QLineEdit>
#include <QStringListModel>

namespace Fooyin::TagEditor {
TagEditorAutocompleteDelegate::TagEditorAutocompleteDelegate(QObject* parent)
    : QStyledItemDelegate{parent}
{
    for(size_t index{0}; index < TagEditorCompletionDomainCount; ++index) {
        m_models.at(index) = new QStringListModel(this);
    }
}

void TagEditorAutocompleteDelegate::setCompletionValues(const TagEditorCompletionValues& completionValues)
{
    for(size_t index{0}; index < TagEditorCompletionDomainCount; ++index) {
        modelForDomain(static_cast<StringPool::Domain>(index))->setStringList(completionValues.at(index));
    }
}

QWidget* TagEditorAutocompleteDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                                     const QModelIndex& index) const
{
    auto* editor = qobject_cast<QLineEdit*>(QStyledItemDelegate::createEditor(parent, option, index));
    if(!editor) {
        editor = new QLineEdit(parent);
    }

    const auto config = completionConfig(index);
    if(!config.enabled) {
        return editor;
    }

    Gui::setMetadataCompleter(editor, modelForDomain(config.domain)->stringList(), config.multivalue);

    return editor;
}

void TagEditorAutocompleteDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    QStyledItemDelegate::setEditorData(editor, index);

    if(auto* lineEdit = qobject_cast<QLineEdit*>(editor)) {
        lineEdit->selectAll();
    }
}

TagEditorAutocompleteDelegate::CompletionConfig
TagEditorAutocompleteDelegate::completionConfig(const QModelIndex& index)
{
    if(!index.isValid()) {
        return {};
    }

    if(index.column() == 0 && !index.data(TagEditorItem::IsDefault).toBool()) {
        return {.enabled = true, .multivalue = false, .domain = StringPool::Domain::ExtraTagKey};
    }

    if(index.column() != 1) {
        return {};
    }

    const QString field = index.siblingAtColumn(0).data(TagEditorItem::ScriptField).toString().toUpper();

    if(const auto domain = Gui::metadataCompletionDomain(field); domain.has_value()) {
        return {.enabled = true, .multivalue = Track::isMultiValueTag(field), .domain = *domain};
    }

    return {};
}

QStringListModel* TagEditorAutocompleteDelegate::modelForDomain(StringPool::Domain domain) const
{
    return m_models.at(static_cast<size_t>(domain));
}
} // namespace Fooyin::TagEditor
