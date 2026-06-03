/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <gui/scripting/scripteditor.h>

#include "expressiontreemodel.h"
#include "scripthighlighter.h"
#include "scriptreferenceentries.h"

#include <core/coresettings.h>
#include <core/library/librarymanager.h>
#include <core/player/playercontroller.h>
#include <core/scripting/scriptenvironmenthelpers.h>
#include <core/scripting/scriptparser.h>
#include <core/track.h>
#include <gui/scripting/richtextutils.h>
#include <gui/scripting/scriptformatter.h>
#include <gui/trackselectioncontroller.h>
#include <utils/utils.h>

#include <QApplication>
#include <QBasicTimer>
#include <QCompleter>
#include <QDesktopServices>
#include <QDir>
#include <QFocusEvent>
#include <QGridLayout>
#include <QHeaderView>
#include <QItemSelection>
#include <QLineEdit>
#include <QPalette>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimerEvent>
#include <QTreeView>
#include <QUrl>

#include <chrono>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto TextChangeInterval = 1500ms;
#else
constexpr auto TextChangeInterval = 1500;
#endif

constexpr auto DialogState = "Interface/ScriptEditorState";

namespace Fooyin {
enum ScriptReferenceRole : int
{
    InsertTextRole = Qt::UserRole + 1,
    CursorOffsetRole,
    KindRole,
};

class ScriptReferenceFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        if(filterRegularExpression().pattern().isEmpty()) {
            return true;
        }

        const int columns = sourceModel()->columnCount(sourceParent);
        for(int column{0}; column < columns; ++column) {
            const QModelIndex index = sourceModel()->index(sourceRow, column, sourceParent);
            if(sourceModel()->data(index).toString().contains(filterRegularExpression())) {
                return true;
            }
        }

        return false;
    }
};

class ScriptCompleter : public QCompleter
{
    Q_OBJECT

public:
    using QCompleter::QCompleter;

    [[nodiscard]] QString pathFromIndex(const QModelIndex& index) const override
    {
        return index.data(InsertTextRole).toString();
    }
};

class ScriptEditorTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit ScriptEditorTextEdit(QWidget* parent = nullptr)
        : QTextEdit{parent}
        , m_completer{new ScriptCompleter(this)}
        , m_variableModel{new QStandardItemModel(this)}
        , m_functionModel{new QStandardItemModel(this)}
    {
        populateCompletionModels();

        m_completer->setWidget(this);
        m_completer->setCaseSensitivity(Qt::CaseInsensitive);
        m_completer->setCompletionMode(QCompleter::PopupCompletion);
        m_completer->setFilterMode(Qt::MatchStartsWith);

        QObject::connect(m_completer, qOverload<const QModelIndex&>(&QCompleter::activated), this,
                         &ScriptEditorTextEdit::insertCompletion);
    }

    void insertSnippet(const QString& insertText, int cursorOffset = 0,
                       ScriptReferenceKind kind = ScriptReferenceKind::Variable)
    {
        QTextCursor cursor{textCursor()};

        if(kind == ScriptReferenceKind::Formatting && cursor.hasSelection() && cursorOffset > 0) {
            const auto splitPosition = insertText.size() - cursorOffset;
            const QString prefix     = insertText.left(splitPosition);
            const QString suffix     = insertText.mid(splitPosition);
            const QString selected   = cursor.selectedText();

            cursor.insertText(prefix + selected + suffix);
            setTextCursor(cursor);
            return;
        }

        cursor.insertText(insertText);
        setTextCursor(cursor);

        if(cursorOffset > 0) {
            cursor = textCursor();
            cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, cursorOffset);
            setTextCursor(cursor);
        }
    }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if(m_completer->popup()->isVisible()) {
            switch(event->key()) {
                case Qt::Key_Return:
                case Qt::Key_Enter:
                case Qt::Key_Escape:
                case Qt::Key_Tab:
                case Qt::Key_Backtab:
                    event->ignore();
                    return;
                default:
                    break;
            }
        }

        QTextEdit::keyPressEvent(event);

        if(shouldUpdateCompletion(event)) {
            updateCompletion();
        }
        else if(isCompletionDismissKey(event)) {
            m_completer->popup()->hide();
        }
    }

