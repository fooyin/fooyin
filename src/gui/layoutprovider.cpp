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

#include <gui/layoutprovider.h>

#include <core/coresettings.h>
#include <gui/guipaths.h>
#include <utils/fileutils.h>
#include <utils/helpers.h>
#include <utils/utils.h>

#include <QFileDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QRegularExpression>
#include <QString>

Q_LOGGING_CATEGORY(LAYOUT_PROV, "fy.layoutprovider")

using namespace Qt::StringLiterals;

constexpr auto ActiveLayoutState = "Interface/ActiveLayout"_L1;

namespace Fooyin {
namespace {
QString layoutFilePath(const QString& name)
{
    QString filename = name.simplified();
    filename.replace(QRegularExpression{uR"([\\/:*?"<>|])"_s}, u"_"_s);
    if(filename.isEmpty()) {
        filename = u"Layout"_s;
    }
    if(!filename.endsWith(u".fyl"_s, Qt::CaseInsensitive)) {
        filename += u".fyl"_s;
    }
    return Gui::layoutsPath() + filename;
}

bool checkFile(const QFileInfo& file)
{
    return file.exists() && file.isFile() && file.isReadable()
        && file.suffix().compare(u"fyl"_s, Qt::CaseInsensitive) == 0;
}

FyLayout renamedLayout(const FyLayout& layout, const QString& name)
{
    if(!layout.isValid() || name.isEmpty()) {
        return {};
    }

    QJsonObject json = layout.json();
    json["Name"_L1]  = name;
    return FyLayout{name, json};
}

bool writeLayout(const FyLayout& layout, const QString& path)
{
    if(!layout.isValid() || path.isEmpty()) {
        return false;
    }

    QDir{}.mkpath(QFileInfo{path}.absolutePath());
    QFile file{path};
    if(!file.open(QIODevice::WriteOnly)) {
        qCWarning(LAYOUT_PROV) << "Couldn't open layout file";
        return false;
    }

    const QByteArray json = QJsonDocument(layout.json()).toJson();
    const auto written    = file.write(json);
    file.close();

    return written >= 0;
}

QString activeLayoutName()
{
    const FyStateSettings settings;
    return settings.value(QLatin1String{ActiveLayoutState}).toString();
}
} // namespace

class LayoutProviderPrivate
{
public:
    [[nodiscard]] LayoutList::iterator layout(const QString& name);

    FyLayout addLayout(const FyLayout& layout, bool import = false, const QString& path = {});
    void updateLayout(const FyLayout& layout);

    void resolveActiveLayout();
    void loadLegacyCurrentLayout();
    [[nodiscard]] QString pathForLayout(const FyLayout& layout) const;
    [[nodiscard]] bool isBuiltIn(const QString& name) const;

    void setLayoutPath(const FyLayout& layout, const QString& path);
    void saveActiveLayoutName() const;

