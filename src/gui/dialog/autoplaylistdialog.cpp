/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "autoplaylistdialog.h"

#include <core/coresettings.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <gui/widgets/scriptlineedit.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

using namespace Qt::StringLiterals;

constexpr auto SavedQueries = "Playlist/AutoPlaylistQueries";

namespace Fooyin {
AutoPlaylistDialog::AutoPlaylistDialog(QWidget* parent)
    : AutoPlaylistDialog{nullptr, nullptr, parent}
{ }

AutoPlaylistDialog::AutoPlaylistDialog(PlaylistHandler* playlisthandler, Playlist* playlist, QWidget* parent)
    : QDialog{parent}
    , m_playlistHandler{playlisthandler}
    , m_playlist{playlist}
    , m_queryChanged{false}
    , m_queryBox{new QComboBox(this)}
    , m_queryEdit{new ScriptTextEdit(this)}
    , m_loadButton{new QPushButton(tr("&Load"), this)}
    , m_saveButton{new QPushButton(tr("&Save"), this)}
    , m_deleteButton{new QPushButton(tr("&Delete"), this)}
    , m_resetButton{new QPushButton(tr("&Restore Defaults"), this)}
{
    setWindowTitle(m_playlist ? tr("Edit Autoplaylist") : tr("Create New Autoplaylist"));
    setModal(true);

    m_queryBox->setEditable(true);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_okButton      = buttonBox->button(QDialogButtonBox::Ok);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* buttonLayout = new QGridLayout();

    int row{0};
    buttonLayout->addWidget(m_loadButton, row++, 0);
    buttonLayout->addWidget(m_saveButton, row++, 0);
    buttonLayout->addWidget(m_deleteButton, row++, 0);
    buttonLayout->addWidget(m_resetButton, row++, 0);
    buttonLayout->setRowStretch(buttonLayout->rowCount(), 1);

    QObject::connect(m_loadButton, &QAbstractButton::clicked, this, &AutoPlaylistDialog::loadQuery);
    QObject::connect(m_saveButton, &QAbstractButton::clicked, this, &AutoPlaylistDialog::saveQuery);
    QObject::connect(m_deleteButton, &QAbstractButton::clicked, this, &AutoPlaylistDialog::deleteQuery);
    QObject::connect(m_resetButton, &QAbstractButton::clicked, this, &AutoPlaylistDialog::resetQueries);
    QObject::connect(m_queryBox, &QComboBox::editTextChanged, this, &AutoPlaylistDialog::updateButtonState);
    QObject::connect(m_queryEdit, &QPlainTextEdit::textChanged, this, &AutoPlaylistDialog::updateButtonState);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addLayout(buttonLayout, row, 2, 2, 1);
    layout->addWidget(new QLabel(tr("Name") + ":"_L1, this), row, 0);
    layout->addWidget(m_queryBox, row++, 1);
    layout->addWidget(new QLabel(tr("Query") + ":"_L1, this), row, 0, Qt::AlignTop);
    layout->addWidget(m_queryEdit, row++, 1);
    layout->addWidget(buttonBox, row++, 0, 1, 3);
    layout->setColumnStretch(1, 1);

    loadQueries();
    m_queryBox->setEditText({});

    if(m_playlist && m_playlist->isAutoPlaylist()) {
        m_queryBox->setEditText(m_playlist->name());
        m_queryEdit->setText(m_playlist->query());
    }

    updateButtonState();
    setMinimumWidth(600);
}

void AutoPlaylistDialog::done(int value)
{
    saveQueries();
    QDialog::done(value);
}

void AutoPlaylistDialog::accept()
{
    const QString name = m_queryBox->currentText();

    if(m_playlistHandler && m_playlist) {
        m_playlistHandler->createAutoPlaylist(m_playlist->name(), m_queryEdit->text());
        if(name != m_playlist->name()) {
            m_playlistHandler->renamePlaylist(m_playlist->id(), name);
        }
    }

    emit playlistEdited(name, m_queryEdit->text());
    QDialog::accept();
}

void AutoPlaylistDialog::updateButtonState() const
{
    const QString currentName = m_queryBox->currentText();
    const bool canSave        = !currentName.isEmpty() && !m_queryEdit->text().isEmpty();

    m_loadButton->setEnabled(m_queryBox->findText(currentName) >= 0);
    m_saveButton->setEnabled(canSave);
    m_deleteButton->setEnabled(m_queryBox->findText(currentName) >= 0);
    m_resetButton->setEnabled(m_queryChanged);
    m_okButton->setEnabled(canSave);
}

AutoPlaylistQuery AutoPlaylistDialog::currentQuery() const
{
    AutoPlaylistQuery query;
    query.name  = m_queryBox->currentText();
    query.query = m_queryEdit->text();
    return query;
}

std::vector<AutoPlaylistQuery>::iterator AutoPlaylistDialog::findQuery(const QString& name)
{
    return std::ranges::find_if(m_queries, [&name](const auto& query) { return query.name == name; });
}

void AutoPlaylistDialog::saveQuery()
{
    const QString name = m_queryBox->currentText();

    const AutoPlaylistQuery query = currentQuery();

    auto existingQuery = findQuery(name);
    if(existingQuery != m_queries.cend()) {
        QMessageBox msg{QMessageBox::Question, tr("Query already exists"),
                        tr("Query %1 already exists. Overwrite?").arg(name), QMessageBox::Yes | QMessageBox::No};
        if(msg.exec() == QMessageBox::Yes) {
            *existingQuery = query;
            m_queryBox->setCurrentIndex(m_queryBox->findText(name));
        }
    }
    else {
        m_queries.push_back(query);
        m_queryBox->addItem(query.name);
        m_queryBox->setCurrentIndex(m_queryBox->count() - 1);
    }

    m_queryChanged = true;
    updateButtonState();
}

void AutoPlaylistDialog::loadQuery()
{
    if(m_queryBox->count() == 0) {
        return;
    }

    const QString name = m_queryBox->currentText();

    auto query = findQuery(name);
    if(query != m_queries.cend()) {
        m_queryBox->setEditText(query->name);
        m_queryEdit->setText(query->query);
    }
}

void AutoPlaylistDialog::deleteQuery()
{
    if(m_queryBox->count() == 0) {
        return;
    }

    const QString name = m_queryBox->currentText();

    auto query = findQuery(name);
    if(query != m_queries.cend()) {
        m_queries.erase(query);
        m_queryBox->removeItem(m_queryBox->findText(name));
    }
}

void AutoPlaylistDialog::resetQueries()
{
    FySettings settings;
    settings.remove(QLatin1String{SavedQueries});
    m_queries.clear();
    m_queryChanged = false;
    populateQueries();
    m_queryBox->setEditText({});
}

void AutoPlaylistDialog::saveQueries() const
{
    if(!m_queryChanged) {
        return;
    }

    QByteArray byteArray;
    QDataStream stream(&byteArray, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<qint32>(m_queries.size());

    for(const auto& query : m_queries) {
        stream << query;
    }

    FySettings settings;
    settings.setValue(SavedQueries, byteArray);
}

void AutoPlaylistDialog::loadQueries()
{
    const FySettings settings;
    QByteArray byteArray = settings.value(SavedQueries).toByteArray();

    if(!byteArray.isEmpty()) {
        QDataStream stream(&byteArray, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_6_0);

        qint32 size;
        stream >> size;

        while(size > 0) {
            --size;

            AutoPlaylistQuery query;
            stream >> query;

            m_queries.push_back(query);
        }

        m_queryChanged = true;
    }

    populateQueries();
}

void AutoPlaylistDialog::populateQueries()
{
    m_queryBox->clear();

    if(m_queries.empty()) {
        loadDefaultQueries();
    }

    for(const auto& query : m_queries) {
        m_queryBox->addItem(query.name);
    }
}

void AutoPlaylistDialog::loadDefaultQueries()
{
    m_queries = {{tr("Most Played"), u"playcount>0 LIMIT 25 SORT- playcount"_s},
                 {tr("Recently Added"), u"addedtime DURING LAST 2 WEEKS SORT- addedtime"_s},
                 {tr("Last Played 2 Weeks"), u"lastplayed DURING LAST 2 WEEKS SORT- lastplayed"_s},
                 {tr("Has Lyrics"), u"lyrics PRESENT"_s}};
}
} // namespace Fooyin

#include "moc_autoplaylistdialog.cpp"