private:
    struct CompletionContext
    {
        bool valid{false};
        ScriptReferenceKind kind{ScriptReferenceKind::Variable};
        QString prefix;
        int startPos{-1};
        int endPos{-1};
    };

    [[nodiscard]] static bool shouldUpdateCompletion(const QKeyEvent* event)
    {
        if(event->modifiers().testFlag(Qt::ControlModifier) || event->modifiers().testFlag(Qt::AltModifier)
           || event->modifiers().testFlag(Qt::MetaModifier)) {
            return false;
        }

        switch(event->key()) {
            case Qt::Key_Backspace:
            case Qt::Key_Delete:
                return true;
            case Qt::Key_Left:
            case Qt::Key_Right:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Home:
            case Qt::Key_End:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
                return false;
            default:
                break;
        }

        const QString text = event->text();
        if(text.size() != 1) {
            return false;
        }

        return !text.front().isNull();
    }

    [[nodiscard]] static bool isCompletionDismissKey(const QKeyEvent* event)
    {
        switch(event->key()) {
            case Qt::Key_Left:
            case Qt::Key_Right:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Home:
            case Qt::Key_End:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
            case Qt::Key_Escape:
                return true;
            default:
                return false;
        }
    }

    void populateCompletionModels()
    {
        for(const auto& entry : scriptReferenceEntries()) {
            auto* item = new QStandardItem(entry.label);
            item->setData(entry.insertText, InsertTextRole);
            item->setData(entry.cursorOffset, CursorOffsetRole);
            item->setData(static_cast<int>(entry.kind), KindRole);
            item->setToolTip(entry.description);

            switch(entry.kind) {
                case ScriptReferenceKind::Variable:
                    m_variableModel->appendRow(item);
                    break;
                case ScriptReferenceKind::Function:
                case ScriptReferenceKind::CommandAlias:
                    m_functionModel->appendRow(item);
                    break;
                case ScriptReferenceKind::Formatting:
                    break;
            }
        }
    }

    void updateCompletion()
    {
        const CompletionContext context = completionContext();
        if(!context.valid) {
            m_completer->popup()->hide();
            return;
        }

        m_completionStart = context.startPos;
        m_completionEnd   = context.endPos;

        QStandardItemModel* model{nullptr};
        switch(context.kind) {
            case ScriptReferenceKind::Variable:
                model = m_variableModel;
                break;
            case ScriptReferenceKind::Function:
                model = m_functionModel;
                break;
            case ScriptReferenceKind::Formatting:
            case ScriptReferenceKind::CommandAlias:
                break;
        }

        if(model) {
            m_completer->setModel(model);
        }

        m_completer->setCompletionPrefix(context.prefix);

        if(!m_completer->setCurrentRow(0)) {
            m_completer->popup()->hide();
            return;
        }

        QRect rect{cursorRect()};
        rect.setWidth(m_completer->popup()->sizeHintForColumn(0)
                      + m_completer->popup()->verticalScrollBar()->sizeHint().width() + 24);
        m_completer->complete(rect);
    }

    [[nodiscard]] CompletionContext completionContext() const
    {
        const QTextCursor cursor = textCursor();
        const QTextBlock block   = cursor.block();
        const QString text       = block.text();
        const int posInBlock     = cursor.position() - block.position();

        if(posInBlock <= 0 || posInBlock > text.size()) {
            return {};
        }

        int start{posInBlock};

        while(start > 0) {
            const QChar ch = text.at(start - 1);

            if(ch.isLetterOrNumber() || ch == u'_') {
                --start;
                continue;
            }

            if(ch == '%'_L1 || ch == '$'_L1) {
                --start;
            }

            break;
        }

        if(start < 0 || start >= posInBlock) {
            return {};
        }

        const QChar opener = text.at(start);
        if(opener != '%'_L1 && opener != '$'_L1) {
            return {};
        }

        // Don't open if at end of variable
        if(opener == '%'_L1 && start == posInBlock - 1 && start > 0) {
            const QChar previous = text.at(start - 1);
            if(previous.isLetterOrNumber() || previous == '_'_L1) {
                return {};
            }
        }

        int end{posInBlock};

        while(end < text.size()) {
            const QChar ch = text.at(end);

            if(ch.isLetterOrNumber() || ch == '_'_L1) {
                ++end;
                continue;
            }

            break;
        }

        if(opener == '%'_L1 && end < text.size() && text.at(end) == '%'_L1) {
            ++end;
        }

        CompletionContext context;
        context.valid    = true;
        context.kind     = opener == '%'_L1 ? ScriptReferenceKind::Variable : ScriptReferenceKind::Function;
        context.prefix   = text.mid(start, posInBlock - start);
        context.startPos = block.position() + start;
        context.endPos   = block.position() + end;
        return context;
    }

    void insertCompletion(const QModelIndex& index)
    {
        if(!index.isValid() || m_completionStart < 0 || m_completionEnd < m_completionStart) {
            return;
        }

        QTextCursor cursor{textCursor()};
        cursor.setPosition(m_completionStart);
        cursor.setPosition(m_completionEnd, QTextCursor::KeepAnchor);
        cursor.insertText(index.data(InsertTextRole).toString());
        setTextCursor(cursor);

        const int cursorOffset = index.data(CursorOffsetRole).toInt();
        if(cursorOffset > 0) {
            cursor = textCursor();
            cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, cursorOffset);
            setTextCursor(cursor);
        }
    }

    ScriptCompleter* m_completer;
    QStandardItemModel* m_variableModel;
    QStandardItemModel* m_functionModel;
    int m_completionStart{-1};
    int m_completionEnd{-1};
};

