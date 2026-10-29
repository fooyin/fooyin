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

#include <gui/scripting/scripteditor.h>
#include <gui/widgets/scriptlineedit.h>

#include <QAction>
#include <QCompleter>
#include <QInputDialog>
#include <QLineEdit>
#include <QSignalBlocker>

using namespace Qt::StringLiterals;

constexpr auto SourceRole         = Qt::UserRole + 1;
constexpr auto MetadataActionRole = Qt::UserRole + 2;

namespace Fooyin {
AutoPlaylistFieldComboBox::AutoPlaylistFieldComboBox(QWidget* parent)
    : AutoPlaylistFieldComboBox{false, parent}
{ }

AutoPlaylistFieldComboBox::AutoPlaylistFieldComboBox(bool allowScripts, QWidget* parent)
    : QComboBox{parent}
    , m_allowScripts{allowScripts}
    , m_showingPresetName{false}
    , m_previousSource{AutoPlaylistFieldSource::BuiltIn}
{
    setEditable(true);
    setInsertPolicy(NoInsert);

    for(const auto& field : AutoPlaylistQueryBuilder::fields()) {
        addItem(field.label, m_allowScripts
                                 ? AutoPlaylistQueryBuilder::fieldScript(field.name, AutoPlaylistFieldSource::BuiltIn)
                                 : field.name);
    }

    insertSeparator(count());
    addItem(tr("Metadata tag…"));
    setItemData(count() - 1, true, MetadataActionRole);

    if(m_allowScripts) {
        m_scriptEdit = new ScriptLineEdit(this);
        m_scriptEdit->setClearButtonEnabled(true);
        m_scriptEdit->setPlaceholderText(tr("None"));

        setLineEdit(m_scriptEdit);
        setCompleter(nullptr);

        QObject::disconnect(m_scriptEdit, &QLineEdit::editingFinished, this, nullptr);

        const auto actions = m_scriptEdit->actions();
        for(const auto* action : actions) {
            QObject::disconnect(action, &QAction::triggered, m_scriptEdit, nullptr);
            QObject::connect(action, &QAction::triggered, this, [this]() {
                const QString original{field()};
                ScriptEditor::openEditor(
                    original,
                    [this, original](const QString& script) {
                        if(script != original) {
                            setScript(script, false);
                            Q_EMIT fieldChanged();
                        }
                    },
                    {}, this);
            });
        }

        setToolTip(tr("Choose a preset or enter a sorting script. Use ‘Metadata tag…’ to read a tag directly.\n"
                      "Clear to disable sorting."));
        m_showingPresetName = true;
    }
    else {
        completer()->setCompletionMode(QCompleter::PopupCompletion);
        lineEdit()->setPlaceholderText(tr("Tag name"));
        setToolTip(tr("Choose a field or enter a tag name. Use ‘Metadata tag…’ to read a tag directly."));
    }

    m_previousField = field();

    const auto updateField = [this]() {
        if(currentData(MetadataActionRole).toBool() && currentText() == itemText(currentIndex())) {
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
    QObject::connect(this, &QComboBox::editTextChanged, this, [this, updateField]() {
        if(m_allowScripts && !currentData(MetadataActionRole).toBool()) {
            m_showingPresetName = false;
        }
        updateField();
    });
    QObject::connect(this, &QComboBox::currentIndexChanged, this, [this, updateField]() {
        if(m_allowScripts && !currentData(MetadataActionRole).toBool()) {
            m_showingPresetName = true;
        }
        updateField();
    });
    QObject::connect(this, &QComboBox::activated, this, [this](int index) {
        if(!itemData(index, MetadataActionRole).toBool()) {
            if(m_allowScripts) {
                setScript(itemData(index).toString(), true);
                Q_EMIT fieldChanged();
            }
            return;
        }

        bool accepted{false};
        const bool previousPreset{m_showingPresetName};
        const QString previousTag = m_previousSource == AutoPlaylistFieldSource::Script ? QString{} : m_previousField;
        const QString name = QInputDialog::getText(this, tr("Metadata tag"), tr("Tag name") + u":"_s, QLineEdit::Normal,
                                                   previousTag, &accepted)
                                 .trimmed();
        if(accepted && !name.isEmpty()) {
            setField(name, AutoPlaylistFieldSource::Metadata);
            Q_EMIT fieldChanged();
        }
        else {
            if(m_allowScripts) {
                setScript(m_previousField, previousPreset);
            }
            else {
                setField(m_previousField, m_previousSource);
            }
        }
    });
}

QString AutoPlaylistFieldComboBox::field() const
{
    if(m_allowScripts) {
        return m_showingPresetName ? currentData().toString() : currentText();
    }
    const QString text = currentText().trimmed();
    if(currentIndex() >= 0 && text == itemText(currentIndex())) {
        return currentData().toString();
    }
    const int index = AutoPlaylistQueryBuilder::fieldIndex(text);
    return index >= 0 ? AutoPlaylistQueryBuilder::fields()[index].name : text;
}

AutoPlaylistFieldSource AutoPlaylistFieldComboBox::source() const
{
    if(m_allowScripts) {
        return AutoPlaylistFieldSource::Script;
    }
    if(currentIndex() >= 0 && currentText() == itemText(currentIndex())) {
        return static_cast<AutoPlaylistFieldSource>(currentData(SourceRole).toInt());
    }
    return AutoPlaylistQueryBuilder::fieldIndex(currentText().trimmed()) >= 0 ? AutoPlaylistFieldSource::BuiltIn
                                                                              : AutoPlaylistFieldSource::Metadata;
}

void AutoPlaylistFieldComboBox::setField(const QString& name, AutoPlaylistFieldSource source)
{
    const QSignalBlocker blocker{this};

    if(m_allowScripts) {
        const QString script = AutoPlaylistQueryBuilder::fieldScript(name, source);
        if(source == AutoPlaylistFieldSource::Metadata && findData(script) < 0) {
            //: %1 is a metadata tag name.
            insertItem(findData(true, MetadataActionRole), tr("%1 (tag)").arg(name), script);
        }
        setScript(script, true);
        return;
    }

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

void AutoPlaylistFieldComboBox::setScript(const QString& script, bool matchPreset)
{
    const QSignalBlocker blocker{this};

    const int index     = matchPreset ? findData(script) : -1;
    m_showingPresetName = index >= 0;
    setCurrentIndex(index);

    if(!m_showingPresetName && m_scriptEdit) {
        m_scriptEdit->setScript(script);
    }
    m_previousField  = script;
    m_previousSource = AutoPlaylistFieldSource::Script;
}
} // namespace Fooyin