    LayoutList m_layouts;
    std::map<QString, FyLayout> m_builtInLayouts;
    FyLayout m_currentLayout;
    std::map<QString, QString> m_layoutPaths;
    QFile m_legacyLayoutFile{Gui::activeLayoutPath()};
};

LayoutList::iterator LayoutProviderPrivate::layout(const QString& name)
{
    return std::ranges::find_if(m_layouts, [name](const FyLayout& layout) { return layout.name() == name; });
}

FyLayout LayoutProviderPrivate::addLayout(const FyLayout& layout, bool import, const QString& path)
{
    if(!layout.isValid()) {
        qCWarning(LAYOUT_PROV) << "Attempted to load an invalid layout";
        return {};
    }

    const auto existingLayout = this->layout(layout.name());

    if(existingLayout != m_layouts.cend()) {
        if(import) {
            return *existingLayout;
        }
        if(!path.isEmpty()) {
            *existingLayout = layout;
            setLayoutPath(layout, path);
            return layout;
        }
        qCWarning(LAYOUT_PROV) << "A layout with the same name (" << layout.name() << ") already exists";
        return {};
    }

    m_layouts.push_back(layout);
    setLayoutPath(layout, path);
    if(path.isEmpty()) {
        m_builtInLayouts[layout.name()] = layout;
    }
    return layout;
}

void LayoutProviderPrivate::updateLayout(const FyLayout& layout)
{
    if(!layout.isValid()) {
        return;
    }

    if(const auto existing = this->layout(layout.name()); existing != m_layouts.end()) {
        *existing = layout;
    }
    else {
        m_layouts.push_back(layout);
    }
}

void LayoutProviderPrivate::resolveActiveLayout()
{
    if(const QString activeName = activeLayoutName(); !activeName.isEmpty()) {
        if(m_currentLayout.name() == activeName) {
            updateLayout(m_currentLayout);
        }
        else if(const auto activeLayout = layout(activeName); activeLayout != m_layouts.end()) {
            m_currentLayout = *activeLayout;
        }
    }
    else if(m_currentLayout.isValid()) {
        updateLayout(m_currentLayout);
    }

    if(!m_currentLayout.isValid() && !m_layouts.empty()) {
        m_currentLayout = m_layouts.front();
    }

    updateLayout(m_currentLayout);
    saveActiveLayoutName();

    if(m_currentLayout.isValid() && pathForLayout(m_currentLayout).isEmpty()) {
        const QString path = layoutFilePath(m_currentLayout.name());
        setLayoutPath(m_currentLayout, path);
        writeLayout(m_currentLayout, path);
    }
}

void LayoutProviderPrivate::loadLegacyCurrentLayout()
{
    if(!m_legacyLayoutFile.exists()) {
        return;
    }

    if(!m_legacyLayoutFile.open(QIODevice::ReadOnly)) {
        qCWarning(LAYOUT_PROV) << "Couldn't open legacy layout file.";
        return;
    }

    const QByteArray json = m_legacyLayoutFile.readAll();
    m_legacyLayoutFile.close();

    const FyLayout layout{json};
    if(!layout.isValid()) {
        qCWarning(LAYOUT_PROV) << "Attempted to load an invalid legacy layout";
        return;
    }

    m_currentLayout = layout;
}

QString LayoutProviderPrivate::pathForLayout(const FyLayout& layout) const
{
    if(!layout.isValid()) {
        return {};
    }

    if(const auto it = m_layoutPaths.find(layout.name()); it != m_layoutPaths.cend()) {
        return it->second;
    }
    return {};
}

bool LayoutProviderPrivate::isBuiltIn(const QString& name) const
{
    return m_builtInLayouts.contains(name);
}

void LayoutProviderPrivate::setLayoutPath(const FyLayout& layout, const QString& path)
{
    if(layout.isValid() && !path.isEmpty()) {
        m_layoutPaths[layout.name()] = path;
    }
}

void LayoutProviderPrivate::saveActiveLayoutName() const
{
    if(!m_currentLayout.isValid()) {
        return;
    }

    FyStateSettings settings;
    settings.setValue(ActiveLayoutState, m_currentLayout.name());
}

LayoutProvider::LayoutProvider(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<LayoutProviderPrivate>()}
{
    p->loadLegacyCurrentLayout();
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

FyLayout LayoutProvider::layoutByName(const QString& name) const
{
    auto layout = p->layout(name);
    if(layout != p->m_layouts.cend()) {
        return *layout;
    }
    return {};
}

QString LayoutProvider::uniqueLayoutName(const QString& name) const
{
    return Utils::findUniqueString(name, p->m_layouts, [](const FyLayout& layout) { return layout.name(); });
}

bool LayoutProvider::canDeleteLayout(const QString& name) const
{
    const FyLayout layout = layoutByName(name);
    return layout.isValid() && !p->pathForLayout(layout).isEmpty();
}

bool LayoutProvider::canResetLayout(const QString& name) const
{
    return p->isBuiltIn(name) && !p->pathForLayout(layoutByName(name)).isEmpty();
}

void LayoutProvider::findLayouts() const
{
    QStringList files;
    QList<QDir> stack{Gui::layoutsPath()};

    while(!stack.isEmpty()) {
        const QDir dir     = stack.takeFirst();
        const auto subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for(const auto& subDir : subDirs) {
            stack.append(QDir{subDir.absoluteFilePath()});
        }

        const auto dirFiles = dir.entryInfoList({u"*.fyl"_s}, QDir::Files);
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
            p->addLayout(FyLayout{json}, false, file);
        }
    }

    p->resolveActiveLayout();
}

void LayoutProvider::saveCurrentLayout()
{
    if(!p->m_currentLayout.isValid()) {
        return;
    }

    QString filepath = p->pathForLayout(p->m_currentLayout);
    if(filepath.isEmpty()) {
        filepath = layoutFilePath(p->m_currentLayout.name());
        p->setLayoutPath(p->m_currentLayout, filepath);
    }

    if(writeLayout(p->m_currentLayout, filepath)) {
        p->saveActiveLayoutName();
        Q_EMIT layoutChanged(p->m_currentLayout);
        Q_EMIT currentLayoutChanged(p->m_currentLayout);
    }
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
    p->updateLayout(layout);
    p->m_currentLayout = layout;
    p->saveActiveLayoutName();
    Q_EMIT currentLayoutChanged(layout);
}

bool LayoutProvider::createLayout(const QString& name, const FyLayout& baseLayout)
{
    if(name.isEmpty() || p->layout(name) != p->m_layouts.end()) {
        return false;
    }

    const FyLayout layout = renamedLayout(baseLayout, name);
    const QString path    = layoutFilePath(name);
    if(!writeLayout(layout, path)) {
        return false;
    }

    p->m_layouts.push_back(layout);
    p->setLayoutPath(layout, path);
    Q_EMIT layoutAdded(layout);
    return true;
}

bool LayoutProvider::deleteLayout(const QString& name)
{
    auto layout = p->layout(name);
    if(layout == p->m_layouts.end()) {
        return false;
    }

    const QString path = p->pathForLayout(*layout);
    if(path.isEmpty()) {
        return false;
    }

    QFile file{path};
    if(file.exists() && !file.moveToTrash()) {
        qCWarning(LAYOUT_PROV) << "Couldn't delete layout file";
        return false;
    }

    const bool wasCurrent = p->m_currentLayout.name() == name;
    p->m_layoutPaths.erase(name);
    p->m_layouts.erase(layout);

    if(wasCurrent) {
        if(const auto defaultLayout = p->layout(u"Default"_s); defaultLayout != p->m_layouts.end()) {
            p->m_currentLayout = *defaultLayout;
        }
        else if(!p->m_layouts.empty()) {
            p->m_currentLayout = p->m_layouts.front();
        }
        else {
            p->m_currentLayout = {};
        }
        p->saveActiveLayoutName();
        Q_EMIT currentLayoutChanged(p->m_currentLayout);
    }

    Q_EMIT layoutRemoved(name);
    return true;
}

bool LayoutProvider::renameLayout(const QString& oldName, const QString& newName)
{
    if(oldName.isEmpty() || newName.isEmpty() || oldName == newName || p->layout(newName) != p->m_layouts.end()) {
        return false;
    }

    const auto layout = p->layout(oldName);
    if(layout == p->m_layouts.end()) {
        return false;
    }

    const FyLayout renamed = renamedLayout(*layout, newName);
    const QString oldPath  = p->pathForLayout(*layout);
    const QString newPath  = layoutFilePath(newName);
    if(!writeLayout(renamed, newPath)) {
        return false;
    }

    if(!oldPath.isEmpty() && oldPath != newPath) {
        QFile::moveToTrash(oldPath);
    }

    *layout = renamed;
    p->m_layoutPaths.erase(oldName);
    p->setLayoutPath(renamed, newPath);

    if(p->m_currentLayout.name() == oldName) {
        p->m_currentLayout = renamed;
        p->saveActiveLayoutName();
        Q_EMIT currentLayoutChanged(p->m_currentLayout);
    }

    Q_EMIT layoutRemoved(oldName);
    Q_EMIT layoutAdded(renamed);
    return true;
}

bool LayoutProvider::duplicateLayout(const QString& sourceName, const QString& newName)
{
    const FyLayout source = layoutByName(sourceName);
    return createLayout(newName, source);
}

bool LayoutProvider::resetLayout(const QString& name)
{
    if(!canResetLayout(name)) {
        return false;
    }

    const FyLayout current = layoutByName(name);
    const QString path     = p->pathForLayout(current);
    if(!path.isEmpty()) {
        QFile::moveToTrash(path);
    }

    const auto builtIn = p->m_builtInLayouts.find(name);
    if(builtIn == p->m_builtInLayouts.end()) {
        return false;
    }

    p->updateLayout(builtIn->second);
    p->m_layoutPaths.erase(name);

    if(p->m_currentLayout.name() == name) {
        p->m_currentLayout = builtIn->second;
        p->saveActiveLayoutName();
        Q_EMIT currentLayoutChanged(p->m_currentLayout);
    }

    Q_EMIT layoutChanged(builtIn->second);
    return true;
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

    FyLayout layout{json};
    const bool existing = p->layout(layout.name()) != p->m_layouts.end();

    QString layoutPath = fileInfo.absoluteFilePath();
    if(!Utils::File::isSamePath(fileInfo.absolutePath(), Gui::layoutsPath())) {
        layoutPath = Gui::layoutsPath() + fileInfo.fileName();
        file.copy(layoutPath);
    }

    layout = p->addLayout(layout, true, layoutPath);
    if(layout.isValid() && !existing) {
        Q_EMIT layoutAdded(layout);
    }

    return layout;
}

void LayoutProvider::importLayout(QWidget* parent)
{
    const QString layoutFile = QFileDialog::getOpenFileName(parent, tr("Open Layout"), {}, tr("fooyin Layout (*.fyl)"),
                                                            nullptr, QFileDialog::DontResolveSymlinks);

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
        Q_EMIT requestChangeLayout(layout);
    }
}

bool LayoutProvider::exportLayout(const FyLayout& layout, const QString& path)
{
    QString filepath{path};
    if(!filepath.contains(u".fyl"_s)) {
        filepath += u".fyl"_s;
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
    if(Utils::File::isSamePath(fileInfo.absolutePath(), Gui::layoutsPath())) {
        auto currLayout = p->layout(layout.name());
        if(currLayout != p->m_layouts.end()) {
            *currLayout = layout;
            p->setLayoutPath(layout, filepath);
            Q_EMIT layoutChanged(layout);
        }
        else {
            p->m_layouts.push_back(layout);
            p->setLayoutPath(layout, filepath);
            Q_EMIT layoutAdded(layout);
        }
    }

    return true;
}
} // namespace Fooyin