class ScriptEditorEnvironment : public ScriptEnvironment
{
public:
    explicit ScriptEditorEnvironment(LibraryManager* libraryManager)
        : m_libraryEnvironment{libraryManager}
    { }

    void updatePlaybackState(PlayerController* playerController)
    {
        m_playbackEnvironment.setPlaylistData(nullptr, playerController ? &playerController->playbackQueue() : nullptr,
                                              nullptr, playerController ? playerController->queuedTracksCount() : 0);

        int currentPlayingTrackIndex{-1};
        int currentPlayingTrackId{-1};

        if(playerController) {
            const PlaylistTrack currentTrack = playerController->currentPlaylistTrack();
            currentPlayingTrackIndex         = currentTrack.indexInPlaylist;
            currentPlayingTrackId            = playerController->currentTrackId();
        }

        m_playbackEnvironment.setTrackState(-1, currentPlayingTrackIndex, currentPlayingTrackId, 0);
        m_playbackEnvironment.setPlaybackState(playerController ? playerController->currentPosition() : 0,
                                               playerController ? playerController->currentTrack().duration() : 0,
                                               playerController ? playerController->bitrate() : 0,
                                               playerController ? playerController->playState()
                                                                : Player::PlayState::Stopped);
    }

    [[nodiscard]] const ScriptPlaybackEnvironment* playbackEnvironment() const override
    {
        return m_playbackEnvironment.playbackEnvironment();
    }

    [[nodiscard]] const ScriptPlaylistEnvironment* playlistEnvironment() const override
    {
        return m_playbackEnvironment.playlistEnvironment();
    }

    [[nodiscard]] const ScriptLibraryEnvironment* libraryEnvironment() const override
    {
        return m_libraryEnvironment.libraryEnvironment();
    }

    [[nodiscard]] const ScriptEvaluationEnvironment* evaluationEnvironment() const override
    {
        return m_libraryEnvironment.evaluationEnvironment();
    }

private:
    LibraryScriptEnvironment m_libraryEnvironment;
    PlaylistScriptEnvironment m_playbackEnvironment;
};

class ScriptEditorPrivate : public QObject
{
    Q_OBJECT

public:
    ScriptEditorPrivate(ScriptEditor* self, LibraryManager* libraryManager, const Track& track,
                        TrackSelectionController* selectionController = nullptr,
                        PlayerController* playerController            = nullptr);
    ~ScriptEditorPrivate() override;

    void setupConnections();
    void setupPlaceholder();
    void setupReference();

    void updateResults();
    void updateResults(const Expression& expression);

    void trackContextChanged();
    void selectionChanged();
    void textChanged();
    void referenceSearchChanged(const QString& text);
    void referenceItemActivated(const QModelIndex& index);
    void referenceTabChanged(int index);

    void showErrors() const;

    void saveState();
    void restoreState();

