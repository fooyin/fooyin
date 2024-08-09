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

#include "fileopsdialog.h"

#include "fileopsmodel.h"

#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTreeView>

constexpr auto CurrentPreset = "FileOps/CurrentPreset";
constexpr auto SavedPresets  = "FileOps/Presets";

namespace Fooyin::FileOps {
class FileOpsDialogPrivate : public QObject
{
    Q_OBJECT

public:
    FileOpsDialogPrivate(FileOpsDialog* self, MusicLibrary* library, const TrackList& tracks, Operation operation,
                         SettingsManager* settings);

    void setup();

    void changeOperation(Operation operation);
    void updateButtonState() const;

    [[nodiscard]] FileOpPreset currentPreset() const;

    void savePreset();
    void loadPreset();
    void deletePreset();

    void saveCurrentPreset() const;
    void savePresets() const;
    void loadCurrentPreset() const;
    void loadPresets();
    void populatePresets();

    void simulateOp() const;
    void toggleRun();
    void modelUpdated();

    void browseDestination() const;
    std::vector<FileOpPreset>::iterator findPreset(const QString& name);

    FileOpsDialog* m_self;
    SettingsManager* m_settings;

    Operation m_operation;

    QRadioButton* m_copyOp;
    QRadioButton* m_moveOp;
    QRadioButton* m_renameOp;

    QLineEdit* m_destination;
    QLineEdit* m_filename;
    QCheckBox* m_entireSource;
    QCheckBox* m_removeEmpty;

    QComboBox* m_presetBox;
    QPushButton* m_loadButton;
    QPushButton* m_saveButton;
    QPushButton* m_deleteButton;

    QTreeView* m_resultsTable;
    FileOpsModel* m_model;
    QLabel* m_status;
    QPushButton* m_runButton{nullptr};

    std::vector<FileOpPreset> m_presets;
    bool m_loading{false};
    bool m_running{false};
};

FileOpsDialogPrivate::FileOpsDialogPrivate(FileOpsDialog* self, MusicLibrary* library, const TrackList& tracks,
                                           Operation operation, SettingsManager* settings)
    : m_self{self}
    , m_settings{settings}
    , m_operation{operation}
    , m_copyOp{new QRadioButton(FileOpsDialog::tr("Copy"), self)}
    , m_moveOp{new QRadioButton(FileOpsDialog::tr("Move"), self)}
    , m_renameOp{new QRadioButton(FileOpsDialog::tr("Rename"), self)}
    , m_destination{new QLineEdit(self)}
    , m_filename{new QLineEdit(QStringLiteral("%filename%"), self)}
    , m_entireSource{new QCheckBox(self)}
    , m_removeEmpty{new QCheckBox(FileOpsDialog::tr("Remove empty source folders"), self)}
    , m_presetBox{new QComboBox(self)}
    , m_loadButton{new QPushButton(FileOpsDialog::tr("&Load"), self)}
    , m_saveButton{new QPushButton(FileOpsDialog::tr("&Save"), self)}
    , m_deleteButton{new QPushButton(FileOpsDialog::tr("&Delete"), self)}
    , m_resultsTable{new QTreeView(self)}
    , m_model{new FileOpsModel(library, tracks, m_settings, self)}
    , m_status{new QLabel(self)}
{ }

void FileOpsDialogPrivate::setup()
{
    auto* layout = new QGridLayout(m_self);

    auto* browseAction = new QAction(Utils::iconFromTheme(Constants::Icons::Options), {}, m_self);
    QObject::connect(browseAction, &QAction::triggered, this, &FileOpsDialogPrivate::browseDestination);
    m_destination->addAction(browseAction, QLineEdit::TrailingPosition);

    m_resultsTable->setModel(m_model);
    m_resultsTable->header()->setStretchLastSection(true);

    const auto firstHint = m_resultsTable->header()->sectionSizeHint(0);
    m_resultsTable->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_resultsTable->header()->resizeSection(0, static_cast<int>(firstHint * 1.6));
    m_resultsTable->header()->resizeSection(1, 200);
    m_resultsTable->header()->resizeSection(2, 300);

    m_copyOp->setChecked(m_operation == Operation::Copy);
    m_moveOp->setChecked(m_operation == Operation::Move);
    m_renameOp->setChecked(m_operation == Operation::Rename);

    m_presetBox->setEditable(true);

    auto* opLabel = new QLabel(tr("Operation") + QStringLiteral(":"));

    auto* destLabel     = new QLabel(tr("Destination") + QStringLiteral(":"));
    auto* filenameLabel = new QLabel(tr("Filename") + QStringLiteral(":"));

    auto* presetGroup  = new QGroupBox(tr("Presets"), m_self);
    auto* presetLayout = new QGridLayout(presetGroup);

    QObject::connect(m_loadButton, &QRadioButton::clicked, this, &FileOpsDialogPrivate::loadPreset);
    QObject::connect(m_saveButton, &QRadioButton::clicked, this, &FileOpsDialogPrivate::savePreset);
    QObject::connect(m_deleteButton, &QRadioButton::clicked, this, &FileOpsDialogPrivate::deletePreset);

    presetLayout->addWidget(m_presetBox, 0, 0, 1, 3);
    presetLayout->addWidget(m_loadButton, 1, 0);
    presetLayout->addWidget(m_saveButton, 1, 1);
    presetLayout->addWidget(m_deleteButton, 1, 2);
    presetLayout->setColumnStretch(3, 1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, m_self);
    m_runButton     = buttonBox->button(QDialogButtonBox::Apply);
    m_runButton->setText(tr("&Run"));
    auto updateRunButton = []() {
    };
    updateRunButton();
    QObject::connect(m_destination, &QLineEdit::textChanged, this, [updateRunButton]() { updateRunButton(); });
    QObject::connect(m_filename, &QLineEdit::textChanged, this, [updateRunButton]() { updateRunButton(); });
    QObject::connect(m_runButton, &QPushButton::clicked, this, &FileOpsDialogPrivate::toggleRun);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, m_self, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, m_self, &QDialog::reject);

