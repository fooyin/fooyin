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

#include "exportlayoutdialog.h"

#include <gui/editablelayout.h>
#include <gui/guipaths.h>
#include <gui/layoutprovider.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace Fooyin {
ExportLayoutDialog::ExportLayoutDialog(EditableLayout* editableLayout, LayoutProvider* layoutProvider, QWidget* parent)
    : QDialog{parent}
    , m_editableLayout{editableLayout}
    , m_layoutProvider{layoutProvider}
    , m_nameEdit{new QLineEdit(this)}
    , m_pathEdit{new QLineEdit(this)}
    , m_saveWindowSize{new QCheckBox(tr("Save window size"), this)}
    , m_errorLabel{new QLabel(this)}
{
    setWindowTitle(tr("Export Layout"));

    const QColor errorColour{Qt::red};
    QPalette palette{m_errorLabel->palette()};
    palette.setColor(QPalette::WindowText, errorColour);
    m_errorLabel->setPalette(palette);

    auto* layout = new QGridLayout(this);

    auto* nameLabel = new QLabel(tr("Name") + QStringLiteral(":"), this);
    auto* pathLabel = new QLabel(tr("Path") + QStringLiteral(":"), this);

    auto* browseButton = new QPushButton(tr("&Browse…"), this);
    QObject::connect(browseButton, &QPushButton::pressed, this, &ExportLayoutDialog::exportLayout);

    m_pathEdit->setPlaceholderText(Gui::layoutsPath());

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if(QPushButton* okButton = buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText(tr("&Save"));
        auto updateSaveButton = [this, okButton]() {
            okButton->setEnabled(!m_nameEdit->text().isEmpty() && !m_pathEdit->text().isEmpty()
                                 && QDir::isAbsolutePath(m_pathEdit->text()));
        };
        updateSaveButton();
        QObject::connect(m_nameEdit, &QLineEdit::textChanged, this, [updateSaveButton]() { updateSaveButton(); });
        QObject::connect(m_pathEdit, &QLineEdit::textChanged, this, [updateSaveButton]() { updateSaveButton(); });
    }
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(nameLabel, 0, 0);
    layout->addWidget(m_nameEdit, 0, 1, 1, 3);
    layout->addWidget(pathLabel, 1, 0);
    layout->addWidget(m_pathEdit, 1, 1, 1, 2);
    layout->addWidget(browseButton, 1, 3);
    layout->addWidget(m_saveWindowSize, 2, 0, 1, 3);
    layout->addWidget(m_errorLabel, 4, 0, 1, 2);
    layout->addWidget(buttonBox, 4, 2, 1, 2);

    layout->setColumnStretch(1, 1);
    layout->setRowStretch(3, 1);
}

QSize ExportLayoutDialog::sizeHint() const
{
    QSize size    = QDialog::sizeHint();
    size.rwidth() = std::max(size.width(), 500);
    return size;
}

void ExportLayoutDialog::accept()
{
    auto layout = m_editableLayout->saveCurrentToLayout(m_nameEdit->text());
    if(!layout.isValid()) {
        m_errorLabel->setText(tr("Failed to save the current layout"));
        return;
    }

    if(m_saveWindowSize) {
        layout.saveWindowSize();
    }
    const bool success = m_layoutProvider->exportLayout(layout, m_pathEdit->text());
    if(success) {
        QDialog::accept();
        return;
    }

    m_errorLabel->setText(tr("Failed to export layout"));
}

void ExportLayoutDialog::exportLayout()
{
    const QString path = !m_pathEdit->text().isEmpty() ? m_pathEdit->text() : Gui::layoutsPath() + m_nameEdit->text();
    const QString saveFile
        = QFileDialog::getSaveFileName(this, tr("Save Layout"), path, tr("%1 Layout %2").arg(u"fooyin", u"(*.fyl)"));
    if(saveFile.isEmpty()) {
        return;
    }

    m_pathEdit->setText(saveFile);
    if(m_nameEdit->text().isEmpty()) {
        const QFileInfo info{saveFile};
        m_nameEdit->setText(info.completeBaseName());
    }
}
} // namespace Fooyin