    ScriptEditor* m_self;
    LibraryManager* m_libraryManager;
    TrackSelectionController* m_selectionController;
    PlayerController* m_playerController;
    FySettings m_settings;
    Track m_track;
    Track m_placeholderTrack;

    QSplitter* m_mainSplitter;
    QSplitter* m_documentSplitter;
    QTabWidget* m_sideTabs;

    ScriptEditorTextEdit* m_editor;
    QTextBrowser* m_results;
    ScriptHighlighter m_highlighter;

    QTreeView* m_expressionTree;
    QTabWidget* m_referenceTabs;
    QLineEdit* m_referenceSearch;
    QTreeView* m_variableReferenceTree;
    QTreeView* m_functionReferenceTree;
    QTreeView* m_formattingReferenceTree;
    QTreeView* m_commandReferenceTree;
    QStandardItemModel* m_variableReferenceModel;
    QStandardItemModel* m_functionReferenceModel;
    QStandardItemModel* m_formattingReferenceModel;
    QStandardItemModel* m_commandReferenceModel;
    ScriptReferenceFilterModel* m_variableReferenceFilter;
    ScriptReferenceFilterModel* m_functionReferenceFilter;
    ScriptReferenceFilterModel* m_formattingReferenceFilter;
    ScriptReferenceFilterModel* m_commandReferenceFilter;
    ExpressionTreeModel* m_model;

    QBasicTimer m_textChangeTimer;

    ScriptParser m_parser;
    ScriptFormatter m_formatter;
    ScriptEditorEnvironment m_environment;
    ScriptContext m_scriptContext;

    ParsedScript m_currentScript;
};

ScriptEditorPrivate::ScriptEditorPrivate(ScriptEditor* self, LibraryManager* libraryManager, const Track& track,
                                         TrackSelectionController* selectionController,
                                         PlayerController* playerController)
    : m_self{self}
    , m_libraryManager{libraryManager}
    , m_selectionController{selectionController}
    , m_playerController{playerController}
    , m_track{track}
    , m_mainSplitter{new QSplitter(Qt::Horizontal, m_self)}
    , m_documentSplitter{new QSplitter(Qt::Vertical, m_self)}
    , m_sideTabs{new QTabWidget(m_self)}
    , m_editor{new ScriptEditorTextEdit(m_self)}
    , m_results{new QTextBrowser(m_self)}
    , m_highlighter{m_editor->document()}
    , m_expressionTree{new QTreeView(m_self)}
    , m_referenceTabs{new QTabWidget(m_self)}
    , m_referenceSearch{new QLineEdit(m_self)}
    , m_variableReferenceTree{new QTreeView(m_self)}
    , m_functionReferenceTree{new QTreeView(m_self)}
    , m_formattingReferenceTree{new QTreeView(m_self)}
    , m_commandReferenceTree{new QTreeView(m_self)}
    , m_variableReferenceModel{new QStandardItemModel(m_self)}
    , m_functionReferenceModel{new QStandardItemModel(m_self)}
    , m_formattingReferenceModel{new QStandardItemModel(m_self)}
    , m_commandReferenceModel{new QStandardItemModel(m_self)}
    , m_variableReferenceFilter{new ScriptReferenceFilterModel(m_self)}
    , m_functionReferenceFilter{new ScriptReferenceFilterModel(m_self)}
    , m_formattingReferenceFilter{new ScriptReferenceFilterModel(m_self)}
    , m_commandReferenceFilter{new ScriptReferenceFilterModel(m_self)}
    , m_model{new ExpressionTreeModel(m_self)}
    , m_environment{libraryManager}
{
    m_scriptContext.environment = &m_environment;
    m_environment.updatePlaybackState(m_playerController);

    auto* mainLayout = new QGridLayout(m_self);
    mainLayout->setContentsMargins({});
    mainLayout->addWidget(m_mainSplitter);

    m_results->setReadOnly(true);
    m_results->setOpenLinks(false);
    m_results->setOpenExternalLinks(false);
    m_results->setUndoRedoEnabled(false);
    m_results->document()->setDocumentMargin(0);

    m_expressionTree->setModel(m_model);
    m_expressionTree->setHeaderHidden(true);
    m_expressionTree->setSelectionMode(QAbstractItemView::SingleSelection);

    m_documentSplitter->addWidget(m_editor);
    m_documentSplitter->addWidget(m_results);

    m_documentSplitter->setStretchFactor(0, 3);
    m_documentSplitter->setStretchFactor(1, 1);

    auto* structureTab    = new QWidget(m_self);
    auto* structureLayout = new QGridLayout(structureTab);

    structureLayout->setContentsMargins({});
    structureLayout->addWidget(m_expressionTree);

    setupReference();

    auto* referenceTab    = new QWidget(m_self);
    auto* referenceLayout = new QGridLayout(referenceTab);
    referenceLayout->setContentsMargins({});

    referenceLayout->addWidget(m_referenceSearch, 0, 0);
    referenceLayout->addWidget(m_referenceTabs, 1, 0);

    m_sideTabs->addTab(structureTab, ScriptEditor::tr("Structure"));
    m_sideTabs->addTab(referenceTab, ScriptEditor::tr("Reference"));

    m_mainSplitter->addWidget(m_documentSplitter);
    m_mainSplitter->addWidget(m_sideTabs);

    m_mainSplitter->setStretchFactor(0, 4);
    m_mainSplitter->setStretchFactor(1, 2);

    setupConnections();
    setupPlaceholder();
    restoreState();
}

