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

#include "metadatalookupdialog.h"

#include "lookupresultsmodel.h"
#include "metadatachangesdialog.h"
#include "metadatalookupregistry.h"
#include "sources/metadatalookupsource.h"
#include "trackmatchmodel.h"

#include <core/engine/audioloader.h>
#include <core/library/musiclibrary.h>
#include <core/network/networkaccessmanager.h>
#include <gui/guiutils.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stringutils.h>

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

constexpr auto Geometry             = "MetadataLookup/DialogGeometry";
constexpr auto SplitterState        = "MetadataLookup/SplitterState";
constexpr auto ResultsSplitterState = "MetadataLookup/ResultsSplitterState";
constexpr auto MatchSplitterState   = "MetadataLookup/MatchSplitterState2";
constexpr auto LocalHeaderState     = "MetadataLookup/LocalHeaderState4";
constexpr auto RemoteHeaderState    = "MetadataLookup/RemoteHeaderState2";
constexpr auto ExistingPolicy       = "MetadataLookup/ExistingMetadataPolicy";
constexpr auto WriteGenres          = "MetadataLookup/WriteGenres";
constexpr auto WriteReleaseIds      = "MetadataLookup/WriteReleaseIds";
constexpr auto UseOriginalDate      = "MetadataLookup/UseOriginalReleaseDate";
constexpr auto Source               = "MetadataLookup/Source";
constexpr auto LookupModeSetting    = "MetadataLookup/LookupMode";

namespace Fooyin {
namespace {
QString commonValue(const TrackList& tracks, const std::function<QString(const Track&)>& getter)
{
    if(tracks.empty()) {
        return {};
    }

    const QString first = getter(tracks.front());
    return std::ranges::all_of(tracks, [&getter, &first](const Track& track) { return getter(track) == first; })
             ? first
             : QString{};
}

bool isDbOnlyMetadataTrack(const Track& track)
{
    return track.isRemote() && track.isInDatabase();
}

bool isIdentifierMode(LookupMode mode)
{
    return mode == LookupMode::ReleaseId || mode == LookupMode::ReleaseGroupId;
}

class EmptyStateTableView : public QTableView
{
public:
    explicit EmptyStateTableView(QString emptyText, QWidget* parent = nullptr)
        : QTableView{parent}
        , m_emptyText{std::move(emptyText)}
    { }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QTableView::paintEvent(event);

        if(model()->rowCount(rootIndex()) > 0) {
            return;
        }

        QPainter painter{viewport()};
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(viewport()->rect().adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap,
                         m_emptyText);
    }

private:
    QString m_emptyText;
};
} // namespace

MetadataLookupDialog::MetadataLookupDialog(TrackList tracks, MusicLibrary* library,
                                           std::shared_ptr<AudioLoader> audioLoader,
                                           std::shared_ptr<NetworkAccessManager> network, SettingsManager* settings,
                                           LookupMode mode, QWidget* parent)
    : MetadataLookupDialog{std::move(tracks),   library,  std::move(audioLoader),
                           std::move(network),  settings, mode,
                           Purpose::WriteFiles, {},       parent}
{ }

MetadataLookupDialog::MetadataLookupDialog(TrackList tracks, std::shared_ptr<NetworkAccessManager> network,
                                           SettingsManager* settings, const LookupQuery& initialQuery, QWidget* parent)
    : MetadataLookupDialog{std::move(tracks),     nullptr,      {},    std::move(network), settings, initialQuery.mode,
                           Purpose::ReturnTracks, initialQuery, parent}
{ }

