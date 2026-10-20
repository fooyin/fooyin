/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "autoplaylistquery.h"
#include "autoplaylistruleswidget.h"

#include <core/coresettings.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/datastream.h>
#include <utils/utils.h>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTextCursor>

using namespace Qt::StringLiterals;

constexpr auto SavedQueries     = "Playlist/AutoPlaylistQueries"_L1;
constexpr auto DialogStateGroup = "AutoPlaylistDialog"_L1;

namespace Fooyin {
AutoPlaylistDialog::AutoPlaylistDialog(QWidget* parent)
    : AutoPlaylistDialog{nullptr, nullptr, parent}
{ }

AutoPlaylistDialog::AutoPlaylistDialog(PlaylistHandler* playlisthandler, Playlist* playlist, QWidget* parent)
    : QDialog{parent}
    , m_playlistHandler{playlisthandler}
    , m_playlist{playlist}
    , m_queryChanged{false}
    , m_haveRules{true}
    , m_nameEdit{new QLineEdit(this)}
    , m_presetBox{new QComboBox(this)}
    , m_modes{new QTabWidget(this)}
    , m_rules{new AutoPlaylistRulesWidget(this)}
    , m_errorLabel{new QLabel(this)}
    , m_modeLabel{new QLabel(this)}
    , m_queryEdit{new ScriptTextEdit(this)}
    , m_sortQueryEdit{new ScriptLineEdit(this)}
    , m_forceSorted{new QCheckBox(tr("Keep playlist sorted"), this)}
    , m_loadButton{new QPushButton(tr("&Load"), this)}
    , m_saveButton{new QPushButton(tr("&Save…"), this)}
    , m_deleteButton{new QPushButton(tr("&Delete"), this)}
    , m_resetButton{new QPushButton(tr("&Restore Defaults"), this)}
    , m_formatAction{new QAction(tr("&Format"), this)}
{
    setWindowTitle(m_playlist ? tr("Edit Autoplaylist") : tr("Create New Autoplaylist"));
    setModal(true);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_okButton      = buttonBox->button(QDialogButtonBox::Ok);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &AutoPlaylistDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* presetGroup  = new QGroupBox(tr("Presets"), this);
    auto* presetLayout = new QGridLayout(presetGroup);

    int row{0};
    presetLayout->addWidget(m_presetBox, row++, 0, 1, 3);
    presetLayout->addWidget(m_loadButton, row, 0);
    presetLayout->addWidget(m_saveButton, row, 1);
    presetLayout->addWidget(m_deleteButton, row++, 2);
    presetLayout->addWidget(m_resetButton, row++, 0, 1, 3);

    m_forceSorted->setToolTip(
        tr("When enabled, the autoplaylist is fully reordered by its sort pattern whenever it regenerates.\n"
           "When disabled, existing track order is preserved and only newly added matching tracks are sorted and then "
           "appended."));

    auto* details = new QFormLayout();
    details->addRow(tr("Name:"), m_nameEdit);
    details->addRow(m_forceSorted);
    details->addRow(m_rules->randomWidget());
    details->addRow(m_rules->limitWidget());

    auto* headerLayout = new QHBoxLayout();
    headerLayout->addLayout(details, 1);
    headerLayout->addWidget(presetGroup, 0);

    m_sortQueryEdit->setPlaceholderText(u"SORT BY %artist%"_s);

    m_modeLabel->setWordWrap(true);
    m_modeLabel->setTextFormat(Qt::PlainText);
    m_modeLabel->hide();

    auto* scriptPage   = new QWidget(this);
    auto* scriptLayout = new QFormLayout(scriptPage);
    scriptLayout->addRow(tr("Query:"), m_queryEdit);
    scriptLayout->addRow(tr("Sort:"), m_sortQueryEdit);

    m_formatAction->setToolTip(tr("Format the query with line breaks and indentation."));
    m_queryEdit->addToolAction(m_formatAction);

    m_modes->addTab(m_rules, tr("Rules"));
    m_modes->addTab(scriptPage, tr("Script"));

    m_errorLabel->setWordWrap(true);
    m_errorLabel->setTextFormat(Qt::PlainText);
    m_errorLabel->hide();

    auto* footerLayout = new QHBoxLayout();
    footerLayout->addWidget(m_modeLabel, 1);
    footerLayout->addWidget(m_errorLabel, 1);
    footerLayout->addWidget(buttonBox);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(headerLayout);
    layout->addWidget(m_modes, 1);
    layout->addLayout(footerLayout);

    QObject::connect(m_loadButton, &QAbstractButton::clicked, this, &AutoPlaylistDialog::loadQuery);
    QObject::connect(m_saveButton, &QAbstractButton::clicked, this, &AutoPlaylistDialog::saveQuery);
    QObject::connect(m_deleteButton, &QAbstractButton::clicked, this, &AutoPlaylistDialog::deleteQuery);
    QObject::connect(m_resetButton, &QAbstractButton::clicked, this, &AutoPlaylistDialog::resetQueries);
    QObject::connect(m_formatAction, &QAction::triggered, this, [this]() {
        const QString query     = m_queryEdit->text();
        const QString formatted = AutoPlaylistQueryBuilder::formatQuery(query);
        if(formatted != query) {
            auto cursor = m_queryEdit->textCursor();
            cursor.beginEditBlock();
            cursor.select(QTextCursor::Document);
            cursor.insertText(formatted);
            cursor.endEditBlock();
        }
        m_queryEdit->setFocus();
    });
    QObject::connect(m_presetBox, &QComboBox::currentIndexChanged, this, &AutoPlaylistDialog::updateButtonState);
    QObject::connect(m_nameEdit, &QLineEdit::textChanged, this, &AutoPlaylistDialog::updateButtonState);
    QObject::connect(m_queryEdit, &QPlainTextEdit::textChanged, this, [this]() {
        m_modeLabel->clear();
        updateButtonState();
    });
    QObject::connect(m_sortQueryEdit, &QLineEdit::textChanged, this, [this]() {
        m_modeLabel->clear();
        updateButtonState();
    });
    QObject::connect(m_rules, &AutoPlaylistRulesWidget::changed, this, &AutoPlaylistDialog::rulesChanged);
    QObject::connect(m_modes, &QTabWidget::currentChanged, this, &AutoPlaylistDialog::changeMode);

    loadQueries();

    if(m_playlist && m_playlist->isAutoPlaylist()) {
        m_nameEdit->setText(m_playlist->name());
        setQuery({.name        = m_playlist->name(),
                  .query       = m_playlist->query(),
                  .sortQuery   = m_playlist->sortQuery(),
                  .forceSorted = m_playlist->forceSorted()});
    }

    updateButtonState();
    setMinimumWidth(720);
    resize(960, 680);

    const FyStateSettings stateSettings;
    Utils::restoreState(this, stateSettings, DialogStateGroup);

    m_nameEdit->setFocus();
}

void AutoPlaylistDialog::done(int value)
{
    FyStateSettings stateSettings;
    Utils::saveState(this, stateSettings, DialogStateGroup);

    saveQueries();
    QDialog::done(value);
}

void AutoPlaylistDialog::accept()
{
    updateButtonState();

    if(!m_okButton->isEnabled()) {
        return;
    }

    const auto query = currentQuery();
    if(m_playlistHandler && m_playlist) {
        m_playlistHandler->createAutoPlaylist(m_playlist->name(), query.query, query.sortQuery, query.forceSorted);
        if(query.name != m_playlist->name()) {
            m_playlistHandler->renamePlaylist(m_playlist->id(), query.name);
        }
    }

    Q_EMIT playlistEdited(query.name, query.query, query.sortQuery, query.forceSorted);
    QDialog::accept();
}

void AutoPlaylistDialog::updateButtonState()
{
    m_rules->limitWidget()->setVisible(m_modes->currentIndex() == 0);
    m_rules->randomWidget()->setVisible(m_modes->currentIndex() == 0);

    QString error;
    if(m_modes->currentIndex() == 0) {
        error = m_rules->error();
    }
    if(error.isEmpty()) {
        error = AutoPlaylistQueryBuilder::queryError(m_queryEdit->text(), m_sortQueryEdit->text());
    }
    m_errorLabel->setText(error);
    m_errorLabel->setVisible(!error.isEmpty());
    m_modeLabel->setVisible(error.isEmpty() && !m_modeLabel->text().isEmpty());

    const bool hasPreset = m_presetBox->currentIndex() >= 0;
    m_loadButton->setEnabled(hasPreset);
    m_saveButton->setEnabled(error.isEmpty());
    m_deleteButton->setEnabled(hasPreset);
    m_resetButton->setEnabled(m_queryChanged);
    m_formatAction->setEnabled(AutoPlaylistQueryBuilder::queryError(m_queryEdit->text(), {}).isEmpty());
    m_okButton->setEnabled(!m_nameEdit->text().trimmed().isEmpty() && error.isEmpty());
}

void AutoPlaylistDialog::setQuery(const AutoPlaylistQuery& query)
{
    const QSignalBlocker modeBlocker{m_modes};
    const QSignalBlocker queryBlocker{m_queryEdit};
    const QSignalBlocker sortBlocker{m_sortQueryEdit};

    m_queryEdit->setText(query.query);
    m_sortQueryEdit->setText(query.sortQuery);
    m_forceSorted->setChecked(query.forceSorted);

    if(const auto rules = AutoPlaylistQueryBuilder::rules(query.query, query.sortQuery)) {
        m_rules->setRules(*rules);
        m_rulesQuery     = query.query;
        m_rulesSortQuery = query.sortQuery;
        m_haveRules      = true;
        m_modes->setCurrentIndex(0);
        m_modeLabel->clear();
    }
    else {
        m_haveRules = false;
        m_modes->setCurrentIndex(1);
        m_modeLabel->setText(tr("This query uses expressions that are not supported by the rule editor."));
    }

    updateButtonState();
}

void AutoPlaylistDialog::rulesChanged()
{
    const QSignalBlocker queryBlocker{m_queryEdit};
    const QSignalBlocker sortBlocker{m_sortQueryEdit};

    const auto [query, sortQuery] = AutoPlaylistQueryBuilder::query(m_rules->rules());
    m_queryEdit->setText(query);
    m_sortQueryEdit->setText(sortQuery);
    m_rulesQuery     = query;
    m_rulesSortQuery = sortQuery;
    m_haveRules      = true;

    updateButtonState();
}

void AutoPlaylistDialog::changeMode(int index)
{
    m_modeLabel->clear();

    if(index == 0
       && !(m_haveRules && m_queryEdit->text() == m_rulesQuery && m_sortQueryEdit->text() == m_rulesSortQuery)) {
        if(const auto rules = AutoPlaylistQueryBuilder::rules(m_queryEdit->text(), m_sortQueryEdit->text())) {
            m_rules->setRules(*rules);
            m_rulesQuery     = m_queryEdit->text();
            m_rulesSortQuery = m_sortQueryEdit->text();
            m_haveRules      = true;
        }
        else {
            const QSignalBlocker blocker{m_modes};

            m_modes->setCurrentIndex(1);
            m_modeLabel->setText(tr("This query cannot be represented as rules. Continue editing it in Script mode."));
        }
    }

    updateButtonState();
}

AutoPlaylistQuery AutoPlaylistDialog::currentQuery() const
{
    return {.name        = m_nameEdit->text(),
            .query       = m_queryEdit->text(),
            .sortQuery   = m_sortQueryEdit->text(),
            .forceSorted = m_forceSorted->isChecked()};
}

std::vector<AutoPlaylistQuery>::iterator AutoPlaylistDialog::findQuery(const QString& name)
{
    return std::ranges::find_if(m_queries, [&name](const auto& query) { return query.name == name; });
}

void AutoPlaylistDialog::saveQuery()
{
    bool accepted{false};
    const QString name
        = QInputDialog::getText(this, tr("Save Preset"), tr("Preset name:"), QLineEdit::Normal,
                                m_presetBox->currentIndex() >= 0 ? m_presetBox->currentText() : m_nameEdit->text(),
                                &accepted)
              .trimmed();
    if(!accepted || name.isEmpty()) {
        return;
    }

    auto query = currentQuery();
    query.name = name;

    const auto existing = findQuery(name);
    if(existing != m_queries.end()) {
        if(QMessageBox::question(this, tr("Preset already exists"),
                                 tr("A preset named ‘%1’ already exists. Overwrite it?").arg(name))
           != QMessageBox::Yes) {
            return;
        }
        *existing = std::move(query);
    }
    else {
        m_queries.push_back(query);
        m_presetBox->addItem(name);
    }

    m_presetBox->setCurrentIndex(m_presetBox->findText(name));
    m_queryChanged = true;
    updateButtonState();
}

void AutoPlaylistDialog::loadQuery()
{
    const auto query = findQuery(m_presetBox->currentText());
    if(query != m_queries.end()) {
        setQuery(*query);
    }
}

void AutoPlaylistDialog::deleteQuery()
{
    const auto query = findQuery(m_presetBox->currentText());
    if(query != m_queries.end()) {
        m_queries.erase(query);
        m_presetBox->removeItem(m_presetBox->currentIndex());
        m_queryChanged = true;
        updateButtonState();
    }
}

void AutoPlaylistDialog::resetQueries()
{
    FySettings settings;
    settings.remove(SavedQueries);

    loadDefaultQueries();
    m_queryChanged = false;
    populateQueries();
    updateButtonState();
}

void AutoPlaylistDialog::saveQueries() const
{
    if(!m_queryChanged) {
        return;
    }

    QByteArray byteArray;
    QDataStream stream(&byteArray, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    DataStream::writeContainer(stream, m_queries);

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

        DataStream::readContainer(stream, m_queries);
        m_queryChanged = stream.status() == QDataStream::Ok;
        if(!m_queryChanged) {
            loadDefaultQueries();
        }
    }
    else {
        loadDefaultQueries();
    }

    populateQueries();
}

void AutoPlaylistDialog::populateQueries()
{
    m_presetBox->clear();

    for(const auto& query : m_queries) {
        m_presetBox->addItem(query.name);
    }
}

void AutoPlaylistDialog::loadDefaultQueries()
{
    m_queries = {
        {.name = tr("Most Played"), .query = u"playcount>0 LIMIT 25"_s, .sortQuery = u"SORT- playcount"_s},
        {.name = tr("Recently Added"), .query = u"addedtime DURING LAST 2 WEEKS"_s, .sortQuery = u"SORT- addedtime"_s},
        {.name      = tr("Last Played 2 Weeks"),
         .query     = u"lastplayed DURING LAST 2 WEEKS"_s,
         .sortQuery = u"SORT- lastplayed"_s},
        {.name = tr("Has Lyrics"), .query = u"lyrics PRESENT"_s, .sortQuery = {}}};
}
} // namespace Fooyin

#include "moc_autoplaylistdialog.cpp"