ScriptEditorPrivate::~ScriptEditorPrivate()
{
    m_textChangeTimer.stop();
    m_editor->disconnect();
}

void ScriptEditorPrivate::setupConnections()
{
    QObject::connect(m_editor, &QTextEdit::textChanged, this, &ScriptEditorPrivate::textChanged);
    QObject::connect(m_model, &QAbstractItemModel::modelReset, m_expressionTree, &QTreeView::expandAll);
    QObject::connect(m_expressionTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &ScriptEditorPrivate::selectionChanged);

    QObject::connect(m_referenceSearch, &QLineEdit::textChanged, this, &ScriptEditorPrivate::referenceSearchChanged);
    const auto connectReferenceTree = [this](QTreeView* tree) {
        QObject::connect(tree, &QTreeView::doubleClicked, this, &ScriptEditorPrivate::referenceItemActivated);
    };
    for(auto* tree :
        {m_variableReferenceTree, m_functionReferenceTree, m_formattingReferenceTree, m_commandReferenceTree}) {
        connectReferenceTree(tree);
    }
    QObject::connect(m_referenceTabs, &QTabWidget::currentChanged, this, &ScriptEditorPrivate::referenceTabChanged);
    QObject::connect(m_results, &QTextBrowser::anchorClicked, m_self, [](const QUrl& url) {
        const QUrl resolvedUrl = QUrl::fromUserInput(url.toString());
        if(resolvedUrl.isValid()) {
            QDesktopServices::openUrl(resolvedUrl);
        }
    });

    if(m_selectionController) {
        QObject::connect(m_selectionController, &TrackSelectionController::selectionChanged, this,
                         &ScriptEditorPrivate::trackContextChanged);
    }
    if(m_playerController) {
        QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                         &ScriptEditorPrivate::trackContextChanged);
        QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this,
                         &ScriptEditorPrivate::trackContextChanged);
        QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this,
                         &ScriptEditorPrivate::trackContextChanged);
        QObject::connect(m_playerController, &PlayerController::positionChangedSeconds, this,
                         qOverload<>(&ScriptEditorPrivate::updateResults));
        QObject::connect(m_playerController, &PlayerController::bitrateChanged, this,
                         qOverload<>(&ScriptEditorPrivate::updateResults));
    }
}