MetadataLookupDialog::MetadataLookupDialog(TrackList tracks, MusicLibrary* library,
                                           std::shared_ptr<AudioLoader> audioLoader,
                                           std::shared_ptr<NetworkAccessManager> network, SettingsManager* settings,
                                           LookupMode mode, Purpose purpose,
                                           const std::optional<LookupQuery>& initialQuery, QWidget* parent)
    : QDialog{parent}
    , m_tracks{std::move(tracks)}
    , m_library{library}
    , m_audioLoader{std::move(audioLoader)}
    , m_network{std::move(network)}
    , m_settings{settings}
    , m_purpose{purpose}
    , m_registry{std::make_unique<MetadataLookupRegistry>(m_network.get())}
    , m_client{nullptr}
    , m_resultsModel{new LookupResultsModel(this)}
    , m_retrievedModel{new RetrievedTrackModel(this)}
    , m_matchModel{new TrackMatchModel(m_tracks, this)}
    , m_releaseLoaded{false}
    , m_busy{false}
    , m_writeInProgress{false}
    , m_writeCompleted{false}
    , m_syncingTrackScrollbars{false}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Metadata Lookup"));

    setMinimumSize(780, 520);
    resize(1050, 700);

    buildUi();

    for(auto* source : m_registry->sources()) {
        connectSource(source);
    }

    restoreState();
    if(initialQuery) {
        setLookupQuery(*initialQuery);
    }
    else {
        setLookupMode(mode);
    }

    QObject::connect(m_resultsView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
                     [this](const QModelIndex& current) { selectRelease(current); });
    QObject::connect(m_matchView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
                     [this](const QModelIndex& current) { selectMatchingRow(current.row()); });
    QObject::connect(m_retrievedView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
                     [this](const QModelIndex& current) { selectMatchingRow(current.row()); });
    QObject::connect(m_matchModel, &TrackMatchModel::mappingsChanged, this, &MetadataLookupDialog::updatePreview);
    QObject::connect(m_source, &QComboBox::currentIndexChanged, this,
                     [this] { setSource(m_source->currentData().toString()); });
    QObject::connect(m_lookupMode, &QComboBox::currentIndexChanged, this, &MetadataLookupDialog::updateLookupEditor);
    QObject::connect(m_idType, &QComboBox::currentIndexChanged, this, &MetadataLookupDialog::updateLookupEditor);
    QObject::connect(m_changesButton, &QPushButton::clicked, this, &MetadataLookupDialog::showChanges);
    QObject::connect(m_search, &QPushButton::clicked, this, &MetadataLookupDialog::startSearch);
    QObject::connect(m_updateFiles, &QPushButton::clicked, this, &MetadataLookupDialog::applyMetadata);
    QObject::connect(m_close, &QPushButton::clicked, this, &QDialog::close);
    QObject::connect(m_policy, &QComboBox::currentIndexChanged, this, [this] { updatePreview(); });
    QObject::connect(m_allowUnresolved, &QCheckBox::toggled, this, [this] { updateActions(); });
    QObject::connect(m_writeGenres, &QCheckBox::toggled, this, [this] { updatePreview(); });
    QObject::connect(m_writeIds, &QCheckBox::toggled, this, [this] { updatePreview(); });
    QObject::connect(m_originalDate, &QCheckBox::toggled, this, [this] { updatePreview(); });
    QObject::connect(m_retrievedView->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        if(m_syncingTrackScrollbars) {
            return;
        }
        m_syncingTrackScrollbars = true;
        m_matchView->verticalScrollBar()->setValue(value);
        m_syncingTrackScrollbars = false;
    });
    QObject::connect(m_matchView->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        if(m_syncingTrackScrollbars) {
            return;
        }
        m_syncingTrackScrollbars = true;
        m_retrievedView->verticalScrollBar()->setValue(value);
        m_syncingTrackScrollbars = false;
    });

    updateActions();
    m_search->click();
}

MetadataLookupDialog::~MetadataLookupDialog()
{
    saveState();
    if(m_client) {
        m_client->cancel();
    }
    if(m_cancelWrite) {
        m_cancelWrite();
    }
}

bool MetadataLookupDialog::hasSameTracks(const TrackList& tracks) const
{
    return std::ranges::equal(m_tracks, tracks,
                              [](const Track& lhs, const Track& rhs) { return lhs.sameIdentityAs(rhs); });
}

void MetadataLookupDialog::startLookup(LookupMode mode)
{
    setLookupMode(mode);
    startSearch();
}

