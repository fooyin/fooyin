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
#include "scripteditortextedit.h"
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
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/fontbutton.h>
#include <utils/utils.h>

#include <QApplication>
#include <QBasicTimer>
#include <QCheckBox>
#include <QCompleter>
#include <QDesktopServices>
#include <QDir>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QItemSelection>
#include <QLineEdit>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSignalBlocker>
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

constexpr auto DialogState             = "Interface/ScriptEditorState";
constexpr auto VariableColourKey       = "Interface/ScriptEditor/VariableColour";
constexpr auto FunctionColourKey       = "Interface/ScriptEditor/FunctionColour";
constexpr auto ConditionalColourKey    = "Interface/ScriptEditor/ConditionalColour";
constexpr auto OperatorColourKey       = "Interface/ScriptEditor/OperatorColour";
constexpr auto QuotedTextColourKey     = "Interface/ScriptEditor/QuotedTextColour";
constexpr auto FormattingTagColourKey  = "Interface/ScriptEditor/FormattingTagColour";
constexpr auto FontKey                 = "Interface/ScriptEditor/Font";
constexpr auto WordWrapKey             = "Interface/ScriptEditor/WordWrap";
constexpr auto AutocompleteKey         = "Interface/ScriptEditor/Autocomplete";
constexpr auto FunctionHintsKey        = "Interface/ScriptEditor/FunctionHints";
constexpr auto ShowWhitespaceKey       = "Interface/ScriptEditor/ShowWhitespace";
constexpr auto HighlightBracketsKey    = "Interface/ScriptEditor/HighlightBrackets";
constexpr auto HighlightCurrentLineKey = "Interface/ScriptEditor/HighlightCurrentLine";
constexpr auto ShowLineNumbersKey      = "Interface/ScriptEditor/ShowLineNumbers";

namespace Fooyin {
namespace {
class ScriptReferenceFilterModel : public QSortFilterProxyModel
{
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

class ScriptEditorEnvironment : public ScriptEnvironment
{
public:
    explicit ScriptEditorEnvironment(LibraryManager* libraryManager)
        : m_libraryEnvironment{libraryManager}
    {
        m_libraryEnvironment.setEvaluationPolicy(TrackListContextPolicy::Unresolved, {}, true);
    }

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
        m_playbackEnvironment.setPlaybackState(
            playerController ? playerController->currentPosition() : 0,
            playerController ? playerController->currentTrack().duration() : 0,
            playerController ? playerController->bitrate() : 0,
            playerController ? playerController->playState() : Player::PlayState::Stopped,
            playerController ? playerController->decoder() : QString{},
            playerController ? playerController->playbackOutputInfo() : Engine::PlaybackOutputInfo{});
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
} // namespace

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
    void setupSettings();
    void updateSyntaxColours();
    void updateEditorSettings();

    void updateResults();
    void updateResults(const Expression& expression);

    void trackContextChanged();
    void selectionChanged();
    void textChanged();
    void referenceSearchChanged(const QString& text);
    void referenceItemActivated(const QModelIndex& index);
    void referenceTabChanged(int index);

    void showErrors();

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

    FontButton* m_font{nullptr};
    QCheckBox* m_wordWrap{nullptr};
    QCheckBox* m_autocomplete{nullptr};
    QCheckBox* m_functionHints{nullptr};
    QCheckBox* m_showWhitespace{nullptr};
    QCheckBox* m_highlightBrackets{nullptr};
    QCheckBox* m_highlightCurrentLine{nullptr};
    QCheckBox* m_showLineNumbers{nullptr};
    ColourButton* m_variableColour{nullptr};
    ColourButton* m_functionColour{nullptr};
    ColourButton* m_conditionalColour{nullptr};
    ColourButton* m_operatorColour{nullptr};
    ColourButton* m_quotedTextColour{nullptr};
    ColourButton* m_formattingTagColour{nullptr};

    QTreeView* m_expressionTree;
    QTabWidget* m_referenceTabs;
    QLineEdit* m_referenceSearch;
    QTreeView* m_variableReferenceTree;
    QTreeView* m_functionReferenceTree;
    QTreeView* m_formattingReferenceTree;
    QStandardItemModel* m_variableReferenceModel;
    QStandardItemModel* m_functionReferenceModel;
    QStandardItemModel* m_formattingReferenceModel;
    ScriptReferenceFilterModel* m_variableReferenceFilter;
    ScriptReferenceFilterModel* m_functionReferenceFilter;
    ScriptReferenceFilterModel* m_formattingReferenceFilter;
    ExpressionTreeModel* m_model;

    QBasicTimer m_textChangeTimer;

    ScriptParser m_parser;
    ScriptFormatter m_formatter;
    ScriptEditorEnvironment m_environment;
    ScriptContext m_scriptContext;

