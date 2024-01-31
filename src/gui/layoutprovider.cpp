/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "guipaths.h"

#include <utils/fileutils.h>

#include <QFileDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QString>

using namespace Qt::Literals::StringLiterals;

namespace {
bool checkFile(const QFileInfo& file)
{
    return file.exists() && file.isFile() && file.isReadable()
        && file.completeSuffix().compare("fyl"_L1, Qt::CaseInsensitive) == 0;
}

QByteArray layoutToJson(const Fooyin::Layout& layout)
{
    QJsonObject root;
    QJsonArray array;

    array.append(layout.json);

    root[layout.name] = array;

    return QJsonDocument(root).toJson();
}
} // namespace

namespace Fooyin {
struct LayoutProvider::Private
{
    LayoutList layouts;
    Layout currentLayout;
    QFile layoutFile{Gui::activeLayoutPath()};

    bool layoutExists(const QString& name) const
    {
        return std::ranges::any_of(layouts, [name](const Layout& layout) { return layout.name == name; });
    }

    std::optional<Layout> addLayout(const QByteArray& json, bool import = false)
    {
        auto layout = LayoutProvider::readLayout(json);
        if(!layout) {
            qInfo() << "Attempted to load an invalid layout";
            return {};
        }

        const auto existingLayout = std::ranges::find_if(
            std::as_const(layouts), [layout](const Layout& existing) { return existing.name == layout->name; });

        if(existingLayout != layouts.cend()) {
            if(import) {
                return *existingLayout;
            }
            qInfo() << "A layout with the same name (" << layout->name << ") already exists";
            return {};
        }

        layouts.push_back(layout.value());
        return layout;
    }
};

LayoutProvider::LayoutProvider()
    : p{std::make_unique<Private>()}
{
    loadCurrentLayout();
}

LayoutProvider::~LayoutProvider()
{
    saveCurrentLayout();
}

Layout LayoutProvider::currentLayout() const
{
    return p->currentLayout;
}

void LayoutProvider::changeLayout(const Layout& layout)
{
    std::ranges::replace_if(
        p->layouts, [layout](const Layout& existing) { return existing.name == layout.name; }, layout);

    p->currentLayout = layout;
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

    const QByteArray json = p->layoutFile.readAll();
    p->layoutFile.close();

    const auto layout = readLayout(json);
    if(!layout) {
        qInfo() << "Attempted to load an invalid layout";
        return;
    }

    if(p->layoutExists(layout->name)) {
        qInfo() << "A layout with the same name (" << layout->name << ") already exists";
        return;
    }

    p->currentLayout = layout.value();
}

void LayoutProvider::saveCurrentLayout()
{
    if(!p->layoutFile.open(QIODevice::WriteOnly)) {
        qCritical() << "Couldn't open layout file";
        return;
    }

    const QByteArray json = layoutToJson(p->currentLayout);

    p->layoutFile.write(json);
    p->layoutFile.close();
}

LayoutList LayoutProvider::layouts() const
{
    return p->layouts;
}

void LayoutProvider::findLayouts()
{
    QStringList files;
    QList<QDir> stack{Gui::layoutsPath()};

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
        QFile newLayout{file};
        const QFileInfo fileInfo{file};

        if(!checkFile(fileInfo)) {
            qInfo() << "Layout file is not valid.";
            return;
        }
        if(!newLayout.open(QIODevice::ReadOnly)) {
            qInfo() << "Couldn't open layout file.";
            return;
        }

        const QByteArray json = newLayout.readAll();
        newLayout.close();

        if(!json.isEmpty()) {
            p->addLayout(json);
        }
    }
}

void LayoutProvider::registerLayout(const QByteArray& json)
{
    p->addLayout(json);
}

std::optional<Layout> LayoutProvider::readLayout(const QByteArray& json)
{
    const auto doc = QJsonDocument::fromJson(json);

    if(doc.isEmpty() || !doc.isObject()) {
        return {};
    }

    const auto layoutObject = doc.object();

    if(layoutObject.empty() || layoutObject.size() > 1) {
        return {};
    }

    const auto nameIt = layoutObject.constBegin();

    if(!nameIt->isArray()) {
        return {};
    }

    const QString name = nameIt.key();

    if(name.isEmpty()) {
        return {};
    }

    const auto layout = nameIt->toArray();

    if(layout.empty() || !layout.first().isObject()) {
        return {};
    }

    return Layout{name, layout.first().toObject()};
}

std::optional<Layout> LayoutProvider::importLayout(const QString& path)
{
    QFile file{path};
    const QFileInfo fileInfo{file};

    if(!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open layout for reading: " << path;
        return {};
    }

    const QByteArray json = file.readAll();
    file.close();

    if(json.isEmpty()) {
        return {};
    }

    if(!Utils::File::isSamePath(fileInfo.absolutePath(), Gui::layoutsPath())) {
        const QString newFile = Gui::layoutsPath() + fileInfo.fileName();
        file.copy(newFile);
    }

    return p->addLayout(json, true);
}

bool LayoutProvider::exportLayout(const Layout& layout, const QString& path)
{
    QString filepath{path};
    if(!filepath.contains(".fyl"_L1)) {
        filepath += u".fyl"_s;
    }

    QFile file{filepath};
    if(!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    const QByteArray json = layoutToJson(layout);

    file.write(json);
    file.close();

    const QFileInfo fileInfo{filepath};
    if(Utils::File::isSamePath(fileInfo.absolutePath(), Gui::layoutsPath()) && !p->layoutExists(layout.name)) {
        p->layouts.push_back(layout);
    }

    return true;
}
} // namespace Fooyin
