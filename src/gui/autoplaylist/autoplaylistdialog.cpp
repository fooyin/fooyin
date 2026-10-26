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
#include "playlist/playlistview.h"
#include "playlist/playlistwidget.h"

#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <core/scripting/trackqueryfilter.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/widgets/scriptlineedit.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/widgetcontext.h>
#include <utils/async.h>
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
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTextCursor>
#include <QTimerEvent>

using namespace Qt::StringLiterals;

constexpr auto SavedQueries     = "Playlist/AutoPlaylistQueries"_L1;
constexpr auto DialogStateGroup = "AutoPlaylistDialog"_L1;
constexpr auto PreviewVisible   = "AutoPlaylistDialog/PreviewVisible"_L1;
constexpr auto PreviewWidth     = "AutoPlaylistDialog/PreviewWidth"_L1;
constexpr auto PreviewState     = "AutoPlaylistDialog/PreviewState"_L1;

namespace Fooyin {
AutoPlaylistDialog::AutoPlaylistDialog(ActionManager* actionManager, MusicLibrary* library,
                                       PlaylistHandler* playlisthandler, Playlist* playlist,
                                       std::function<PlaylistWidget*(QWidget*)> createPreview, QWidget* parent)
    : QDialog{parent}
    , m_library{library}
    , m_playlistHandler{playlisthandler}
    , m_playlist{playlist}
    , m_queryChanged{false}
    , m_haveRules{true}
    , m_countGeneration{0}
    , m_countValid{false}
    , m_countRunning{false}
    , m_countPending{false}
    , m_createPreview{std::move(createPreview)}
    , m_splitter{new QSplitter(Qt::Horizontal, this)}
    , m_editor{new QWidget(m_splitter)}
    , m_previewPanel{new QWidget(m_splitter)}
    , m_previewAction{new QAction(tr("Show preview"), this)}
    , m_previewWidth{420}
    , m_nameEdit{new QLineEdit(this)}
    , m_presetBox{new QComboBox(this)}
    , m_modes{new QTabWidget(this)}
    , m_rules{new AutoPlaylistRulesWidget(this)}
    , m_statusLabel{new QLabel(this)}
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

    auto* context = new WidgetContext(this, Context{Constants::Context::AutoPlaylistDialog}, this);
    actionManager->addContextObject(context);

    const FySettings settings;
    m_presetBox->setEditable(settings.value(Settings::Gui::PresetComboBoxModeKey, true).toBool());
    m_presetBox->setInsertPolicy(QComboBox::NoInsert);
    m_saveButton->setText(m_presetBox->isEditable() ? tr("&Save") : tr("&Save…"));

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

    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextFormat(Qt::PlainText);

    m_previewAction->setCheckable(true);
    m_previewAction->setToolTip(tr("Show matching tracks"));
    Gui::setThemeIcon(m_previewAction, Constants::Icons::SidebarRight);
    auto* previewCmd    = actionManager->registerAction(m_previewAction, Constants::Actions::ToggleAutoPlaylistSidebar,
                                                        context->context());
    auto* previewButton = new ToolButton(this);
    previewButton->setDefaultAction(previewCmd->action());
    QObject::connect(m_previewAction, &QAction::triggered, this, [this](bool visible) { setPreviewVisible(visible); });

    auto* footerLayout = new QHBoxLayout();
    footerLayout->addWidget(previewButton);
    footerLayout->addWidget(m_statusLabel, 1);
    footerLayout->addWidget(buttonBox);

    m_editor->setMinimumWidth(480);

    auto* editorLayout = new QVBoxLayout(m_editor);
    editorLayout->setContentsMargins({});

    editorLayout->addLayout(headerLayout);
    editorLayout->addWidget(m_modes, 1);
    editorLayout->addWidget(m_modeLabel);

    m_previewPanel->setMinimumWidth(280);

    auto* previewLayout = new QHBoxLayout(m_previewPanel);
    Gui::setContentsMargins(previewLayout, Qt::TopEdge | Qt::RightEdge | Qt::BottomEdge, 0);

    m_splitter->insertWidget(0, m_editor);
    m_splitter->addWidget(m_previewPanel);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_previewPanel->hide();

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_splitter, 1);
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
    QObject::connect(m_presetBox, &QComboBox::currentTextChanged, this, &AutoPlaylistDialog::updateButtonState);
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

    QObject::connect(m_library, &MusicLibrary::tracksLoaded, this, &AutoPlaylistDialog::scheduleTrackCount);
    QObject::connect(m_library, &MusicLibrary::tracksAdded, this, &AutoPlaylistDialog::scheduleTrackCount);
    QObject::connect(m_library, &MusicLibrary::tracksMetadataChanged, this, &AutoPlaylistDialog::scheduleTrackCount);
    QObject::connect(m_library, &MusicLibrary::tracksUpdated, this, &AutoPlaylistDialog::scheduleTrackCount);
    QObject::connect(m_library, &MusicLibrary::tracksStatsChanged, this, &AutoPlaylistDialog::scheduleTrackCount);
    QObject::connect(m_library, &MusicLibrary::tracksDeleted, this, &AutoPlaylistDialog::scheduleTrackCount);
    QObject::connect(m_library, &MusicLibrary::tracksSorted, this, &AutoPlaylistDialog::scheduleTrackCount);

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

    m_previewWidth            = std::max(280, stateSettings.value(PreviewWidth, m_previewWidth).toInt());
    const bool previewVisible = stateSettings.value(PreviewVisible, false).toBool();
    m_previewAction->setChecked(previewVisible);
    if(previewVisible) {
        setPreviewVisible(true, false);
    }

    m_nameEdit->setFocus();
}

