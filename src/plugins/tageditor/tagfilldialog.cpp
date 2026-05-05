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

#include "tagfilldialog.h"
#include "tagfillpreviewmodel.h"

#include <core/constants.h>
#include <core/coresettings.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/utils.h>

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>

#include <ranges>
#include <utility>

using namespace Qt::StringLiterals;

constexpr auto FillDialogStateGroup        = "TagEditor/FillDialog"_L1;
constexpr auto FillDialogSourceHistoryKey  = "TagEditor/FillDialog/SourceHistory"_L1;
constexpr auto FillDialogPatternHistoryKey = "TagEditor/FillDialog/PatternHistory"_L1;
constexpr auto FillDialogSourceModeKey     = "TagEditor/FillDialog/SourceMode"_L1;
constexpr auto FillDialogSourceScriptKey   = "TagEditor/FillDialog/SourceScript"_L1;
constexpr auto FillDialogPatternKey        = "TagEditor/FillDialog/Pattern"_L1;
constexpr auto MaxFillDialogHistoryEntries = 10;

constexpr auto DefaultSourceScript = "%filename%"_L1;
constexpr auto DefaultPattern      = "%title%"_L1;

namespace Fooyin::TagEditor {
namespace {
QStringList historyForValue(const QString& value, const QStringList& existingHistory, const QString& requiredValue = {})
{
    QStringList history;

    const auto appendUnique = [&history](const QString& historyValue) {
        const QString trimmedHistoryValue = historyValue.trimmed();
        if(trimmedHistoryValue.isEmpty() || history.contains(trimmedHistoryValue)) {
            return;
        }

        history.push_back(trimmedHistoryValue);
    };

    appendUnique(value);

    for(const QString& existingValue : existingHistory) {
        appendUnique(existingValue);
        if(history.size() >= MaxFillDialogHistoryEntries) {
            break;
        }
    }

    const QString trimmedRequiredValue = requiredValue.trimmed();
    if(!trimmedRequiredValue.isEmpty() && !history.contains(trimmedRequiredValue)) {
        if(history.size() >= MaxFillDialogHistoryEntries && !history.isEmpty()) {
            history.removeLast();
        }
        history.push_back(trimmedRequiredValue);
    }

    return history;
}

QStringList sourceHistoryForValue(const QString& value, const QStringList& existingHistory)
{
    return historyForValue(value, existingHistory, DefaultSourceScript);
}

QStringList patternHistoryForValue(const QString& value, const QStringList& existingHistory)
{
    return historyForValue(value, existingHistory, DefaultPattern);
}

class FillValuesDialog : public QDialog
{
public:
    explicit FillValuesDialog(TrackList tracks, QWidget* parent = nullptr);

    [[nodiscard]] FillValuesResult fillResult() const;

    void saveState();

private:
    [[nodiscard]] FillSourceMode sourceMode() const;
    [[nodiscard]] QString sourceScript() const;
    [[nodiscard]] QString sourceColumnTitle() const;

    [[nodiscard]] bool autoCapitalise() const;
    [[nodiscard]] FillValuesOptions options() const;

    void loadHistory();
    void updateSourceScriptState() const;
    void updatePreview();

    static QString fillFieldColumnTitle(const QString& field);
    [[nodiscard]] static QStringList comboItems(const QComboBox* combo);