    auto* opLayout = new QHBoxLayout();

    opLayout->addWidget(m_copyOp);
    opLayout->addWidget(m_moveOp);
    opLayout->addWidget(m_renameOp);

    auto* optionsLayout = new QHBoxLayout();

    optionsLayout->addWidget(m_entireSource);
    optionsLayout->addWidget(m_removeEmpty);
    optionsLayout->addStretch();

    int row{0};
    layout->addWidget(opLabel, row, 0);
    layout->addLayout(opLayout, row, 1);
    layout->addWidget(presetGroup, row++, 4, 3, 1);
    layout->addWidget(destLabel, row, 0);
    layout->addWidget(m_destination, row++, 1, 1, 3);
    layout->addWidget(filenameLabel, row, 0);
    layout->addWidget(m_filename, row++, 1, 1, 3);
    layout->addLayout(optionsLayout, row++, 0, 1, 6);
    layout->addWidget(m_resultsTable, row++, 0, 1, 5);
    layout->addWidget(m_status, row, 0, 1, 5);
    layout->addWidget(buttonBox, row, 4, 1, 2);
    layout->setColumnStretch(3, 1);

    QObject::connect(m_copyOp, &QRadioButton::clicked, m_model, [this]() { changeOperation(Operation::Copy); });
    QObject::connect(m_moveOp, &QRadioButton::clicked, m_model, [this]() { changeOperation(Operation::Move); });
    QObject::connect(m_renameOp, &QRadioButton::clicked, m_model, [this]() { changeOperation(Operation::Rename); });
    QObject::connect(m_destination, &QLineEdit::textChanged, this, &FileOpsDialogPrivate::simulateOp);
    QObject::connect(m_filename, &QLineEdit::textChanged, this, &FileOpsDialogPrivate::simulateOp);
    QObject::connect(m_entireSource, &QCheckBox::clicked, this, &FileOpsDialogPrivate::simulateOp);
    QObject::connect(m_removeEmpty, &QCheckBox::clicked, this, &FileOpsDialogPrivate::simulateOp);
    QObject::connect(m_presetBox, &QComboBox::currentTextChanged, this, &FileOpsDialogPrivate::updateButtonState);