void AutoPlaylistDialog::done(int value)
{
    ++m_countGeneration;
    m_countTimer.stop();
    m_countPending = false;

    FyStateSettings stateSettings;
    stateSettings.setValue(PreviewVisible, m_previewAction->isChecked());
    stateSettings.setValue(PreviewWidth, m_previewAction->isChecked() ? m_splitter->sizes().at(1) : m_previewWidth);
    if(m_preview) {
        QJsonObject previewState;
        m_preview->saveLayoutData(previewState);
        stateSettings.setValue(PreviewState, previewState);
    }
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
    if(m_playlist) {
        m_playlistHandler->createAutoPlaylist(m_playlist->name(), query.query, query.sortQuery, query.forceSorted);
        if(query.name != m_playlist->name()) {
            m_playlistHandler->renamePlaylist(m_playlist->id(), query.name);
        }
    }
    else if(auto* playlist
            = m_playlistHandler->createNewAutoPlaylist(query.name, query.query, query.sortQuery, query.forceSorted)) {
        Q_EMIT playlistCreated(playlist);
    }

    QDialog::accept();
}

bool AutoPlaylistDialog::eventFilter(QObject* watched, QEvent* event)
{
    if(m_preview && watched == m_preview->view() && event->type() == QEvent::KeyPress) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        if(keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void AutoPlaylistDialog::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_countTimer.timerId()) {
        m_countTimer.stop();
        startTrackCount();
        return;
    }
    QDialog::timerEvent(event);
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

    m_modeLabel->setVisible(!m_modeLabel->text().isEmpty());

    const QString query = m_sortQueryEdit->text().isEmpty() ? m_queryEdit->text()
                                                            : m_queryEdit->text() + u' ' + m_sortQueryEdit->text();
    const bool valid    = error.isEmpty();
    if(query != m_countQuery || valid != m_countValid) {
        m_countQuery = query;
        m_countValid = valid;
        scheduleTrackCount();
    }
    if(!valid) {
        m_statusLabel->setText(error);
    }

    const bool hasPreset = findQuery(m_presetBox->currentText()) != m_queries.end();
    m_loadButton->setEnabled(hasPreset);
    m_saveButton->setEnabled(error.isEmpty()
                             && (!m_presetBox->isEditable() || !m_presetBox->currentText().trimmed().isEmpty()));
    m_deleteButton->setEnabled(hasPreset);
    m_resetButton->setEnabled(m_queryChanged);
    m_formatAction->setEnabled(AutoPlaylistQueryBuilder::queryError(m_queryEdit->text(), {}).isEmpty());
    m_okButton->setEnabled(!m_nameEdit->text().trimmed().isEmpty() && error.isEmpty());
}

void AutoPlaylistDialog::scheduleTrackCount()
{
    ++m_countGeneration;
    m_countTimer.stop();
    m_countPending = m_countValid;

    if(m_countPending) {
        m_statusLabel->setText(tr("Updating…"));
        m_countTimer.start(300, this);
    }
    updatePreview();
}

void AutoPlaylistDialog::startTrackCount()
{
    if(!m_countPending || m_countRunning || m_countTimer.isActive()) {
        return;
    }

    m_countPending = false;
    m_countRunning = true;

    const auto generation{m_countGeneration};
    const auto options = TrackQueryFilter::defaultSearchOptions();

    Utils::asyncExec([query = m_countQuery, tracks = m_library->libraryTracks(), options]() {
        TrackQueryFilter filter{options};
        return filter.filter(query, tracks);
    }).then(this, [this, generation](const TrackList& tracks) {
        m_countRunning = false;
        if(generation == m_countGeneration) {
            m_previewTracks  = tracks;
            const auto count = static_cast<int>(tracks.size());
            m_statusLabel->setText(count == 0 ? tr("No matching tracks") : tr("%Ln matching track(s)", nullptr, count));
            if(m_preview && m_previewAction->isChecked()) {
                m_preview->setTracks(m_previewTracks);
            }
            updatePreview();
        }
        startTrackCount();
    });
}

