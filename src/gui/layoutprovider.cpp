/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "layoutprovider.h"

#include "guipaths.h"

#include <QDir>
#include <QFileDialog>

namespace Fy::Gui {
bool checkFile(const QFileInfo& file)
{
    return file.exists() && file.isFile() && file.isReadable()
        && file.completeSuffix().compare("fyl", Qt::CaseInsensitive) == 0;
}

LayoutProvider::LayoutProvider()
    : m_layoutFile{Gui::activeLayoutPath()}
{
    loadCurrentLayout();
}

void Gui::LayoutProvider::findLayouts()
{
    QStringList files;
    QList<QDir> stack{layoutsPath()};

    while(!stack.isEmpty()) {
        const QDir dir = stack.takeFirst();
        for(const auto& subDir : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            stack.append(QDir{subDir.absoluteFilePath()});
        }
        for(const auto& file : dir.entryInfoList({"*.fyl"}, QDir::Files)) {
            files.append(file.absoluteFilePath());
        }
    }

    for(const auto& file : files) {
        addLayout(file);
    }
}

Layout LayoutProvider::currentLayout() const
{
    return m_currentLayout;
}

void LayoutProvider::loadCurrentLayout()
{
    if(!m_layoutFile.exists()) {
        return;
    }

    if(!m_layoutFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Couldn't open layout file.";
        return;
    }

    const QByteArray layout = m_layoutFile.readAll();
    m_layoutFile.close();

    m_currentLayout = {"Default", layout};
}

void LayoutProvider::saveCurrentLayout(const QByteArray& json)
{
    if(!m_layoutFile.open(QIODevice::WriteOnly)) {
        qCritical() << "Couldn't open layout file.";
        return;
    }

    m_layoutFile.write(json);
    m_layoutFile.close();
}

LayoutList LayoutProvider::layouts() const
{
    return m_layouts;
}

void LayoutProvider::registerLayout(const QString& name, const QByteArray& json)
{
    if(name.isEmpty() || json.isEmpty()) {
        qInfo() << "Layout name or json empty.";
        return;
    }
    const Layout layout{name, json};
    m_layouts.emplace_back(layout);
}

void LayoutProvider::registerLayout(const QString& file)
{
    addLayout(file);
}

void LayoutProvider::importLayout()
{
    QFileDialog dialog;
    dialog.setFileMode(QFileDialog::ExistingFile);
    QString openFile = dialog.getOpenFileName(nullptr, "Open Layout", "", "Fooyin Layout (*.fyl)");
    if(openFile.isEmpty()) {
        return;
    }
    QFile file(openFile);
    if(!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open layout for reading: " << openFile;
        return;
    }
    const QByteArray json = file.readAll();
    file.close();

    if(!json.isEmpty()) {
        const QFileInfo fileInfo{file};
        const QString newFile = Gui::layoutsPath() + fileInfo.fileName();
        if(file.copy(newFile)) {
            registerLayout(fileInfo.fileName(), json);
        }
        else {
            qDebug() << "Could not copy layout to " << newFile;
        }
    }
}

void LayoutProvider::exportLayout(const QByteArray& json)
{
    QFileDialog dialog;
    dialog.setFileMode(QFileDialog::AnyFile);
    QString saveFile = dialog.getSaveFileName(nullptr, "Save Layout", Gui::layoutsPath(), "Fooyin Layout (*.fyl)");
    if(saveFile.isEmpty()) {
        return;
    }
    if(!saveFile.contains(".fyl")) {
        saveFile += ".fyl";
    }
    QFile file(saveFile);
    if(!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(json);
    file.close();
}

void LayoutProvider::addLayout(const QString& file)
{
    QFile layoutFile{file};
    const QFileInfo fileInfo{file};
    if(!checkFile(fileInfo)) {
        qInfo() << "Layout file is not valid.";
        return;
    }
    if(!layoutFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Couldn't open layout file.";
        return;
    }
    const QByteArray json = layoutFile.readAll();
    layoutFile.close();

    if(!json.isEmpty()) {
        const Layout layout{fileInfo.baseName(), json};
        m_layouts.emplace_back(layout);
    }
}
} // namespace Fy::Gui