void ScriptEditorPrivate::setupPlaceholder()
{
    QString basePath = QDir::homePath() + "/Music"_L1;
    if(m_libraryManager && m_libraryManager->hasLibrary()) {
        const auto& libraries = m_libraryManager->allLibraries();
        if(!libraries.empty()) {
            const auto& library = libraries.cbegin()->second;
            m_placeholderTrack.setLibraryId(library.id);
            basePath = library.path;
        }
    }

    m_placeholderTrack.setFilePath(
        QDir{basePath}.filePath(u"The Static Hour/City After Midnight/04 - Signal Fires.flac"_s));
    m_placeholderTrack.setTitle(u"Signal Fires"_s);
    m_placeholderTrack.setAlbum(u"City After Midnight"_s);
    m_placeholderTrack.setAlbumArtists({u"The Static Hour"_s});
    m_placeholderTrack.setArtists({u"The Static Hour"_s, u"Mina Vale"_s});
    m_placeholderTrack.setDate(u"2023-10-06"_s);
    m_placeholderTrack.setTrackNumber(u"4"_s);
    m_placeholderTrack.setTrackTotal(u"11"_s);
    m_placeholderTrack.setDiscNumber(u"1"_s);
    m_placeholderTrack.setDiscTotal(u"1"_s);
    m_placeholderTrack.setGenres({u"Synthpop"_s, u"Indie Pop"_s});
    m_placeholderTrack.setBitDepth(24);
    m_placeholderTrack.setSampleRate(44100);
    m_placeholderTrack.setBitrate(1012);
    m_placeholderTrack.setChannels(2);
    m_placeholderTrack.setCodec(u"FLAC"_s);
    m_placeholderTrack.setTagTypes({u"XiphComment"_s});
    m_placeholderTrack.setEncoding(u"Lossless"_s);
    m_placeholderTrack.setComment(u"Single mix"_s);
    m_placeholderTrack.setComposers({u"Ada Mercer"_s, u"Jon Keene"_s});
    m_placeholderTrack.setPerformers({u"Ada Mercer"_s, u"Jon Keene"_s, u"Mina Vale"_s});
    m_placeholderTrack.setRatingStars(4);
    m_placeholderTrack.setPlayCount(27);
    m_placeholderTrack.addExtraTag(u"LABEL"_s, u"Northline Records"_s);
    m_placeholderTrack.addExtraTag(u"CATALOGNUMBER"_s, u"NLR-042"_s);
    m_placeholderTrack.setCreatedTime(1696618800000);
    m_placeholderTrack.setAddedTime(1696963320000);
    m_placeholderTrack.setDuration(222000);
    m_placeholderTrack.setFileSize(28700000);
}

void ScriptEditorPrivate::setupReference()
{
    const auto headers
        = QStringList{ScriptEditor::tr("Item"), ScriptEditor::tr("Category"), ScriptEditor::tr("Description")};

    for(auto* model :
        {m_variableReferenceModel, m_functionReferenceModel, m_formattingReferenceModel, m_commandReferenceModel}) {
        model->setHorizontalHeaderLabels(headers);
    }

    const auto appendRow = [](QStandardItemModel* model, const ScriptReferenceEntry& entry) {
        QList<QStandardItem*> row;
        row.reserve(3);

        auto* item        = new QStandardItem(entry.label);
        auto* category    = new QStandardItem(entry.category);
        auto* description = new QStandardItem(entry.description);

        for(auto* column : {item, category, description}) {
            column->setEditable(false);
            column->setData(entry.insertText, InsertTextRole);
            column->setData(entry.cursorOffset, CursorOffsetRole);
            column->setData(static_cast<int>(entry.kind), KindRole);
            column->setToolTip(entry.description);
        }

        row << item << category << description;
        model->appendRow(row);
    };

    for(const auto& entry : scriptReferenceEntries()) {
        switch(entry.kind) {
            case ScriptReferenceKind::Variable:
                appendRow(m_variableReferenceModel, entry);
                break;
            case ScriptReferenceKind::Function:
                appendRow(m_functionReferenceModel, entry);
                break;
            case ScriptReferenceKind::Formatting:
                appendRow(m_formattingReferenceModel, entry);
                break;
            case ScriptReferenceKind::CommandAlias:
                appendRow(m_commandReferenceModel, entry);
                break;
        }
    }

    const auto configureReferenceTree
        = [](QTreeView* tree, ScriptReferenceFilterModel* filter, QStandardItemModel* model) {
              filter->setSourceModel(model);
              filter->setFilterCaseSensitivity(Qt::CaseInsensitive);

              tree->setModel(filter);
              tree->setRootIsDecorated(false);
              tree->setAlternatingRowColors(true);
              tree->setSelectionBehavior(QAbstractItemView::SelectRows);
              tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
              tree->setSortingEnabled(true);
              tree->sortByColumn(0, Qt::AscendingOrder);

              auto* header = tree->header();
              header->setSectionResizeMode(QHeaderView::Interactive);
              header->setStretchLastSection(true);

              for(int column{0}; column < model->columnCount() - 1; ++column) {
                  tree->resizeColumnToContents(column);
              }
          };

    configureReferenceTree(m_variableReferenceTree, m_variableReferenceFilter, m_variableReferenceModel);
    configureReferenceTree(m_functionReferenceTree, m_functionReferenceFilter, m_functionReferenceModel);
    configureReferenceTree(m_formattingReferenceTree, m_formattingReferenceFilter, m_formattingReferenceModel);
    configureReferenceTree(m_commandReferenceTree, m_commandReferenceFilter, m_commandReferenceModel);

    m_referenceTabs->addTab(m_variableReferenceTree, ScriptEditor::tr("Variables"));
    m_referenceTabs->addTab(m_functionReferenceTree, ScriptEditor::tr("Functions"));
    m_referenceTabs->addTab(m_formattingReferenceTree, ScriptEditor::tr("Formatting"));
    m_referenceTabs->addTab(m_commandReferenceTree, ScriptEditor::tr("Commands"));

    m_referenceSearch->setPlaceholderText(ScriptEditor::tr("Filter"));
}

