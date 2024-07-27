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

#include <gui/guipaths.h>
#include <utils/fileutils.h>
#include <utils/utils.h>

#include <QFileDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QString>

Q_LOGGING_CATEGORY(LAYOUT_PROV, "LayoutProvider")

namespace {
bool checkFile(const QFileInfo& file)
{
    return file.exists() && file.isFile() && file.isReadable()
        && file.suffix().compare(QStringLiteral("fyl"), Qt::CaseInsensitive) == 0;
}
} // namespace

namespace Fooyin {
class LayoutProviderPrivate
{
public:
    [[nodiscard]] bool layoutExists(const QString& name) const;
    FyLayout addLayout(const FyLayout& layout, bool import = false);

    LayoutList m_layouts;
    FyLayout m_currentLayout;
    QFile m_layoutFile{Gui::activeLayoutPath()};
};

bool LayoutProviderPrivate::layoutExists(const QString& name) const
{
    return std::ranges::any_of(m_layouts, [name](const FyLayout& layout) { return layout.name() == name; });
}

FyLayout LayoutProviderPrivate::addLayout(const FyLayout& layout, bool import)
{
    if(!layout.isValid()) {
        qCWarning(LAYOUT_PROV) << "Attempted to load an invalid layout";
        return {};
    }

    const auto existingLayout = std::ranges::find_if(
        std::as_const(m_layouts), [layout](const FyLayout& existing) { return existing.name() == layout.name(); });

    if(existingLayout != m_layouts.cend()) {
        if(import) {
            return *existingLayout;
        }
        qCWarning(LAYOUT_PROV) << "A layout with the same name (" << layout.name() << ") already exists";
        return {};
    }

    m_layouts.push_back(layout);
    return layout;
}

LayoutProvider::LayoutProvider(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<LayoutProviderPrivate>()}
{
    loadCurrentLayout();
}

LayoutProvider::~LayoutProvider() = default;

FyLayout LayoutProvider::currentLayout() const
{
    return p->m_currentLayout;
}

LayoutList LayoutProvider::layouts() const
{
    return p->m_layouts;
}

void LayoutProvider::findLayouts()
{
    QStringList files;
    QList<QDir> stack{Gui::layoutsPath()};

    while(!stack.isEmpty()) {
        const QDir dir     = stack.takeFirst();
        const auto subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for(const auto& subDir : subDirs) {
            stack.append(QDir{subDir.absoluteFilePath()});
        }

        const auto dirFiles = dir.entryInfoList({QStringLiteral("*.fyl")}, QDir::Files);
        for(const auto& file : dirFiles) {
            files.append(file.absoluteFilePath());
        }
    }

    for(const auto& file : files) {
        QFile newLayout{file};
        const QFileInfo fileInfo{file};

        if(!checkFile(fileInfo)) {
            qCInfo(LAYOUT_PROV) << "Layout file is not valid.";
            return;
        }
        if(!newLayout.open(QIODevice::ReadOnly)) {
            qCInfo(LAYOUT_PROV) << "Couldn't open layout file.";
            return;
        }

        const QByteArray json = newLayout.readAll();
        newLayout.close();

        if(!json.isEmpty()) {
            p->addLayout(FyLayout{json});
        }
    }
}

void LayoutProvider::loadCurrentLayout()
{
    if(!p->m_layoutFile.exists()) {
        return;
    }

    if(!p->m_layoutFile.open(QIODevice::ReadOnly)) {
        qCWarning(LAYOUT_PROV) << "Couldn't open layout file.";
        return;
    }

    const QByteArray json = p->m_layoutFile.readAll();
    p->m_layoutFile.close();

    const FyLayout layout{json};
    if(!layout.isValid()) {
        qCWarning(LAYOUT_PROV) << "Attempted to load an invalid layout";
        return;
    }

    if(p->layoutExists(layout.name())) {
        qCWarning(LAYOUT_PROV) << "A layout with the same name (" << layout.name() << ") already exists";
        return;
    }

    p->m_currentLayout = layout;
}

void LayoutProvider::saveCurrentLayout()
{
    if(!p->m_layoutFile.open(QIODevice::WriteOnly)) {
        qCWarning(LAYOUT_PROV) << "Couldn't open layout file";
        return;
    }

    const QByteArray json = QJsonDocument(p->m_currentLayout.json()).toJson();

    p->m_layoutFile.write(json);
    p->m_layoutFile.close();
}

void LayoutProvider::registerLayout(const FyLayout& layout)
{
    p->addLayout(layout);
}

void LayoutProvider::registerLayout(const QByteArray& json)
{
    p->addLayout(FyLayout{json});
}

void LayoutProvider::changeLayout(const FyLayout& layout)
{
    std::ranges::replace_if(
        p->m_layouts, [layout](const FyLayout& existing) { return existing.name() == layout.name(); }, layout);

    p->m_currentLayout = layout;
}

FyLayout LayoutProvider::importLayout(const QString& path)
{
    QFile file{path};
    const QFileInfo fileInfo{file};

    if(!file.open(QIODevice::ReadOnly)) {
        qCWarning(LAYOUT_PROV) << "Could not open layout for reading: " << path;
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

    return p->addLayout(FyLayout{json}, true);
}

void LayoutProvider::importLayout(QWidget* parent)
{
    const QString layoutFile
        = QFileDialog::getOpenFileName(parent, tr("Open Layout"), {}, tr("%1 Layout").arg(u"fooyin") + u" (*.fyl)");

    if(layoutFile.isEmpty()) {
        return;
    }

    const auto layout = importLayout(layoutFile);
    if(!layout.isValid()) {
        Utils::showMessageBox(tr("Invalid Layout"), tr("Layout could not be imported."));
        return;
    }

    QMessageBox message;
    message.setIcon(QMessageBox::Warning);
    message.setText(tr("Replace existing layout?"));
    message.setInformativeText(tr("Unless exported, the current layout will be lost."));

    message.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    message.setDefaultButton(QMessageBox::No);

    const int buttonClicked = message.exec();

    if(buttonClicked == QMessageBox::Yes) {
        emit requestChangeLayout(layout);
    }
}

bool LayoutProvider::exportLayout(const FyLayout& layout, const QString& path)
{
    QString filepath{path};
    if(!filepath.contains(QStringLiteral(".fyl"))) {
        filepath += QStringLiteral(".fyl");
    }

    QFile file{filepath};
    if(!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    const QByteArray json = QJsonDocument(layout.json()).toJson();

    const auto bytesWritten = file.write(json);
    file.close();

    if(bytesWritten < 0) {
        return false;
    }

    const QFileInfo fileInfo{filepath};
    if(Utils::File::isSamePath(fileInfo.absolutePath(), Gui::layoutsPath()) && !p->layoutExists(layout.name())) {
        p->m_layouts.push_back(layout);
        emit layoutAdded(layout);
    }

    return true;
}
} // namespace Fooyin