    TrackList m_tracks;
    QComboBox* m_sourceMode;
    ScriptComboBox* m_sourceScript;
    QComboBox* m_pattern;
    QLabel* m_previewLabel;
    QTableView* m_preview;
    FillPreviewModel* m_previewModel;
    QDialogButtonBox* m_buttons;
    QPushButton* m_autoCapitaliseButton;
};

FillValuesDialog::FillValuesDialog(TrackList tracks, QWidget* parent)
    : QDialog(parent)
    , m_tracks{std::move(tracks)}
    , m_sourceMode{new QComboBox(this)}
    , m_sourceScript{new ScriptComboBox(DefaultSourceScript, m_tracks.front(), this)}
    , m_pattern{new QComboBox(this)}
    , m_previewLabel{new QLabel(tr("Preview"), this)}
    , m_preview{new QTableView(this)}
    , m_previewModel{new FillPreviewModel(this)}
    , m_buttons{new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this)}
    , m_autoCapitaliseButton{new QPushButton(tr("Auto Capitalisation"), this)}
{
    setWindowTitle(tr("Automatically Fill Values"));
    setModal(true);

    auto* layout = new QGridLayout(this);

    m_sourceMode->addItem(tr("File names"), static_cast<int>(FillSourceMode::Filename));
    m_sourceMode->addItem(tr("Other…"), static_cast<int>(FillSourceMode::Other));
    m_sourceScript->setInsertPolicy(QComboBox::NoInsert);
    m_pattern->setEditable(true);
    m_pattern->setInsertPolicy(QComboBox::NoInsert);
    m_pattern->setToolTip(tr("Patterns use literal text and %field% captures.\n"
                             "This does not support full scripting."));

    m_previewLabel->setWordWrap(true);

    int row{0};
    layout->addWidget(new QLabel(tr("Source") + u":"_s, this), row, 0);
    layout->addWidget(m_sourceMode, row, 1);
    layout->addWidget(m_sourceScript, row++, 2);
    layout->addWidget(new QLabel(tr("Pattern") + u":"_s, this), row, 0);
    layout->addWidget(m_pattern, row++, 1, 1, 2);
    layout->addWidget(m_previewLabel, row++, 0, 1, 2);
    layout->addWidget(m_preview, row++, 0, 1, 3);
    layout->addWidget(m_buttons, row++, 0, 1, 3);
    layout->setColumnStretch(2, 1);
    layout->setRowStretch(4, 1);

    m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_preview->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_preview->setSelectionMode(QAbstractItemView::SingleSelection);
    m_preview->verticalHeader()->setVisible(false);
    m_preview->horizontalHeader()->setStretchLastSection(false);
    m_preview->setModel(m_previewModel);
    m_autoCapitaliseButton->setCheckable(true);
    m_buttons->addButton(m_autoCapitaliseButton, QDialogButtonBox::ActionRole);
    loadHistory();

    QObject::connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(m_sourceMode, &QComboBox::currentIndexChanged, this, [this]() {
        updateSourceScriptState();
        updatePreview();
    });
    QObject::connect(m_sourceScript, &QComboBox::currentTextChanged, this, &FillValuesDialog::updatePreview);
    QObject::connect(m_pattern, &QComboBox::currentTextChanged, this, &FillValuesDialog::updatePreview);
    QObject::connect(m_autoCapitaliseButton, &QPushButton::toggled, this, &FillValuesDialog::updatePreview);

    const FyStateSettings stateSettings;
    const QString geometryKey = QStringLiteral("%1/Geometry").arg(FillDialogStateGroup);
    const QString sizeKey     = QStringLiteral("%1/Size").arg(FillDialogStateGroup);
    if(stateSettings.contains(geometryKey) || stateSettings.contains(sizeKey)) {
        Utils::restoreState(this, stateSettings, FillDialogStateGroup);
    }
    else {
        resize(960, 460);
    }

    updateSourceScriptState();
    updatePreview();
}

FillValuesResult FillValuesDialog::fillResult() const
{
    return calculateFillValues(m_tracks, options());
}

void FillValuesDialog::saveState()
{
    FyStateSettings stateSettings;
    Utils::saveState(this, stateSettings, FillDialogStateGroup);
    stateSettings.setValue(FillDialogSourceModeKey, static_cast<int>(sourceMode()));
    stateSettings.setValue(FillDialogSourceScriptKey, sourceScript());
    stateSettings.setValue(FillDialogPatternKey, m_pattern->currentText());
    stateSettings.setValue(FillDialogSourceHistoryKey,
                           sourceHistoryForValue(m_sourceScript->currentText(), comboItems(m_sourceScript)));
    stateSettings.setValue(FillDialogPatternHistoryKey,
                           patternHistoryForValue(m_pattern->currentText(), comboItems(m_pattern)));
}

FillSourceMode FillValuesDialog::sourceMode() const
{
    return static_cast<FillSourceMode>(m_sourceMode->currentData().toInt());
}

QString FillValuesDialog::sourceScript() const
{
    return m_sourceScript->currentText();
}

QString FillValuesDialog::sourceColumnTitle() const
{
    switch(sourceMode()) {
        case FillSourceMode::Filename:
            return tr("File Name");
        case FillSourceMode::Other:
            return tr("Source");
    }

    return tr("Source");
}

bool FillValuesDialog::autoCapitalise() const
{
    return m_autoCapitaliseButton->isChecked();
}

FillValuesOptions FillValuesDialog::options() const
{
    return {.sourceMode     = sourceMode(),
            .sourceScript   = sourceScript(),
            .pattern        = m_pattern->currentText(),
            .autoCapitalise = autoCapitalise()};
}

void FillValuesDialog::loadHistory()
{
    const FyStateSettings stateSettings;

    const FillSourceMode savedSourceMode = static_cast<FillSourceMode>(
        stateSettings.value(FillDialogSourceModeKey, static_cast<int>(FillSourceMode::Filename)).toInt());
    const QString savedSourceScript = stateSettings.value(FillDialogSourceScriptKey, DefaultSourceScript).toString();
    const QString savedPattern      = stateSettings.value(FillDialogPatternKey, DefaultPattern).toString();

    const QStringList sourceHistory
        = sourceHistoryForValue(savedSourceScript, stateSettings.value(FillDialogSourceHistoryKey).toStringList());
    const QStringList patternHistory
        = patternHistoryForValue(savedPattern, stateSettings.value(FillDialogPatternHistoryKey).toStringList());

    m_sourceScript->addItems(sourceHistory);
    m_pattern->addItems(patternHistory);

    if(const int sourceModeIndex = m_sourceMode->findData(static_cast<int>(savedSourceMode)); sourceModeIndex >= 0) {
        m_sourceMode->setCurrentIndex(sourceModeIndex);
    }

    m_sourceScript->setCurrentText(savedSourceScript);
    m_pattern->setCurrentText(savedPattern);
}

void FillValuesDialog::updateSourceScriptState() const
{
    m_sourceScript->setEnabled(sourceMode() == FillSourceMode::Other);
}

void FillValuesDialog::updatePreview()
{
    const FillValuesResult result = fillResult();

    QStringList fieldTitles;
    fieldTitles.reserve(static_cast<qsizetype>(result.fields.size()));
    for(const auto& field : result.fields) {
        fieldTitles.push_back(fillFieldColumnTitle(field));
    }

    m_previewModel->setPreviewData(result, sourceColumnTitle(), fieldTitles);

    m_preview->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_preview->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for(int column{2}; column < m_previewModel->columnCount({}); ++column) {
        m_preview->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }

    if(auto* okButton = m_buttons->button(QDialogButtonBox::Ok)) {
        okButton->setEnabled(result.patternValid);
    }
}

QString FillValuesDialog::fillFieldColumnTitle(const QString& field)
{
    QString upperField = field.toUpper();

    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::Title}) {
        return tr("Track Title");
    }
    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::Artist}) {
        return tr("Artist Name");
    }
    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::Album}) {
        return tr("Album Title");
    }
    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::AlbumArtist}) {
        return tr("Album Artist");
    }
    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::TrackNumber}) {
        return tr("Track Number");
    }
    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::TrackTotal}) {
        return tr("Total Tracks");
    }
    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::Disc}) {
        return tr("Disc Number");
    }
    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::DiscTotal}) {
        return tr("Total Discs");
    }
    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::Genre}) {
        return tr("Genre");
    }
    if(upperField == "GENRES"_L1) {
        return tr("Genres");
    }
    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::Composer}) {
        return tr("Composer");
    }
    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::Performer}) {
        return tr("Performer");
    }
    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::Comment}) {
        return tr("Comment");
    }
    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::Date}) {
        return tr("Date");
    }
    if(upperField == QLatin1StringView{Fooyin::Constants::MetaData::Rating}) {
        return tr("Rating");
    }

    return upperField;
}

QStringList FillValuesDialog::comboItems(const QComboBox* combo)
{
    QStringList items;
    items.reserve(combo->count());

    for(int i{0}; i < combo->count(); ++i) {
        items.push_back(combo->itemText(i));
    }

    return items;
}
} // namespace

QDialog* openFillDialog(const TrackList& tracks, QWidget* parent,
                        std::function<void(const FillValuesResult&)> onAccepted)
{
    if(tracks.empty()) {
        return nullptr;
    }

    auto* dialog = new FillValuesDialog(tracks, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QDialog::accepted, dialog, [dialog, onAccepted = std::move(onAccepted)]() {
        dialog->saveState();
        if(onAccepted) {
            onAccepted(dialog->fillResult());
        }
    });

    QObject::connect(dialog, &QDialog::finished, dialog, [dialog](int result) {
        if(result != QDialog::Accepted) {
            dialog->saveState();
        }
    });

    dialog->open();
    return dialog;
}
} // namespace Fooyin::TagEditor
