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

#include <gui/layoutprovider.h>

#include <gui/guipaths.h>

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QString>

namespace Fy::Gui {
bool checkFile(const QFileInfo& file)
{
    return file.exists() && file.isFile() && file.isReadable()
        && file.completeSuffix().compare("fyl", Qt::CaseInsensitive) == 0;
}

struct LayoutProvider::Private
{
    LayoutList layouts;
    Layout currentLayout;
    QFile layoutFile{Gui::activeLayoutPath()};

    bool layoutExists(const QString& name)
    {
        const auto layoutIt = std::ranges::find_if(std::as_const(layouts), [name](const Layout& layout) {
            return layout.name == name;
        });

        return layoutIt != layouts.cend();
    }

    void addLayout(const QString& file)
    {
        QFile layoutFile{file};
        const QFileInfo fileInfo{file};

        if(layoutExists(fileInfo.baseName())) {
            qInfo() << "A layout with the same name already exists";
            return;
        }
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
            layouts.emplace_back(layout);
        }
    }
};

LayoutProvider::LayoutProvider()
    : p{std::make_unique<Private>()}
{
    loadCurrentLayout();
}

LayoutProvider::~LayoutProvider() = default;

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
        p->addLayout(file);
    }
}

Layout LayoutProvider::currentLayout() const
{
    return p->currentLayout;
}

void LayoutProvider::loadCurrentLayout()
{
    if(!p->layoutFile.exists()) {
        return;
    }

    if(!p->layoutFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Couldn't open layout file.";
        return;
    }

    const QByteArray layout = p->layoutFile.readAll();
    p->layoutFile.close();

    p->currentLayout = {"Default", layout};
}

void LayoutProvider::saveCurrentLayout(const QByteArray& json)
{
    if(!p->layoutFile.open(QIODevice::WriteOnly)) {
        qCritical() << "Couldn't open layout file";
        return;
    }

    p->layoutFile.write(json);
    p->layoutFile.close();
}

LayoutList LayoutProvider::layouts() const
{
    return p->layouts;
}

void LayoutProvider::registerLayout(const QString& name, const QByteArray& json)
{
    if(name.isEmpty() || json.isEmpty()) {
        qInfo() << "Layout name or json empty";
        return;
    }

    if(p->layoutExists(name)) {
        qInfo() << "A layout with the same name already exists";
        return;
    }

    const Layout layout{name, json};
    p->layouts.emplace_back(layout);
}

void LayoutProvider::registerLayout(const QString& file)
{
    p->addLayout(file);
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
        file.copy(newFile);
        registerLayout(fileInfo.fileName(), json);
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
} // namespace Fy::Gui