    QObject::connect(m_model, &FileOpsModel::simulated, this, &FileOpsDialogPrivate::modelUpdated);
    QObject::connect(m_model, &QAbstractItemModel::rowsRemoved, this, &FileOpsDialogPrivate::modelUpdated);

    changeOperation(m_operation);
    loadPresets();
    loadCurrentPreset();
    updateButtonState();

    m_self->resize(720, 480);
}

void FileOpsDialogPrivate::changeOperation(Operation operation)
{
    m_operation = operation;
    m_destination->setDisabled(operation == Operation::Rename);
    m_removeEmpty->setEnabled(operation == Operation::Move);
    m_entireSource->setEnabled(operation != Operation::Rename);
    m_entireSource->setText(operation == Operation::Copy ? tr("Copy entire source folder contents")
                                                         : tr("Move entire source folder contents"));
    populatePresets();
    simulateOp();
}

void FileOpsDialogPrivate::updateButtonState() const
{
    m_loadButton->setEnabled(m_presetBox->findText(m_presetBox->currentText()) >= 0);
    m_saveButton->setEnabled(!m_presetBox->currentText().isEmpty());
    m_deleteButton->setEnabled(m_presetBox->findText(m_presetBox->currentText()) >= 0);
}

FileOpPreset FileOpsDialogPrivate::currentPreset() const
{
    FileOpPreset preset;
    preset.op          = m_operation;
    preset.dest        = m_destination->text();
    preset.filename    = m_filename->text();
    preset.wholeDir    = m_entireSource->isChecked();
    preset.removeEmpty = m_removeEmpty->isChecked();

    return preset;
}

void FileOpsDialogPrivate::savePreset()
{
    const QString name = m_presetBox->currentText();

    FileOpPreset preset = currentPreset();
    preset.name         = name;

    auto existingPreset = findPreset(name);
    if(existingPreset != m_presets.cend()) {
        QMessageBox msg{QMessageBox::Question, tr("Preset already exists"),
                        tr("Preset %1 already exists. Overwrite?").arg(name), QMessageBox::Yes | QMessageBox::No};
        if(msg.exec() == QMessageBox::Yes) {
            *existingPreset = preset;
            m_presetBox->setCurrentIndex(m_presetBox->findText(name));
        }
    }
    else {
        m_presets.push_back(preset);
        m_presetBox->addItem(preset.name);
        m_presetBox->setCurrentIndex(m_presetBox->count() - 1);
        updateButtonState();
    }
}

void FileOpsDialogPrivate::loadPreset()
{
    if(m_presetBox->count() == 0) {
        return;
    }

    m_loading = true;

    const QString name = m_presetBox->currentText();

    auto preset = findPreset(name);
    if(preset != m_presets.cend()) {
        m_destination->setText(preset->dest);
        m_filename->setText(preset->filename);
        m_entireSource->setChecked(preset->wholeDir);
        m_removeEmpty->setChecked(preset->removeEmpty);
    }

    m_loading = false;
    simulateOp();
}

void FileOpsDialogPrivate::deletePreset()
{
    if(m_presetBox->count() == 0) {
        return;
    }

    const QString name = m_presetBox->currentText();

    auto preset = findPreset(name);
    if(preset != m_presets.cend()) {
        m_presets.erase(preset);
        m_presetBox->removeItem(m_presetBox->findText(name));
    }
}