void AutoPlaylistDialog::setPreviewVisible(bool visible, bool resizeDialog)
{
    const int editorWidth = m_splitter->sizes().front();

    if(visible && !m_preview) {
        m_preview = m_createPreview(m_previewPanel);

        const FyStateSettings stateSettings;
        const QJsonObject previewState = stateSettings.value(PreviewState).toJsonObject();
        if(!previewState.isEmpty()) {
            m_preview->loadLayoutData(previewState);
        }
        m_preview->finalise();

        m_preview->view()->setLoadingText(tr("Updating…"));
        m_preview->view()->installEventFilter(this);
        m_previewPanel->layout()->addWidget(m_preview);
    }

    if(!visible) {
        m_previewWidth = m_splitter->sizes().at(1);
    }
    m_previewPanel->setVisible(visible);

    const int margin = visible ? m_previewPanel->layout()->contentsMargins().left() : 0;
    Gui::setContentsMargins(m_editor->layout(), Qt::RightEdge, margin);

    if(resizeDialog && !isMaximized()) {
        const auto available     = screen()->availableGeometry();
        const int extraWidth     = m_previewWidth + m_splitter->handleWidth();
        const int targetWidth    = visible ? width() + extraWidth : width() - extraWidth;
        const int availableWidth = std::max(minimumWidth(), available.width());
        resize(std::clamp(targetWidth, minimumWidth(), availableWidth), height());
        if(visible) {
            move(std::max(available.left(), std::min(x(), available.right() - frameGeometry().width() + 1)), y());
        }
    }

    layout()->activate();

    if(visible) {
        m_splitter->setSizes({resizeDialog ? editorWidth : m_splitter->width() - m_previewWidth, m_previewWidth});
        m_preview->setTracks(m_previewTracks);
    }

    updatePreview();
}

void AutoPlaylistDialog::updatePreview()
{
    if(!m_countValid) {
        m_previewTracks.clear();
        if(m_preview) {
            m_preview->view()->setEmptyText({});
            m_preview->setTracks({});
        }
    }
    else {
        if(m_preview) {
            m_preview->view()->setEmptyText(tr("No matching tracks"));
        }
    }
}

void AutoPlaylistDialog::setQuery(const AutoPlaylistQuery& query)
{
    const QSignalBlocker modeBlocker{m_modes};
    const QSignalBlocker queryBlocker{m_queryEdit};
    const QSignalBlocker sortBlocker{m_sortQueryEdit};

    m_queryEdit->setText(query.query);
    m_sortQueryEdit->setScript(query.sortQuery);
    m_forceSorted->setChecked(query.forceSorted);

    if(const auto rules = AutoPlaylistQueryBuilder::rules(query.query, query.sortQuery)) {
        m_rules->setRules(*rules);
        m_rulesQuery     = query.query;
        m_rulesSortQuery = query.sortQuery;
        m_haveRules      = true;
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
    m_sortQueryEdit->setScript(sortQuery);
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
    QString name = m_presetBox->currentText();
    if(!m_presetBox->isEditable()) {
        bool accepted{false};
        name = QInputDialog::getText(this, tr("Save Preset"), tr("Preset name:"), QLineEdit::Normal,
                                     m_presetBox->currentIndex() >= 0 ? name : m_nameEdit->text(), &accepted);
        if(!accepted) {
            return;
        }
    }
    name = name.trimmed();
    if(name.isEmpty()) {
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
        const QString name = m_nameEdit->text();
        if(name.trimmed().isEmpty() || name == m_lastPresetName) {
            m_lastPresetName = query->name;
            m_nameEdit->setText(m_lastPresetName);
            m_nameEdit->setCursorPosition(0);
        }
        setQuery(*query);
    }
}

void AutoPlaylistDialog::deleteQuery()
{
    const QString name = m_presetBox->currentText();
    const auto query   = findQuery(name);
    if(query != m_queries.end()) {
        m_queries.erase(query);
        m_presetBox->removeItem(m_presetBox->findText(name));
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
        {.name = tr("Most Played"), .query = u"playcount > 0 LIMIT 50"_s, .sortQuery = u"SORT- playcount"_s},
        {.name = tr("Recently Added"), .query = u"addedtime DURING LAST 2 WEEKS"_s, .sortQuery = u"SORT- addedtime"_s},
        {.name      = tr("Recently Played"),
         .query     = u"lastplayed DURING LAST 2 WEEKS"_s,
         .sortQuery = u"SORT- lastplayed"_s},
        {.name = tr("Loved"), .query = uR"(loved IS 1)"_s, .sortQuery = u"SORT+ artist"_s},
        {.name = tr("Highly Rated"), .query = u"rating >= 4"_s, .sortQuery = u"SORT- rating"_s},
        {.name = tr("Unplayed"), .query = uR"(playcount IS 0)"_s, .sortQuery = {}},
        {.name = tr("Forgotten Favourites"),
         .query
         = uR"((rating >= 4 OR loved IS "1") AND playcount > 0 AND lastplayed NOT DURING LAST 6 MONTHS LIMIT 50)"_s,
         .sortQuery = u"SORT+ $rand()"_s},
        {.name = tr("Has Lyrics"), .query = u"lyrics PRESENT"_s, .sortQuery = {}}};
}
} // namespace Fooyin

#include "moc_autoplaylistdialog.cpp"
