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

#include "autoplaylistfieldcombobox.h"

#include <QCompleter>
#include <QInputDialog>
#include <QLineEdit>
#include <QSignalBlocker>

using namespace Qt::StringLiterals;

constexpr auto SourceRole         = Qt::UserRole + 1;
constexpr auto MetadataActionRole = Qt::UserRole + 2;

namespace Fooyin {
AutoPlaylistFieldComboBox::AutoPlaylistFieldComboBox(QWidget* parent)
    : QComboBox{parent}
    , m_previousSource{AutoPlaylistFieldSource::BuiltIn}
{
    for(const auto& field : AutoPlaylistQueryBuilder::fields()) {
        addItem(field.label, field.name);
    }

    insertSeparator(count());
    addItem(tr("Metadata tag…"));
    setItemData(count() - 1, true, MetadataActionRole);

    setEditable(true);
    setInsertPolicy(NoInsert);
    completer()->setCompletionMode(QCompleter::PopupCompletion);
    lineEdit()->setPlaceholderText(tr("Tag name"));
    setToolTip(tr("Choose a field or enter a tag name. Use ‘Metadata tag…’ to read a tag directly."));

    m_previousField = field();

    const auto updateField = [this]() {
        if(currentText() == itemText(findData(true, MetadataActionRole))) {
            return;
        }

        const QString name     = field();
        const auto fieldSource = source();
        if(name == m_previousField && fieldSource == m_previousSource) {
            return;
        }

        m_previousField  = name;
        m_previousSource = fieldSource;
        Q_EMIT fieldChanged();
    };
    QObject::connect(this, &QComboBox::editTextChanged, this, updateField);
    QObject::connect(this, &QComboBox::currentIndexChanged, this, updateField);
    QObject::connect(this, &QComboBox::activated, this, [this](int index) {
        if(!itemData(index, MetadataActionRole).toBool()) {
            return;
        }

        bool accepted{false};
        const QString name = QInputDialog::getText(this, tr("Metadata tag"), tr("Tag name") + u":"_s, QLineEdit::Normal,
                                                   m_previousField, &accepted)
                                 .trimmed();
        if(accepted && !name.isEmpty()) {
            setField(name, AutoPlaylistFieldSource::Metadata);
            Q_EMIT fieldChanged();
        }
        else {
            setField(m_previousField, m_previousSource);
        }
    });
}

QString AutoPlaylistFieldComboBox::field() const
{
    const QString text = currentText().trimmed();
    if(currentIndex() >= 0 && text == itemText(currentIndex())) {
        return currentData().toString();
    }
    const int index = AutoPlaylistQueryBuilder::fieldIndex(text);
    return index >= 0 ? AutoPlaylistQueryBuilder::fields()[index].name : text;
}

AutoPlaylistFieldSource AutoPlaylistFieldComboBox::source() const
{
    if(currentIndex() >= 0 && currentText() == itemText(currentIndex())) {
        return static_cast<AutoPlaylistFieldSource>(currentData(SourceRole).toInt());
    }
    return AutoPlaylistQueryBuilder::fieldIndex(currentText().trimmed()) >= 0 ? AutoPlaylistFieldSource::BuiltIn
                                                                              : AutoPlaylistFieldSource::Metadata;
}

void AutoPlaylistFieldComboBox::setField(const QString& name, AutoPlaylistFieldSource source)
{
    const QSignalBlocker blocker{this};

    const int fieldIndex = AutoPlaylistQueryBuilder::fieldIndex(name, source);
    if(fieldIndex >= 0 || name.isEmpty()) {
        setCurrentIndex(findData(fieldIndex >= 0 ? AutoPlaylistQueryBuilder::fields()[fieldIndex].name : name));
        if(currentIndex() < 0) {
            setEditText(name);
        }
    }
    else {
        int index{-1};
        for(int i{0}; i < count(); ++i) {
            if(itemData(i).toString() == name
               && itemData(i, SourceRole).toInt() == static_cast<int>(AutoPlaylistFieldSource::Metadata)) {
                index = i;
                break;
            }
        }
        if(index < 0) {
            index = findData(true, MetadataActionRole);
            //: %1 is a metadata tag name.
            insertItem(index, tr("%1 (tag)").arg(name), name);
            setItemData(index, static_cast<int>(AutoPlaylistFieldSource::Metadata), SourceRole);
        }
        setCurrentIndex(index);
    }

    m_previousField  = field();
    m_previousSource = this->source();
}
} // namespace Fooyin