    ParsedScript m_currentScript;
    ErrorList m_formatErrors;
    bool m_errorsVisible{false};
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
    , m_variableReferenceModel{new QStandardItemModel(m_self)}
    , m_functionReferenceModel{new QStandardItemModel(m_self)}
    , m_formattingReferenceModel{new QStandardItemModel(m_self)}
    , m_variableReferenceFilter{new ScriptReferenceFilterModel(m_self)}
    , m_functionReferenceFilter{new ScriptReferenceFilterModel(m_self)}
    , m_formattingReferenceFilter{new ScriptReferenceFilterModel(m_self)}
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
    setupSettings();

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
    QObject::connect(m_editor, &QPlainTextEdit::textChanged, this, &ScriptEditorPrivate::textChanged);
    QObject::connect(m_model, &QAbstractItemModel::modelReset, m_expressionTree, &QTreeView::expandAll);
    QObject::connect(m_expressionTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &ScriptEditorPrivate::selectionChanged);

    QObject::connect(m_referenceSearch, &QLineEdit::textChanged, this, &ScriptEditorPrivate::referenceSearchChanged);
    const auto connectReferenceTree = [this](QTreeView* tree) {
        QObject::connect(tree, &QTreeView::doubleClicked, this, &ScriptEditorPrivate::referenceItemActivated);
    };
    for(auto* tree : {m_variableReferenceTree, m_functionReferenceTree, m_formattingReferenceTree}) {
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
        QObject::connect(m_selectionController, &TrackSelectionController::displaySelectionChanged, this,
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
        QObject::connect(m_playerController, &PlayerController::playbackOutputInfoChanged, this,
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

    for(auto* model : {m_variableReferenceModel, m_functionReferenceModel, m_formattingReferenceModel}) {
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

    m_referenceTabs->addTab(m_variableReferenceTree, ScriptEditor::tr("Variables"));
    m_referenceTabs->addTab(m_functionReferenceTree, ScriptEditor::tr("Functions"));
    m_referenceTabs->addTab(m_formattingReferenceTree, ScriptEditor::tr("Formatting"));

    m_referenceSearch->setPlaceholderText(ScriptEditor::tr("Filter"));
}

void ScriptEditorPrivate::setupSettings()
{
    const auto defaults = ScriptHighlighter::defaultColours();

    auto* settingsTab    = new QWidget(m_self);
    auto* settingsLayout = new QGridLayout(settingsTab);
    auto* editorGroup    = new QGroupBox(ScriptEditor::tr("Editor"), settingsTab);
    auto* editorLayout   = new QGridLayout(editorGroup);
    auto* coloursGroup   = new QGroupBox(ScriptEditor::tr("Syntax highlighting"), settingsTab);
    auto* coloursLayout  = new QGridLayout(coloursGroup);

    QFont defaultFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if(defaultFont.pointSize() <= 0) {
        defaultFont.setPointSize(QApplication::font().pointSize());
    }

    m_font                 = new FontButton(ScriptEditor::tr("Font") + u":"_s, true, editorGroup);
    m_wordWrap             = new QCheckBox(ScriptEditor::tr("Word wrap"), editorGroup);
    m_autocomplete         = new QCheckBox(ScriptEditor::tr("Autocomplete"), editorGroup);
    m_functionHints        = new QCheckBox(ScriptEditor::tr("Function parameter hints"), editorGroup);
    m_showWhitespace       = new QCheckBox(ScriptEditor::tr("Show whitespace"), editorGroup);
    m_highlightBrackets    = new QCheckBox(ScriptEditor::tr("Highlight matching brackets"), editorGroup);
    m_highlightCurrentLine = new QCheckBox(ScriptEditor::tr("Highlight current line"), editorGroup);
    m_showLineNumbers      = new QCheckBox(ScriptEditor::tr("Show line numbers"), editorGroup);

    const QVariant savedFont = m_settings.value(FontKey);
    const bool customFont    = savedFont.canConvert<QFont>();
    m_font->setButtonFont(customFont ? savedFont.value<QFont>() : defaultFont);
    m_font->setChecked(customFont);

    m_wordWrap->setChecked(m_settings.value(WordWrapKey, true).toBool());
    m_autocomplete->setChecked(m_settings.value(AutocompleteKey, true).toBool());
    m_functionHints->setChecked(m_settings.value(FunctionHintsKey, true).toBool());
    m_showWhitespace->setChecked(m_settings.value(ShowWhitespaceKey, false).toBool());
    m_highlightBrackets->setChecked(m_settings.value(HighlightBracketsKey, true).toBool());
    m_highlightCurrentLine->setChecked(m_settings.value(HighlightCurrentLineKey, true).toBool());
    m_showLineNumbers->setChecked(m_settings.value(ShowLineNumbersKey, false).toBool());

    int row{0};
    editorLayout->addWidget(m_font, row++, 0, 1, 2);
    for(auto* option : {m_wordWrap, m_autocomplete, m_functionHints, m_showWhitespace, m_highlightBrackets,
                        m_highlightCurrentLine, m_showLineNumbers}) {
        editorLayout->addWidget(option, row++, 0, 1, 2);
    }
    editorLayout->setColumnStretch(2, 1);

    QObject::connect(m_font, &FontButton::fontUpdated, this, [this](const QFont& font) {
        if(m_font->isChecked()) {
            m_settings.setValue(FontKey, font);
            updateEditorSettings();
        }
    });
    QObject::connect(m_font, &FontButton::toggled, this, [this](bool checked) {
        if(checked) {
            m_settings.setValue(FontKey, m_font->buttonFont());
        }
        else {
            m_settings.remove(FontKey);
        }
        updateEditorSettings();
    });

    const auto connectOption = [this](QCheckBox* option, const char* key) {
        QObject::connect(option, &QCheckBox::clicked, this, [this, key](bool checked) {
            m_settings.setValue(key, checked);
            updateEditorSettings();
        });
    };

    connectOption(m_wordWrap, WordWrapKey);
    connectOption(m_autocomplete, AutocompleteKey);
    connectOption(m_functionHints, FunctionHintsKey);
    connectOption(m_showWhitespace, ShowWhitespaceKey);
    connectOption(m_highlightBrackets, HighlightBracketsKey);
    connectOption(m_highlightCurrentLine, HighlightCurrentLineKey);
    connectOption(m_showLineNumbers, ShowLineNumbersKey);

    auto* resetEditor = new QPushButton(ScriptEditor::tr("Reset editor settings"), editorGroup);
    editorLayout->addWidget(resetEditor, row++, 0, 1, 2, Qt::AlignLeft);
    QObject::connect(resetEditor, &QPushButton::clicked, this, [this, defaultFont]() {
        const QSignalBlocker fontBlocker{m_font};

        m_font->setButtonFont(defaultFont);
        m_font->setChecked(false);
        m_wordWrap->setChecked(true);
        m_autocomplete->setChecked(true);
        m_functionHints->setChecked(true);
        m_showWhitespace->setChecked(false);
        m_highlightBrackets->setChecked(true);
        m_highlightCurrentLine->setChecked(true);
        m_showLineNumbers->setChecked(false);

        for(const char* key : {FontKey, WordWrapKey, AutocompleteKey, FunctionHintsKey, ShowWhitespaceKey,
                               HighlightBracketsKey, HighlightCurrentLineKey, ShowLineNumbersKey}) {
            m_settings.remove(key);
        }
        m_settings.remove(u"Interface/ScriptEditor/FontFamily"_s);
        m_settings.remove(u"Interface/ScriptEditor/FontSize"_s);

        updateEditorSettings();
    });

    const auto createColourButton
        = [this, coloursGroup](const QString& label, const char* key, const QColor& defaultColour) {
              const QVariant savedColour = m_settings.value(key);
              const bool custom          = savedColour.canConvert<QColor>() && savedColour.value<QColor>().isValid();
              auto* button = new ColourButton(label + u":"_s, custom ? savedColour.value<QColor>() : defaultColour,
                                              true, coloursGroup);
              button->setChecked(custom);

              QObject::connect(button, &ColourButton::colourUpdated, this, [this, button, key](const QColor& colour) {
                  if(button->isChecked()) {
                      m_settings.setValue(key, colour);
                      updateSyntaxColours();
                  }
              });
              QObject::connect(button, &ColourButton::toggled, this, [this, button, key](bool checked) {
                  if(checked) {
                      m_settings.setValue(key, button->colour());
                  }
                  else {
                      m_settings.remove(key);
                  }
                  updateSyntaxColours();
              });

              return button;
          };

    m_variableColour = createColourButton(ScriptEditor::tr("Variables"), VariableColourKey, defaults.variable);
    m_functionColour = createColourButton(ScriptEditor::tr("Functions"), FunctionColourKey, defaults.function);
    m_conditionalColour
        = createColourButton(ScriptEditor::tr("Conditionals"), ConditionalColourKey, defaults.conditional);
    m_operatorColour   = createColourButton(ScriptEditor::tr("Operators"), OperatorColourKey, defaults.operatorColour);
    m_quotedTextColour = createColourButton(ScriptEditor::tr("Quoted text"), QuotedTextColourKey, defaults.quotedText);
    m_formattingTagColour
        = createColourButton(ScriptEditor::tr("Formatting tags"), FormattingTagColourKey, defaults.formattingTag);

    ColourButton::alignLabels({m_variableColour, m_functionColour, m_conditionalColour, m_operatorColour,
                               m_quotedTextColour, m_formattingTagColour});

    row = 0;
    for(auto* button : {m_variableColour, m_functionColour, m_conditionalColour, m_operatorColour, m_quotedTextColour,
                        m_formattingTagColour}) {
        coloursLayout->addWidget(button, row++, 0);
    }

    auto* resetColours = new QPushButton(ScriptEditor::tr("Reset colours"), coloursGroup);
    coloursLayout->addWidget(resetColours, row++, 0, Qt::AlignLeft);
    coloursLayout->setRowStretch(row, 1);

    QObject::connect(resetColours, &QPushButton::clicked, this, [this]() {
        const auto resetButton = [this](ColourButton* button, const char* key, const QColor& colour) {
            const QSignalBlocker blocker{button};
            button->setChecked(false);
            button->setColour(colour);
            m_settings.remove(key);
        };

        const auto defaultColours = ScriptHighlighter::defaultColours();
        resetButton(m_variableColour, VariableColourKey, defaultColours.variable);
        resetButton(m_functionColour, FunctionColourKey, defaultColours.function);
        resetButton(m_conditionalColour, ConditionalColourKey, defaultColours.conditional);
        resetButton(m_operatorColour, OperatorColourKey, defaultColours.operatorColour);
        resetButton(m_quotedTextColour, QuotedTextColourKey, defaultColours.quotedText);
        resetButton(m_formattingTagColour, FormattingTagColourKey, defaultColours.formattingTag);
        updateSyntaxColours();
    });

    settingsLayout->addWidget(editorGroup, 0, 0);
    settingsLayout->addWidget(coloursGroup, 1, 0);
    settingsLayout->setRowStretch(2, 1);
    m_sideTabs->addTab(settingsTab, ScriptEditor::tr("Settings"));

    updateSyntaxColours();
    updateEditorSettings();
}

void ScriptEditorPrivate::updateSyntaxColours()
{
    const auto defaults = ScriptHighlighter::defaultColours();
    m_highlighter.setColours({
        .variable       = m_variableColour->isChecked() ? m_variableColour->colour() : defaults.variable,
        .function       = m_functionColour->isChecked() ? m_functionColour->colour() : defaults.function,
        .conditional    = m_conditionalColour->isChecked() ? m_conditionalColour->colour() : defaults.conditional,
        .operatorColour = m_operatorColour->isChecked() ? m_operatorColour->colour() : defaults.operatorColour,
        .quotedText     = m_quotedTextColour->isChecked() ? m_quotedTextColour->colour() : defaults.quotedText,
        .formattingTag  = m_formattingTagColour->isChecked() ? m_formattingTagColour->colour() : defaults.formattingTag,
    });
}

void ScriptEditorPrivate::updateEditorSettings()
{
    m_editor->setFont(m_font->buttonFont());
    m_editor->setLineWrapMode(m_wordWrap->isChecked() ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    m_editor->setAutocompleteEnabled(m_autocomplete->isChecked());
    m_editor->setFunctionHintsEnabled(m_functionHints->isChecked());
    m_editor->setWhitespaceVisible(m_showWhitespace->isChecked());
    m_editor->setMatchingBracketsHighlighted(m_highlightBrackets->isChecked());
    m_editor->setCurrentLineHighlighted(m_highlightCurrentLine->isChecked());
    m_editor->setLineNumbersVisible(m_showLineNumbers->isChecked());
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
    m_formatErrors         = m_formatter.errors();
    const QString htmlBody = richTextToHtml(formatted);
    const QString html     = u"<html><body style=\"margin:0;\">%1</body></html>"_s.arg(htmlBody);

    m_results->setHtml(html);
    if(m_errorsVisible) {
        showErrors();
    }
}

void ScriptEditorPrivate::trackContextChanged()
{
    m_track = m_selectionController ? m_selectionController->displayTrack() : Track{};
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
    m_formatErrors.clear();
    m_errorsVisible = false;

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

void ScriptEditorPrivate::showErrors()
{
    m_errorsVisible = true;

    for(const ScriptError& error : m_currentScript.errors) {
        m_results->append(error.message.toHtmlEscaped());
    }
    for(const ScriptError& error : m_formatErrors) {
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

    static const QString defaultScript = u"%track%. %title%"_s;

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
                                              selectionController && selectionController->displayTrack().isValid()
                                                  ? selectionController->displayTrack()
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
                              const Track& track, QWidget* parent)
{
    auto* editor = new ScriptEditor(script, track, parent);
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
