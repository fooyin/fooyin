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

#include "importlayoutdialog.h"

#include <gui/layoutprovider.h>
#include <gui/theme/fytheme.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

using namespace Qt::StringLiterals;

namespace Fooyin {
ImportLayoutDialog::ImportLayoutDialog(FyLayout layout, const QString& path, LayoutProvider* layoutProvider,
                                       QWidget* parent)
    : QDialog{parent}
    , m_layout{std::move(layout)}
    , m_layoutProvider{layoutProvider}
    , m_loadedPath{QFileInfo{path}.absoluteFilePath()}
    , m_nameEdit{new QLineEdit(this)}
    , m_pathEdit{new QLineEdit(path, this)}
    , m_importColours{new QCheckBox(tr("Import colours"), this)}
    , m_importFonts{new QCheckBox(tr("Import fonts"), this)}
    , m_importWindowSize{new QCheckBox(tr("Import window size"), this)}
    , m_switchToLayout{new QCheckBox(tr("Switch to imported layout"), this)}
    , m_errorLabel{new QLabel(this)}
    , m_importButton{nullptr}
{
    setWindowTitle(tr("Import Layout"));

    QString name = m_layout.name();
    if(m_layoutProvider && m_layoutProvider->layoutByName(name).isValid()) {
        name = m_layoutProvider->uniqueLayoutName(name);
    }
    m_nameEdit->setText(name);

    m_switchToLayout->setChecked(false);
    updateOptions();

    const QColor errorColour{Qt::red};
    QPalette palette{m_errorLabel->palette()};
    palette.setColor(QPalette::WindowText, errorColour);
    m_errorLabel->setPalette(palette);

    auto* buttons  = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_importButton = buttons->button(QDialogButtonBox::Ok);
    if(m_importButton) {
        m_importButton->setText(tr("&Import"));
    }
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &ImportLayoutDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* browseButton = new QPushButton(tr("&Browse…"), this);
    QObject::connect(browseButton, &QPushButton::clicked, this, &ImportLayoutDialog::browse);
    QObject::connect(m_nameEdit, &QLineEdit::textChanged, this, &ImportLayoutDialog::updateImportButton);
    QObject::connect(m_pathEdit, &QLineEdit::textChanged, this, &ImportLayoutDialog::updateImportButton);
    QObject::connect(m_pathEdit, &QLineEdit::editingFinished, this, [this]() {
        const QString editedPath = QFileInfo{m_pathEdit->text()}.absoluteFilePath();
        if(editedPath != m_loadedPath) {
            loadLayout(editedPath, true);
        }
    });

    auto* mainLayout = new QGridLayout(this);

    int row{0};
    mainLayout->addWidget(new QLabel(tr("Name") + u":"_s, this), row, 0);
    mainLayout->addWidget(m_nameEdit, row++, 1, 1, 3);
    mainLayout->addWidget(new QLabel(tr("Path") + u":"_s, this), row, 0);
    mainLayout->addWidget(m_pathEdit, row, 1, 1, 2);
    mainLayout->addWidget(browseButton, row++, 3);
    mainLayout->addWidget(m_importColours, row++, 0, 1, 2);
    mainLayout->addWidget(m_importFonts, row++, 0, 1, 2);
    mainLayout->addWidget(m_importWindowSize, row++, 0, 1, 2);
    mainLayout->addWidget(m_switchToLayout, row++, 0, 1, 2);
    mainLayout->addWidget(m_errorLabel, row, 0, 1, 2);
    mainLayout->addWidget(buttons, row++, 2, 1, 2);
    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(row, 1);

    updateImportButton();
}

QSize ImportLayoutDialog::sizeHint() const
{
    QSize size    = QDialog::sizeHint();
    size.rwidth() = std::max(size.width(), 500);
    return size;
}

FyLayout ImportLayoutDialog::selectedLayout() const
{
    FyLayout layout{m_layout};
    FyLayout::ThemeOptions options;
    options.setFlag(FyLayout::SaveColours, m_importColours->isChecked());
    options.setFlag(FyLayout::SaveFonts, m_importFonts->isChecked());

    if(options == FyLayout::ThemeOptions{}) {
        layout.removeTheme();
    }
    else if(const FyTheme theme = layout.loadTheme(); theme.isValid()) {
        layout.saveTheme(theme, options);
    }
    else {
        layout.setThemeOptions(options);
    }

    if(!m_importWindowSize->isChecked()) {
        layout.removeWindowSize();
    }

    return layout;
}

void ImportLayoutDialog::accept()
{
    const QString path = QFileInfo{m_pathEdit->text()}.absoluteFilePath();
    if(path != m_loadedPath) {
        loadLayout(path, true);
        return;
    }

    const QString name = m_nameEdit->text().trimmed();
    if(name.isEmpty()) {
        m_errorLabel->setText(tr("Enter a layout name."));
        return;
    }
    if(m_layoutProvider && m_layoutProvider->layoutByName(name).isValid()) {
        m_errorLabel->setText(tr("A layout with this name already exists."));
        return;
    }

    QJsonObject json = m_layout.json();
    json["Name"_L1]  = name;
    m_layout         = FyLayout{name, json};
    QDialog::accept();
}

bool ImportLayoutDialog::loadLayout(const QString& path, bool updateName)
{
    QFile file{path};
    if(!file.open(QIODevice::ReadOnly)) {
        m_errorLabel->setText(tr("Layout file could not be opened."));
        updateImportButton();
        return false;
    }

    const FyLayout layout{file.readAll()};
    file.close();
    if(!layout.isValid()) {
        m_errorLabel->setText(tr("Layout file is invalid."));
        updateImportButton();
        return false;
    }

    m_layout     = layout;
    m_loadedPath = QFileInfo{path}.absoluteFilePath();

    if(updateName) {
        QString name = layout.name();
        if(m_layoutProvider && m_layoutProvider->layoutByName(name).isValid()) {
            name = m_layoutProvider->uniqueLayoutName(name);
        }
        m_nameEdit->setText(name);
    }

    m_errorLabel->clear();
    updateOptions();
    updateImportButton();
    return true;
}

void ImportLayoutDialog::browse()
{
    const QString path
        = QFileDialog::getOpenFileName(this, tr("Open Layout"), m_pathEdit->text(), tr("fooyin Layout (*.fyl)"),
                                       nullptr, QFileDialog::DontResolveSymlinks);
    if(path.isEmpty()) {
        return;
    }

    m_pathEdit->setText(path);
    loadLayout(path, true);
}

void ImportLayoutDialog::updateImportButton()
{
    if(!m_importButton) {
        return;
    }

    const QFileInfo file{m_pathEdit->text()};
    m_importButton->setEnabled(!m_nameEdit->text().trimmed().isEmpty() && file.isAbsolute() && file.isFile()
                               && file.isReadable());
}

void ImportLayoutDialog::updateOptions()
{
    const auto themeOptions = m_layout.themeOptions();
    m_importColours->setEnabled(themeOptions.testFlag(FyLayout::SaveColours));
    m_importColours->setChecked(m_importColours->isEnabled());
    m_importFonts->setEnabled(themeOptions.testFlag(FyLayout::SaveFonts));
    m_importFonts->setChecked(m_importFonts->isEnabled());

    const bool hasWindowSize = m_layout.appliesWindowSize() && m_layout.json().value("WindowSize"_L1).isObject();
    m_importWindowSize->setEnabled(hasWindowSize);
    m_importWindowSize->setChecked(false);
}

bool ImportLayoutDialog::switchToLayout() const
{
    return m_switchToLayout->isChecked();
}
} // namespace Fooyin

#include "moc_importlayoutdialog.cpp"