void FileOpsDialogPrivate::saveCurrentPreset() const
{
    QByteArray byteArray;
    QDataStream stream(&byteArray, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << currentPreset();

    byteArray = qCompress(byteArray, 9);

    m_settings->fileSet(CurrentPreset, byteArray);
}

void FileOpsDialogPrivate::savePresets() const
{
    QByteArray byteArray;
    QDataStream stream(&byteArray, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<qint32>(m_presets.size());

    for(const auto& preset : m_presets) {
        stream << preset;
    }

    byteArray = qCompress(byteArray, 9);

    m_settings->fileSet(SavedPresets, byteArray);
}

void FileOpsDialogPrivate::loadCurrentPreset() const
{
    auto byteArray = m_settings->fileValue(CurrentPreset).toByteArray();

    if(!byteArray.isEmpty()) {
        byteArray = qUncompress(byteArray);

        QDataStream stream(&byteArray, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_6_0);

        FileOpPreset preset;
        stream >> preset;

        m_destination->setText(preset.dest);
        m_filename->setText(preset.filename);
        m_entireSource->setChecked(preset.wholeDir);
        m_removeEmpty->setChecked(preset.removeEmpty);

        simulateOp();
    }
}

void FileOpsDialogPrivate::loadPresets()
{
    auto byteArray = m_settings->fileValue(SavedPresets).toByteArray();

    if(!byteArray.isEmpty()) {
        byteArray = qUncompress(byteArray);

        QDataStream stream(&byteArray, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_6_0);

        qint32 size;
        stream >> size;

        while(size > 0) {
            --size;

            FileOpPreset preset;
            stream >> preset;

            m_presets.push_back(preset);
        }
    }

    populatePresets();
}

void FileOpsDialogPrivate::populatePresets()
{
    m_presetBox->clear();

    for(const auto& preset : m_presets) {
        if(preset.op == m_operation) {
            m_presetBox->addItem(preset.name);
        }
    }
}

void FileOpsDialogPrivate::simulateOp() const
{
    if(m_loading) {
        return;
    }

    FileOpPreset preset;
    preset.op          = m_operation;
    preset.dest        = m_destination->text();
    preset.filename    = m_filename->text();
    preset.wholeDir    = m_entireSource->isChecked();
    preset.removeEmpty = m_removeEmpty->isChecked();

    m_model->simulate(preset);

    m_status->setText(FileOpsDialog::tr("Determining operations…"));
}

void FileOpsDialogPrivate::toggleRun()
{
    if(m_running) {
        m_runButton->setText(tr("&Run"));
        m_model->stop();
    }
    else {
        m_runButton->setText(tr("&Abort"));
        m_model->run();
    }

    m_running = !m_running;
}

void FileOpsDialogPrivate::modelUpdated()
{
    const int opCount = m_model->rowCount({});

    if(opCount == 0) {
        m_runButton->setText(tr("&Run"));
        m_running = false;
        m_status->setText(FileOpsDialog::tr("Nothing to do"));
        m_runButton->setEnabled(false);
    }
    else if(m_operation != Operation::Rename && !QFileInfo{m_destination->text()}.isWritable()) {
        m_status->setText(FileOpsDialog::tr("Cannot write to %1").arg(m_destination->text()));
        m_runButton->setEnabled(false);
    }
    else {
        m_status->setText(FileOpsDialog::tr("Pending operations") + QStringLiteral(": %1").arg(opCount));
        m_runButton->setEnabled(true);
    }
}

void FileOpsDialogPrivate::browseDestination() const
{
    const QString path = !m_destination->text().isEmpty() ? m_destination->text() : QDir::homePath();
    const QString dir  = QFileDialog::getExistingDirectory(m_self, FileOpsDialog::tr("Select Directory"), path);
    if(!dir.isEmpty()) {
        m_destination->setText(dir);
    }
}

std::vector<FileOpPreset>::iterator FileOpsDialogPrivate::findPreset(const QString& name)
{
    return std::ranges::find_if(
        m_presets, [this, &name](const auto& preset) { return preset.op == m_operation && preset.name == name; });
}

FileOpsDialog::FileOpsDialog(MusicLibrary* library, const TrackList& tracks, Operation operation,
                             SettingsManager* settings, QWidget* parent)
    : QDialog{parent}
    , p{std::make_unique<FileOpsDialogPrivate>(this, library, tracks, operation, settings)}
{
    setWindowTitle(tr("File Operation"));
    setModal(true);

    p->setup();
}

void FileOpsDialog::done(int value)
{
    p->saveCurrentPreset();
    p->savePresets();
    QDialog::done(value);
}
} // namespace Fooyin::FileOps

#include "fileopsdialog.moc"
#include "moc_fileopsdialog.cpp"