void ScriptEditorPrivate::updateResults()
{
    const auto indexes = m_expressionTree->selectionModel()->selectedIndexes();
    if(!indexes.empty()) {
        if(const auto* item = static_cast<ExpressionTreeItem*>(indexes.front().internalPointer())) {
            updateResults(item->expression());
        }
        return;
    }

    if(m_model->rowCount({}) > 0) {
        if(const auto* item = static_cast<ExpressionTreeItem*>(m_model->index(0, 0, {}).internalPointer())) {
            updateResults(item->expression());
        }
    }
}

void ScriptEditorPrivate::updateResults(const Expression& expression)
{
    ParsedScript script;
    script.expressions = {expression};

    m_formatter.setBaseFont(m_results->font());

    m_environment.updatePlaybackState(m_playerController);

    const Track track      = m_track.isValid() ? m_track : m_placeholderTrack;
    const auto result      = m_parser.evaluate(script, track, m_scriptContext);
    const auto formatted   = m_formatter.evaluate(result);
    const QString htmlBody = richTextToHtml(formatted);
    const QString html     = u"<html><body style=\"margin:0;\">%1</body></html>"_s.arg(htmlBody);

    m_results->setHtml(html);
}

void ScriptEditorPrivate::trackContextChanged()
{
    m_track = m_selectionController ? m_selectionController->selectedTrack() : Track{};
    if(!m_track.isValid() && m_playerController) {
        m_track = m_playerController->currentTrack();
    }
    updateResults();
}

void ScriptEditorPrivate::selectionChanged()
{
    const auto indexes = m_expressionTree->selectionModel()->selectedIndexes();
    if(indexes.empty()) {
        return;
    }

    const auto* item = static_cast<ExpressionTreeItem*>(indexes.front().internalPointer());

    updateResults(item->expression());
}

void ScriptEditorPrivate::textChanged()
{
    m_textChangeTimer.start(TextChangeInterval, m_self);
    m_results->clear();

    m_currentScript = m_parser.parse(m_editor->toPlainText());
    m_model->populate(m_currentScript.expressions);
    updateResults();
}

void ScriptEditorPrivate::referenceSearchChanged(const QString& text)
{
    const QRegularExpression expression{QRegularExpression::escape(text), QRegularExpression::CaseInsensitiveOption};
    m_variableReferenceFilter->setFilterRegularExpression(expression);
    m_functionReferenceFilter->setFilterRegularExpression(expression);
    m_formattingReferenceFilter->setFilterRegularExpression(expression);
    m_commandReferenceFilter->setFilterRegularExpression(expression);
}

void ScriptEditorPrivate::referenceItemActivated(const QModelIndex& index)
{
    const QModelIndex itemIndex = index.siblingAtColumn(0);
    if(!itemIndex.isValid()) {
        return;
    }

    m_editor->insertSnippet(itemIndex.data(InsertTextRole).toString(), itemIndex.data(CursorOffsetRole).toInt(),
                            static_cast<ScriptReferenceKind>(itemIndex.data(KindRole).toInt()));
    m_editor->setFocus();
}