void MetadataLookupDialog::closeEvent(QCloseEvent* event)
{
    if(m_writeInProgress) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void MetadataLookupDialog::buildUi()
{
    m_source = new QComboBox(this);
    for(const auto* source : m_registry->sources()) {
        m_source->addItem(source->name(), source->id());
    }

    m_lookupMode = new QComboBox(this);

    m_artist
        = new QLineEdit(commonValue(m_tracks, [](const Track& track) { return track.effectiveAlbumArtist(); }), this);
    m_album = new QLineEdit(commonValue(m_tracks, [](const Track& track) { return track.album(); }), this);

    m_discToc
        = new QLineEdit(commonValue(m_tracks, [](const Track& track) { return track.metaValue(u"DISCTOC"_s); }), this);
    m_discToc->setPlaceholderText(tr("First track, last track, lead-out sector, then track offsets"));

    m_releaseId = new QLineEdit(
        commonValue(m_tracks, [](const Track& track) { return track.metaValue(u"MUSICBRAINZ_ALBUMID"_s); }), this);
    m_releaseGroupId = new QLineEdit(
        commonValue(m_tracks, [](const Track& track) { return track.metaValue(u"MUSICBRAINZ_RELEASEGROUPID"_s); }),
        this);
    m_idType = new QComboBox(this);

    m_search = new QPushButton(tr("Search"), this);
    m_search->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    for(auto* editor : {m_artist, m_album, m_discToc, m_releaseId, m_releaseGroupId}) {
        editor->setClearButtonEnabled(true);
        QObject::connect(editor, &QLineEdit::returnPressed, m_search, &QPushButton::click);
    }

    auto* artistAlbumPage   = new QWidget(this);
    auto* artistAlbumLayout = new QHBoxLayout(artistAlbumPage);
    artistAlbumLayout->setContentsMargins({});
    artistAlbumLayout->addWidget(new QLabel(tr("Artist") + u":"_s, artistAlbumPage));
    artistAlbumLayout->addWidget(m_artist, 1);
    artistAlbumLayout->addWidget(new QLabel(tr("Album") + u":"_s, artistAlbumPage));
    artistAlbumLayout->addWidget(m_album, 1);

    auto* discTocPage   = new QWidget(this);
    auto* discTocLayout = new QHBoxLayout(discTocPage);
    discTocLayout->setContentsMargins({});
    discTocLayout->addWidget(new QLabel(tr("TOC") + u":"_s, discTocPage));
    discTocLayout->addWidget(m_discToc, 1);

    m_identifierEditor = new QStackedWidget(this);
    m_identifierEditor->addWidget(m_releaseId);
    m_identifierEditor->addWidget(m_releaseGroupId);

    auto* identifierPage   = new QWidget(this);
    auto* identifierLayout = new QHBoxLayout(identifierPage);
    identifierLayout->setContentsMargins({});
    identifierLayout->addWidget(new QLabel(tr("Type") + u":"_s, identifierPage));
    identifierLayout->addWidget(m_idType);
    identifierLayout->addWidget(new QLabel(tr("ID") + u":"_s, identifierPage));
    identifierLayout->addWidget(m_identifierEditor, 1);

    m_lookupEditor = new QStackedWidget(this);
    m_lookupEditor->addWidget(artistAlbumPage);
    m_lookupEditor->addWidget(discTocPage);
    m_lookupEditor->addWidget(identifierPage);

    auto* searchBox    = new QWidget(this);
    auto* searchLayout = new QGridLayout(searchBox);
    searchLayout->setContentsMargins({});
    searchLayout->addWidget(new QLabel(tr("Source") + u":"_s, searchBox), 0, 0);
    searchLayout->addWidget(m_source, 0, 1);
    searchLayout->addWidget(new QLabel(tr("Lookup by") + u":"_s, searchBox), 0, 2);
    searchLayout->addWidget(m_lookupMode, 0, 3);
    searchLayout->addWidget(m_search, 0, 5, 2, 1);
    searchLayout->addWidget(m_lookupEditor, 1, 0, 1, 5);
    searchLayout->setColumnStretch(4, 1);

    m_resultsView = new EmptyStateTableView(tr("Search for a release to see matching results"), this);
    m_resultsView->setModel(m_resultsModel);
    m_resultsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsView->setAlternatingRowColors(true);
    m_resultsView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultsView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_resultsView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_resultsView->verticalHeader()->hide();

    auto* releaseInfo       = new QGroupBox(tr("Release information"), this);
    auto* releaseInfoLayout = new QFormLayout(releaseInfo);
    releaseInfoLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    const auto addReleaseField = [releaseInfo, releaseInfoLayout](const QString& name) {
        auto* label = new QLabel(name + u":"_s, releaseInfo);
        label->setForegroundRole(QPalette::PlaceholderText);

        auto* value = new QLabel(releaseInfo);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setWordWrap(true);
        releaseInfoLayout->addRow(label, value);
        return value;
    };

    m_releaseArtist         = addReleaseField(tr("Artist"));
    m_releaseAlbum          = addReleaseField(tr("Album"));
    m_releaseDate           = addReleaseField(tr("Date"));
    m_releaseOriginalDate   = addReleaseField(tr("Original release date"));
    m_releaseCountry        = addReleaseField(tr("Country"));
    m_releaseLabel          = addReleaseField(tr("Label"));
    m_releaseCatalogNumber  = addReleaseField(tr("Catalogue number"));
    m_releaseBarcode        = addReleaseField(tr("Barcode"));
    m_releaseFormat         = addReleaseField(tr("Format"));
    m_releaseType           = addReleaseField(tr("Type"));
    m_releaseStatus         = addReleaseField(tr("Status"));
    m_releaseDisambiguation = addReleaseField(tr("Comment"));

    auto* resultsPage   = new QWidget(this);
    auto* resultsLayout = new QVBoxLayout(resultsPage);
    resultsLayout->setContentsMargins({});
    resultsLayout->addWidget(Gui::createSectionHeader(tr("Releases"), resultsPage));
    resultsLayout->addWidget(m_resultsView);

    m_resultsSplitter = new QSplitter(Qt::Horizontal, this);
    m_resultsSplitter->addWidget(resultsPage);
    m_resultsSplitter->addWidget(releaseInfo);
    m_resultsSplitter->setStretchFactor(0, 2);
    m_resultsSplitter->setStretchFactor(1, 1);
    m_resultsSplitter->setSizes({700, 350});

    m_retrievedView = new EmptyStateTableView(tr("Select a release above to load its tracks"), this);
    m_retrievedView->setModel(m_retrievedModel);
    m_retrievedView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_retrievedView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_retrievedView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_retrievedView->setAlternatingRowColors(true);
    m_retrievedView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_retrievedView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_retrievedView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_retrievedView->verticalHeader()->hide();

    m_matchView = new EmptyStateTableView(tr("No local tracks were selected"), this);
    m_matchView->setModel(m_matchModel);
    m_matchView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_matchView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_matchView->setDragDropMode(QAbstractItemView::InternalMove);
    m_matchView->setDragDropOverwriteMode(false);
    m_matchView->setDefaultDropAction(Qt::MoveAction);
    m_matchView->setDragEnabled(true);
    m_matchView->setDropIndicatorShown(true);
    m_matchView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_matchView->setAlternatingRowColors(true);
    m_matchView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_matchView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_matchView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_matchView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_matchView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_matchView->verticalHeader()->hide();

    auto* retrievedPage   = new QWidget(this);
    auto* retrievedLayout = new QVBoxLayout(retrievedPage);
    retrievedLayout->setContentsMargins({});
    retrievedLayout->addWidget(Gui::createSectionHeader(tr("Retrieved tracks"), retrievedPage));
    retrievedLayout->addWidget(m_retrievedView);

    auto* matchingPage   = new QWidget(this);
    auto* matchingLayout = new QVBoxLayout(matchingPage);
    matchingLayout->setContentsMargins({});

    auto* matchingHeader       = new QWidget(matchingPage);
    auto* matchingHeaderLayout = new QHBoxLayout(matchingHeader);
    matchingHeaderLayout->setContentsMargins({});

    auto* dragHint = new QLabel(tr("Drag rows to align tracks"), matchingHeader);
    dragHint->setForegroundRole(QPalette::PlaceholderText);

    matchingHeaderLayout->addWidget(Gui::createSectionHeader(tr("Local tracks"), matchingHeader));
    matchingHeaderLayout->addStretch();
    matchingHeaderLayout->addWidget(dragHint);
    matchingLayout->addWidget(matchingHeader);
    matchingLayout->addWidget(m_matchView);

    m_matchSplitter = new QSplitter(Qt::Horizontal, this);
    m_matchSplitter->addWidget(retrievedPage);
    m_matchSplitter->addWidget(matchingPage);
    m_matchSplitter->setStretchFactor(0, 1);
    m_matchSplitter->setStretchFactor(1, 1);
    m_matchSplitter->setSizes({500, 500});

    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->addWidget(m_resultsSplitter);
    m_splitter->addWidget(m_matchSplitter);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 3);
    m_splitter->setSizes({280, 360});

    m_policy = new QComboBox(this);
    m_policy->addItem(tr("Fill missing metadata"), static_cast<int>(ExistingMetadataPolicy::FillMissing));
    m_policy->addItem(tr("Replace lookup fields"), static_cast<int>(ExistingMetadataPolicy::ReplaceLookupFields));
    m_policy->addItem(tr("Wipe writable tags, then apply"), static_cast<int>(ExistingMetadataPolicy::WipeWritableTags));
    m_policy->setToolTip(tr("Choose how retrieved metadata is combined with the track's existing tags."));

    m_allowUnresolved = new QCheckBox(m_purpose == Purpose::ReturnTracks ? tr("Allow applying unresolved tracks")
                                                                         : tr("Allow updating unresolved tracks"),
                                      this);
    m_writeGenres     = new QCheckBox(tr("Write genres"), this);
    m_writeIds        = new QCheckBox(tr("Write provider IDs"), this);

    m_originalDate = new QCheckBox(tr("Use original date for Date"), this);
    m_allowUnresolved->setToolTip(
        m_purpose == Purpose::ReturnTracks
            ? tr("Allow applying metadata when local tracks are unmatched or have an ambiguous match.")
            : tr("Allow writing when local tracks are unmatched or have an ambiguous match."));
    m_writeGenres->setToolTip(tr("Write genres supplied by the selected metadata provider when available."));
    m_writeIds->setToolTip(tr("Write identifiers supplied by the selected metadata provider."));
    m_originalDate->setToolTip(
        tr("Use the original release date for the Date tag instead of this specific release's date."));

    m_allowUnresolved->hide();
    m_writeGenres->setChecked(true);
    m_writeIds->setChecked(true);

    auto* options       = new QGroupBox(tr("Metadata options"), this);
    auto* optionsLayout = new QGridLayout(options);
    options->setFlat(true);
    optionsLayout->addWidget(new QLabel(tr("Existing metadata") + u":"_s, options), 0, 0);
    optionsLayout->addWidget(m_policy, 0, 1);
    optionsLayout->addWidget(m_writeGenres, 0, 2);
    optionsLayout->addWidget(m_writeIds, 0, 3);
    optionsLayout->addWidget(m_originalDate, 0, 4);
    optionsLayout->setColumnStretch(1, 1);

    m_changesButton = new QPushButton(tr("Changes…"), this);
    m_changesButton->setEnabled(false);

    m_status = new QLabel(tr("Ready."), this);
    m_status->setWordWrap(true);

    m_buttons = new QDialogButtonBox(this);
    m_buttons->addButton(m_changesButton, QDialogButtonBox::ActionRole);
    m_updateFiles = m_buttons->addButton(m_purpose == Purpose::ReturnTracks ? tr("Apply") : tr("Update files"),
                                         QDialogButtonBox::AcceptRole);
    m_close       = m_buttons->addButton(QDialogButtonBox::Close);
    m_updateFiles->setDefault(true);

    auto* footer = new QGridLayout();
    footer->addWidget(options, 0, 0, 1, 3);
    footer->addWidget(m_status, 1, 0);
    footer->addWidget(m_allowUnresolved, 1, 1);
    footer->addWidget(m_buttons, 1, 2);
    footer->setColumnStretch(0, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(searchBox);
    layout->addWidget(m_splitter, 1);
    layout->addLayout(footer);
}

void MetadataLookupDialog::connectSource(MetadataLookupSource* source)
{
    QObject::connect(source, &MetadataLookupSource::searchFinished, this, [this, source](const auto& results) {
        if(source != m_client) {
            return;
        }
        m_resultsModel->setResults(results);
        m_status->setText(results.empty() ? tr("No matching releases found.")
                                          : tr("Found %Ln release(s).", nullptr, static_cast<int>(results.size())));
        if(!results.empty()) {
            m_resultsView->selectRow(0);
        }
    });
    QObject::connect(source, &MetadataLookupSource::releaseFetched, this, [this, source](const Release& release) {
        if(source == m_client) {
            setRelease(release);
        }
    });
    QObject::connect(source, &MetadataLookupSource::failed, this, [this, source](const QString& error) {
        if(source == m_client) {
            m_status->setText(error);
        }
    });
    QObject::connect(source, &MetadataLookupSource::busyChanged, this, [this, source](bool busy) {
        if(source != m_client) {
            return;
        }
        m_busy = busy;
        if(busy) {
            m_status->setText(tr("Contacting %1...").arg(source->name()));
        }
        updateActions();
    });
}

void MetadataLookupDialog::setSource(const QString& sourceId)
{
    MetadataLookupSource* source = m_registry->source(sourceId);
    if(m_client == source) {
        return;
    }

    if(m_client) {
        m_client->cancel();
    }

    m_client        = source;
    m_busy          = false;
    m_releaseLoaded = false;

    updateLookupModes();
    m_resultsModel->setResults({});
    m_retrievedModel->setRelease({});
    m_matchModel->setRelease({});
    updateReleaseInformation({});
    m_applyResult = {};

    m_changesButton->setText(tr("Changes…"));
    m_status->setText(tr("Ready."));
    updateActions();
}

void MetadataLookupDialog::updateLookupModes()
{
    const LookupMode previousMode = m_lookupMode->currentIndex() >= 0 ? query().mode : LookupMode::ArtistAlbum;
    const QSignalBlocker modeBlocker{m_lookupMode};
    const QSignalBlocker idBlocker{m_idType};

    m_lookupMode->clear();
    m_idType->clear();

    if(m_client) {
        const auto modes    = m_client->supportedModes();
        const auto supports = [&modes](LookupMode mode) {
            return std::ranges::find(modes, mode) != modes.cend();
        };

        if(supports(LookupMode::ArtistAlbum)) {
            m_lookupMode->addItem(tr("Artist and album"), static_cast<int>(LookupMode::ArtistAlbum));
        }
        if(supports(LookupMode::DiscToc)) {
            m_lookupMode->addItem(tr("Disc TOC"), static_cast<int>(LookupMode::DiscToc));
        }
        if(supports(LookupMode::ReleaseId)) {
            m_idType->addItem(tr("Release"), static_cast<int>(LookupMode::ReleaseId));
        }
        if(supports(LookupMode::ReleaseGroupId)) {
            m_idType->addItem(tr("Release group"), static_cast<int>(LookupMode::ReleaseGroupId));
        }
        if(m_idType->count() > 0) {
            m_lookupMode->addItem(tr("%1 ID").arg(m_client->name()), m_idType->itemData(0));
        }
    }

    setLookupMode(previousMode);
    m_lookupMode->setEnabled(m_lookupMode->count() > 1);
}

void MetadataLookupDialog::setLookupMode(LookupMode mode)
{
    int index = m_lookupMode->findData(static_cast<int>(mode));
    if(index < 0 && isIdentifierMode(mode)) {
        for(int i{0}; i < m_lookupMode->count(); ++i) {
            if(isIdentifierMode(static_cast<LookupMode>(m_lookupMode->itemData(i).toInt()))) {
                index = i;
                break;
            }
        }
    }
    m_lookupMode->setCurrentIndex(index >= 0 ? index : (m_lookupMode->count() > 0 ? 0 : -1));

    const int idIndex = m_idType->findData(static_cast<int>(mode));
    if(idIndex >= 0) {
        m_idType->setCurrentIndex(idIndex);
    }
    updateLookupEditor();
}

void MetadataLookupDialog::setLookupQuery(const LookupQuery& query)
{
    m_artist->setText(query.artist);
    m_album->setText(query.album);
    m_discToc->setText(query.discToc);
    m_discId = query.discId;

    if(query.mode == LookupMode::ReleaseGroupId) {
        m_releaseGroupId->setText(query.identifier);
    }
    else if(query.mode == LookupMode::ReleaseId) {
        m_releaseId->setText(query.identifier);
    }

    setLookupMode(query.mode);
}

void MetadataLookupDialog::updateLookupEditor()
{
    if(m_lookupMode->currentIndex() < 0) {
        m_lookupEditor->setEnabled(false);
        return;
    }

    m_lookupEditor->setEnabled(true);

    LookupMode mode = static_cast<LookupMode>(m_lookupMode->currentData().toInt());
    if(isIdentifierMode(mode) && m_idType->currentIndex() >= 0) {
        mode = static_cast<LookupMode>(m_idType->currentData().toInt());
    }

    m_lookupEditor->setCurrentIndex(mode == LookupMode::ArtistAlbum ? 0 : mode == LookupMode::DiscToc ? 1 : 2);
    if(isIdentifierMode(mode)) {
        m_identifierEditor->setCurrentIndex(mode == LookupMode::ReleaseId ? 0 : 1);
    }
}

void MetadataLookupDialog::saveState()
{
    m_settings->fileSet(Geometry, saveGeometry());
    m_settings->fileSet(SplitterState, m_splitter->saveState());
    m_settings->fileSet(ResultsSplitterState, m_resultsSplitter->saveState());
    m_settings->fileSet(MatchSplitterState, m_matchSplitter->saveState());
    m_settings->fileSet(LocalHeaderState, m_matchView->horizontalHeader()->saveState());
    m_settings->fileSet(RemoteHeaderState, m_retrievedView->horizontalHeader()->saveState());
    m_settings->fileSet(ExistingPolicy, m_policy->currentData().toInt());
    m_settings->fileSet(WriteGenres, m_writeGenres->isChecked());
    m_settings->fileSet(WriteReleaseIds, m_writeIds->isChecked());
    m_settings->fileSet(UseOriginalDate, m_originalDate->isChecked());
    m_settings->fileSet(Source, m_source->currentData().toString());
    m_settings->fileSet(LookupModeSetting, static_cast<int>(query().mode));
}

void MetadataLookupDialog::restoreState()
{
    const QByteArray geometry = m_settings->fileValue(Geometry).toByteArray();
    if(!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const QByteArray splitter = m_settings->fileValue(SplitterState).toByteArray();
    if(!splitter.isEmpty()) {
        m_splitter->restoreState(splitter);
    }
    const QByteArray resultsSplitter = m_settings->fileValue(ResultsSplitterState).toByteArray();
    if(!resultsSplitter.isEmpty()) {
        m_resultsSplitter->restoreState(resultsSplitter);
    }
    const QByteArray matchSplitter = m_settings->fileValue(MatchSplitterState).toByteArray();
    if(!matchSplitter.isEmpty()) {
        m_matchSplitter->restoreState(matchSplitter);
    }
    const QByteArray localHeader = m_settings->fileValue(LocalHeaderState).toByteArray();
    if(!localHeader.isEmpty()) {
        m_matchView->horizontalHeader()->restoreState(localHeader);
    }
    const QByteArray remoteHeader = m_settings->fileValue(RemoteHeaderState).toByteArray();
    if(!remoteHeader.isEmpty()) {
        m_retrievedView->horizontalHeader()->restoreState(remoteHeader);
    }

    const int policy
        = m_settings->fileValue(ExistingPolicy, static_cast<int>(ExistingMetadataPolicy::ReplaceLookupFields)).toInt();
    m_policy->setCurrentIndex(std::max(0, m_policy->findData(policy)));

    m_writeGenres->setChecked(m_settings->fileValue(WriteGenres, true).toBool());
    m_writeIds->setChecked(m_settings->fileValue(WriteReleaseIds, true).toBool());
    m_originalDate->setChecked(m_settings->fileValue(UseOriginalDate, false).toBool());

    const QString sourceId = m_settings->fileValue(Source).toString();
    const int sourceIndex  = m_source->findData(sourceId);
    m_source->setCurrentIndex(sourceIndex >= 0 ? sourceIndex : (m_source->count() > 0 ? 0 : -1));
    setSource(m_source->currentData().toString());

    setLookupMode(static_cast<LookupMode>(
        m_settings->fileValue(LookupModeSetting, static_cast<int>(LookupMode::ArtistAlbum)).toInt()));
}

void MetadataLookupDialog::startSearch()
{
    if(!m_client) {
        return;
    }

    m_releaseLoaded = false;
    m_resultsModel->setResults({});
    m_retrievedModel->setRelease({});
    m_matchModel->setRelease({});
    updateReleaseInformation({});
    m_applyResult = {};

    m_changesButton->setText(tr("Changes…"));
    m_client->search(query());
}

void MetadataLookupDialog::selectRelease(const QModelIndex& current)
{
    if(const auto* release = m_resultsModel->releaseAt(current.row())) {
        m_releaseLoaded = false;
        updateReleaseInformation(*release);
        updateActions();
        m_client->fetchRelease(release->id);
    }
}

void MetadataLookupDialog::setRelease(const Release& release)
{
    const QSignalBlocker unresolvedBlocker{m_allowUnresolved};

    Release selectedRelease{release};
    const LookupQuery lookupQuery = query();
    bool matchByPosition{false};

    // A TOC identifies one physical disc, so restrict multi-disc releases to the matching medium and align its tracks
    // by position
    if(lookupQuery.mode == LookupMode::DiscToc && !lookupQuery.discId.isEmpty()) {
        const auto medium     = std::ranges::find_if(release.media, [&lookupQuery](const ReleaseMedium& candidate) {
            return candidate.discIds.contains(lookupQuery.discId);
        });
        const bool hasDiscIds = std::ranges::any_of(
            release.media, [](const ReleaseMedium& candidate) { return !candidate.discIds.isEmpty(); });

        if(medium != release.media.cend()) {
            selectedRelease.media = {*medium};
            matchByPosition       = true;
        }
        else if(hasDiscIds) {
            m_releaseLoaded = false;
            m_retrievedModel->setRelease({});
            m_matchModel->setRelease({});
            m_applyResult = {};
            m_changesButton->setText(tr("Changes…"));
            m_status->setText(tr("The selected release does not contain the queried disc."));
            updateActions();
            return;
        }
        else if(release.media.size() == 1) {
            matchByPosition = true;
        }
    }

    m_allowUnresolved->setChecked(false);
    m_releaseLoaded = true;
    m_retrievedModel->setRelease(selectedRelease);
    m_matchModel->setRelease(selectedRelease, matchByPosition);
    updateReleaseInformation(selectedRelease.summary);

    if(m_matchModel->rowCount() > 0) {
        m_matchView->selectRow(0);
    }
    updatePreview();
}

void MetadataLookupDialog::updateReleaseInformation(const ReleaseSummary& release)
{
    const auto setValue = [](QLabel* label, const QString& value) {
        label->setText(value.isEmpty() ? u"—"_s : value);
    };

    setValue(m_releaseArtist, artistCreditText(release.artistCredit));
    setValue(m_releaseAlbum, release.title);
    setValue(m_releaseDate, release.date);
    setValue(m_releaseOriginalDate, release.originalDate);
    setValue(m_releaseCountry, release.country);
    setValue(m_releaseLabel, release.labels.join(u", "_s));
    setValue(m_releaseCatalogNumber, release.catalogNumbers.join(u", "_s));
    setValue(m_releaseBarcode, release.barcode);
    setValue(m_releaseFormat, release.formats.join(u", "_s));
    setValue(m_releaseType, release.releaseTypes.join(u", "_s));
    setValue(m_releaseStatus, release.status);
    setValue(m_releaseDisambiguation, release.disambiguation);
}

void MetadataLookupDialog::selectMatchingRow(int row)
{
    if(row < 0) {
        return;
    }

    const QSignalBlocker localBlocker{m_matchView->selectionModel()};
    const QSignalBlocker remoteBlocker{m_retrievedView->selectionModel()};

    if(row < m_matchModel->rowCount()) {
        m_matchView->selectRow(row);
    }
    if(row < m_retrievedModel->rowCount()) {
        m_retrievedView->selectRow(row);
    }
    else {
        m_retrievedView->selectionModel()->setCurrentIndex({}, QItemSelectionModel::Clear);
    }
}

void MetadataLookupDialog::updatePreview()
{
    if(!m_releaseLoaded) {
        m_applyResult = {};
        m_changesButton->setText(tr("Changes…"));
        updateActions();
        return;
    }

    m_applyResult = applyReleaseMetadata(m_tracks, m_matchModel->release(), m_matchModel->matches(), applyOptions());
    m_changesButton->setText(tr("Changes (%1)…").arg(m_applyResult.changes.size()));
    updateActions();
}

void MetadataLookupDialog::showChanges()
{
    if(m_applyResult.changes.empty()) {
        return;
    }

    auto* dialog = new MetadataChangesDialog(m_tracks, m_applyResult.changes, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(true);
    dialog->show();
}

void MetadataLookupDialog::updateActions()
{
    m_search->setEnabled(m_client && m_lookupMode->currentIndex() >= 0 && !m_busy && !m_writeInProgress);
    m_source->setEnabled(!m_busy && !m_writeInProgress);

    const bool canReorder = m_releaseLoaded && !m_busy && !m_writeInProgress;
    m_matchView->setDragEnabled(canReorder);
    m_matchView->viewport()->setCursor(canReorder ? Qt::OpenHandCursor : Qt::ArrowCursor);

    m_changesButton->setEnabled(!m_busy && !m_writeInProgress && !m_applyResult.changes.empty());
    m_close->setEnabled(!m_writeInProgress);

    const bool hasUnresolved = m_releaseLoaded && m_matchModel->hasUnresolved();
    m_allowUnresolved->setVisible(hasUnresolved);
    m_allowUnresolved->setEnabled(!m_busy && !m_writeInProgress);

    if(m_writeCompleted) {
        m_updateFiles->setEnabled(false);
        return;
    }

    const bool unresolvedAllowed = !m_matchModel->hasUnresolved() || m_allowUnresolved->isChecked();
    const bool canApply          = m_purpose == Purpose::ReturnTracks || canWriteAllTracks();
    const bool valid             = m_releaseLoaded && !m_busy && !m_writeInProgress && unresolvedAllowed
                                && !m_applyResult.tracks.empty() && canApply;
    m_updateFiles->setEnabled(valid);

    if(m_releaseLoaded && !canApply) {
        m_status->setText(tr("One or more selected tracks cannot be updated."));
    }
    else if(m_releaseLoaded && m_matchModel->hasUnresolved() && !m_allowUnresolved->isChecked()) {
        m_status->setText(m_purpose == Purpose::ReturnTracks
                              ? tr("Review unmatched or ambiguous tracks before applying metadata.")
                              : tr("Review unmatched or ambiguous tracks before updating files."));
    }
    else if(m_releaseLoaded && m_applyResult.tracks.empty()) {
        m_status->setText(tr("The selected release produces no metadata changes."));
    }
    else if(m_releaseLoaded && !m_busy) {
        m_status->setText(m_purpose == Purpose::ReturnTracks ? tr("Metadata will be applied to %Ln track(s).", nullptr,
                                                                  static_cast<int>(m_applyResult.tracks.size()))
                                                             : tr("%Ln track(s) will be updated.", nullptr,
                                                                  static_cast<int>(m_applyResult.tracks.size())));
    }
}

void MetadataLookupDialog::applyMetadata()
{
    updatePreview();

    if(!m_updateFiles->isEnabled() || !confirmMetadataWipe()) {
        return;
    }

    if(m_purpose == Purpose::WriteFiles) {
        writeMetadata();
        return;
    }

    TrackList tracks{m_tracks};

    Q_ASSERT(m_applyResult.tracks.size() == m_applyResult.trackIndices.size());

    for(size_t i{0}; i < m_applyResult.tracks.size(); ++i) {
        const size_t localIndex = m_applyResult.trackIndices.at(i);
        if(localIndex < tracks.size()) {
            tracks.at(localIndex) = m_applyResult.tracks.at(i);
        }
    }

    Q_EMIT tracksApplied(tracks);
    accept();
}

void MetadataLookupDialog::writeMetadata()
{
    WriteRequest request = m_library->writeTrackMetadata(m_applyResult.tracks);

    m_writeInProgress = true;
    m_cancelWrite     = request.cancel;

    m_progressDialog = new ElapsedProgressDialog(tr("Writing metadata…"), tr("Abort"), 0, 1, this);
    m_progressDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_progressDialog->setModal(true);
    m_progressDialog->setMinimumDuration(500ms);
    m_progressDialog->setBusy(true);
    m_progressDialog->setShowRemaining(false);
    m_progressDialog->setWindowTitle(tr("Writing Metadata"));
    m_progressDialog->setText(
        tr("Writing metadata to %Ln track(s)…", nullptr, static_cast<int>(m_applyResult.tracks.size())));
    m_progressDialog->startTimer();

    QObject::connect(m_progressDialog, &ElapsedProgressDialog::cancelled, this, [this] {
        if(m_cancelWrite) {
            m_cancelWrite();
        }
    });

    updateActions();

    request.finished.then(this, [this](const WriteResult& result) {
        if(m_progressDialog) {
            m_progressDialog->close();
        }

        m_writeInProgress = false;
        m_cancelWrite     = {};
        updateActions();

        if(result.state == WriteState::Cancelled) {
            const QString succeeded = tr("%Ln succeeded", nullptr, result.succeeded);
            const QString failed    = tr("%Ln failed", nullptr, result.failed);
            m_status->setText(tr("Metadata writing was cancelled.") + u" %1; %2."_s.arg(succeeded, failed));
        }
        else if(result.failed > 0) {
            const QString succeeded = tr("%Ln succeeded", nullptr, result.succeeded);
            const QString failed    = tr("%Ln failed", nullptr, result.failed);
            m_status->setText(tr("Metadata writing finished:") + u" %1; %2."_s.arg(succeeded, failed));
        }
        else {
            m_writeCompleted = true;
            m_status->setText(tr("Metadata was updated in %Ln track(s).", nullptr, result.succeeded));
            m_updateFiles->setEnabled(false);
        }
    });
}

bool MetadataLookupDialog::confirmMetadataWipe()
{
    return applyOptions().policy != ExistingMetadataPolicy::WipeWritableTags
        || QMessageBox::warning(this, tr("Wipe existing tags?"),
                                tr("Existing metadata and custom tags will be removed before applying the selected "
                                   "release. Ratings, ReplayGain, technical information, playback statistics, and "
                                   "artwork will be preserved."),
                                QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
               == QMessageBox::Yes;
}

LookupQuery MetadataLookupDialog::query() const
{
    LookupMode mode = m_lookupMode->currentIndex() >= 0 ? static_cast<LookupMode>(m_lookupMode->currentData().toInt())
                                                        : LookupMode::ArtistAlbum;
    if(isIdentifierMode(mode) && m_idType->currentIndex() >= 0) {
        mode = static_cast<LookupMode>(m_idType->currentData().toInt());
    }

    return {.mode       = mode,
            .artist     = m_artist->text().simplified(),
            .album      = m_album->text().simplified(),
            .discToc    = m_discToc->text().simplified(),
            .discId     = mode == LookupMode::DiscToc ? m_discId : QString{},
            .identifier = mode == LookupMode::ReleaseGroupId ? m_releaseGroupId->text().simplified()
                                                             : m_releaseId->text().simplified()};
}

MetadataApplyOptions MetadataLookupDialog::applyOptions() const
{
    return {.policy                 = static_cast<ExistingMetadataPolicy>(m_policy->currentData().toInt()),
            .writeGenres            = m_writeGenres->isChecked(),
            .writeReleaseIds        = m_writeIds->isChecked(),
            .useOriginalReleaseDate = m_originalDate->isChecked()};
}

bool MetadataLookupDialog::canWriteAllTracks() const
{
    return !m_tracks.empty() && std::ranges::all_of(m_tracks, [this](const Track& track) {
        return !track.hasCue() && !track.isInArchive()
            && (isDbOnlyMetadataTrack(track) || m_audioLoader->canWriteMetadata(track));
    });
}
} // namespace Fooyin

#include "moc_metadatalookupdialog.cpp"