void ScriptEditorPrivate::referenceTabChanged(int index)
{
    if(index < 0) {
        return;
    }

    if(auto* tree = qobject_cast<QTreeView*>(m_referenceTabs->widget(index))) {
        tree->setFocus();
    }
}

void ScriptEditorPrivate::showErrors() const
{
    const auto errors = m_currentScript.errors;
    for(const ScriptError& error : errors) {
        m_results->append(error.message.toHtmlEscaped());
    }
}

void ScriptEditorPrivate::saveState()
{
    QByteArray byteArray;
    QDataStream out(&byteArray, QIODevice::WriteOnly);

    out << m_self->size();
    out << m_mainSplitter->saveState();
    out << m_documentSplitter->saveState();
    out << m_editor->toPlainText();

    byteArray = qCompress(byteArray, 9);

    m_settings.setValue(DialogState, byteArray);
}

void ScriptEditorPrivate::restoreState()
{
    QByteArray byteArray = m_settings.value(DialogState).toByteArray();

    static auto defaultScript = u"%track%. %title%"_s;

    if(byteArray.isEmpty()) {
        m_editor->setPlainText(defaultScript);
        return;
    }

    byteArray = qUncompress(byteArray);

    QDataStream in(&byteArray, QIODevice::ReadOnly);

    QSize dialogSize;
    QByteArray mainSplitterState;
    QByteArray documentSplitterState;
    QString editorText;

    in >> dialogSize;
    in >> mainSplitterState;
    in >> documentSplitterState;
    in >> editorText;

    if(editorText.isEmpty()) {
        editorText = defaultScript;
    }

    m_self->resize(dialogSize);
    m_mainSplitter->restoreState(mainSplitterState);
    m_documentSplitter->restoreState(documentSplitterState);
    m_editor->setPlainText(editorText);
    m_editor->moveCursor(QTextCursor::End);

    textChanged();
    m_expressionTree->expandAll();

    updateResults();
}

ScriptEditor::ScriptEditor(LibraryManager* libraryManager, const Track& track, QWidget* parent)
    : QDialog{parent}
    , p{std::make_unique<ScriptEditorPrivate>(this, libraryManager, track)}
{
    setWindowTitle(tr("Script Editor"));
}

ScriptEditor::ScriptEditor(LibraryManager* libraryManager, TrackSelectionController* selectionController,
                           PlayerController* playerController, QWidget* parent)
    : QDialog{parent}
    , p{std::make_unique<ScriptEditorPrivate>(this, libraryManager,
                                              selectionController && selectionController->selectedTrack().isValid()
                                                  ? selectionController->selectedTrack()
                                              : playerController ? playerController->currentTrack()
                                                                 : Track{},
                                              selectionController, playerController)}
{
    setWindowTitle(tr("Script Editor"));
}

ScriptEditor::ScriptEditor(LibraryManager* libraryManager, QWidget* parent)
    : ScriptEditor{libraryManager, Track{}, parent}
{ }

ScriptEditor::ScriptEditor(const QString& script, const Track& track, QWidget* parent)
    : ScriptEditor{nullptr, track, parent}
{
    p->m_editor->setPlainText(script);
}

ScriptEditor::ScriptEditor(QWidget* parent)
    : ScriptEditor{nullptr, parent}
{ }

ScriptEditor::~ScriptEditor()
{
    p->saveState();
}

void ScriptEditor::openEditor(const QString& script, const std::function<void(const QString&)>& callback,
                              const Track& track)
{
    auto* editor = new ScriptEditor(script, track);
    editor->setAttribute(Qt::WA_DeleteOnClose);
    editor->setModal(true);

    QObject::connect(editor, &QDialog::finished,
                     [editor, callback]() { callback(editor->p->m_editor->toPlainText()); });

    editor->show();
}

QSize ScriptEditor::sizeHint() const
{
    return Utils::proportionateSize(this, 0.3, 0.3);
}

void ScriptEditor::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->m_textChangeTimer.timerId()) {
        p->m_textChangeTimer.stop();
        p->showErrors();
    }
    QDialog::timerEvent(event);
}
} // namespace Fooyin

#include "gui/scripting/moc_scripteditor.cpp"
#include "scripteditor.moc"
