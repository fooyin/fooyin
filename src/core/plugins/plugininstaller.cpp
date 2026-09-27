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

#include "plugininstaller.h"

#include "plugininfo.h"

#include <core/plugins/plugin.h>
#include <utils/fileutils.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QLoggingCategory>
#include <QPluginLoader>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTemporaryDir>

#include <archive.h>
#include <archive_entry.h>

#include <array>
#include <optional>
#include <unordered_set>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(PLUGIN_INSTALLER, "fy.plugininstaller")

constexpr qint64 MaxEntrySize     = 256LL * 1024 * 1024;
constexpr qint64 MaxExtractedSize = 512LL * 1024 * 1024;
constexpr auto MaxEntryCount      = 4096;

namespace Fooyin::PluginInstaller {
namespace {

struct ArchiveDeleter
{
    void operator()(archive* value) const
    {
        archive_read_close(value);
        archive_read_free(value);
    }
};
using ArchivePtr = std::unique_ptr<archive, ArchiveDeleter>;

struct InstalledPlugin
{
    QString identifier;
};

QString targetDirectory()
{
    QString osName;
#ifdef Q_OS_WIN
    osName = "windows"_L1;
#elifdef Q_OS_LINUX
    osName = "linux"_L1;
#elifdef Q_OS_MACOS
    osName = "macos"_L1;
#elifdef Q_OS_FREEBSD
    osName = "freebsd"_L1;
#else
    return {};
#endif

    const QString arch = QSysInfo::buildCpuArchitecture();
    if(arch == "x86_64"_L1 || arch == "amd64"_L1) {
        return osName + "-x64"_L1;
    }
    if(arch == "arm64"_L1 || arch == "aarch64"_L1) {
        return osName + "-arm64"_L1;
    }
    return {};
}

bool isPlatformDirectory(const QString& name)
{
    static const std::array directories{"windows-x64"_L1, "windows-arm64"_L1, "linux-x64"_L1,   "linux-arm64"_L1,
                                        "macos-x64"_L1,   "macos-arm64"_L1,   "freebsd-x64"_L1, "freebsd-arm64"_L1};
    return std::ranges::find(directories, name) != directories.cend();
}

bool isValidPathPart(const QString& part)
{
    if(part.isEmpty() || part == u"." || part == u".." || part.endsWith(u'.') || part.endsWith(u' ')
       || std::ranges::any_of(
           part, [](QChar character) { return character.unicode() < 0x20 || u"<>:\"|?*"_s.contains(character); })) {
        return false;
    }

    const QString baseName = part.section(u'.', 0, 0).toUpper();
    static const std::array reservedNames{"CON"_L1,  "PRN"_L1,  "AUX"_L1,  "NUL"_L1,  "COM1"_L1, "COM2"_L1,
                                          "COM3"_L1, "COM4"_L1, "COM5"_L1, "COM6"_L1, "COM7"_L1, "COM8"_L1,
                                          "COM9"_L1, "LPT1"_L1, "LPT2"_L1, "LPT3"_L1, "LPT4"_L1, "LPT5"_L1,
                                          "LPT6"_L1, "LPT7"_L1, "LPT8"_L1, "LPT9"_L1};
    return std::ranges::find(reservedNames, baseName) == reservedNames.cend();
}

std::optional<QString> outputPath(const QString& entryPath, bool isDirectory, const QString& target)
{
    QString path = QDir::fromNativeSeparators(entryPath);
    path.replace(u'\\', u'/');
    while(isDirectory && path.endsWith(u'/')) {
        path.chop(1);
    }

    static const QRegularExpression pathRegex{u"^[A-Za-z]:"_s};

    if(path.isEmpty() || path.startsWith(u'/') || path.startsWith("//"_L1) || pathRegex.match(path).hasMatch()) {
        return {};
    }

    const QStringList parts = path.split(u'/', Qt::KeepEmptyParts);
    if(std::ranges::any_of(parts, [](const QString& part) { return !isValidPathPart(part); })) {
        return {};
    }

    if(parts.front() == target) {
        if(parts.size() == 1) {
            return isDirectory ? std::optional{QString{}} : std::nullopt;
        }
        return QStringList{parts.cbegin() + 1, parts.cend()}.join(u'/');
    }

    if(isPlatformDirectory(parts.front())) {
        return QString{};
    }

    if(parts.front() == "licenses"_L1 || parts.size() == 1) {
        return path;
    }

    return {};
}

bool extractBundle(const QString& filepath, const QString& destination)
{
    const QString target = targetDirectory();
    if(target.isEmpty()) {
        qCWarning(PLUGIN_INSTALLER) << "Unsupported plugin bundle target" << QSysInfo::buildCpuArchitecture();
        return false;
    }

    const ArchivePtr reader{archive_read_new()};
    archive_read_support_filter_none(reader.get());
    archive_read_support_format_zip(reader.get());

    if(archive_read_open_filename(reader.get(), QFile::encodeName(filepath).constData(), 10240) != ARCHIVE_OK) {
        qCWarning(PLUGIN_INSTALLER) << "Could not open plugin bundle:" << archive_error_string(reader.get());
        return false;
    }

    std::unordered_set<QString> extractedPaths;
    archive_entry* entry{nullptr};
    qint64 extractedSize{0};
    int entryCount{0};
    bool foundTarget{false};

    int archiveResult{ARCHIVE_OK};
    while((archiveResult = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
        if(++entryCount > MaxEntryCount || archive_entry_is_encrypted(entry) == 1) {
            return false;
        }

        const auto fileType = archive_entry_filetype(entry);
        const bool isDir    = fileType == AE_IFDIR;
        if(!isDir && fileType != AE_IFREG) {
            return false;
        }

        const char* utf8Path = archive_entry_pathname_utf8(entry);
        const char* rawPath  = utf8Path ? utf8Path : archive_entry_pathname(entry);
        if(!rawPath) {
            return false;
        }

        const QString entryPath = utf8Path ? QString::fromUtf8(rawPath) : QFile::decodeName(rawPath);
        const auto relativePath = outputPath(entryPath, isDir, target);
        if(!relativePath) {
            return false;
        }

        if(relativePath->isEmpty()) {
            archive_read_data_skip(reader.get());
            continue;
        }

        foundTarget |= QDir::fromNativeSeparators(entryPath).startsWith(target + u'/');
        const QString pathKey = relativePath->toCaseFolded();
        if(!extractedPaths.emplace(pathKey).second) {
            return false;
        }

        const QString output = QDir{destination}.filePath(*relativePath);
        if(isDir) {
            if(!QDir{}.mkpath(output)) {
                return false;
            }
            continue;
        }

        const la_int64_t declaredSize = archive_entry_size_is_set(entry) ? archive_entry_size(entry) : 0;
        if(declaredSize < 0 || declaredSize > MaxEntrySize || extractedSize + declaredSize > MaxExtractedSize
           || !QDir{}.mkpath(QFileInfo{output}.absolutePath())) {
            return false;
        }

        QFile outputFile{output};
        if(!outputFile.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            return false;
        }

        std::array<char, 64UL * 1024> buffer{};
        qint64 entrySize{0};
        la_ssize_t bytesRead{0};
        while((bytesRead = archive_read_data(reader.get(), buffer.data(), buffer.size())) > 0) {
            entrySize += bytesRead;
            extractedSize += bytesRead;
            if(entrySize > MaxEntrySize || extractedSize > MaxExtractedSize
               || outputFile.write(buffer.data(), bytesRead) != bytesRead) {
                return false;
            }
        }
        if(bytesRead < 0) {
            return false;
        }
    }

    return archiveResult == ARCHIVE_EOF && foundTarget;
}

std::optional<InstalledPlugin> inspectPlugin(const QString& directory)
{
    std::optional<InstalledPlugin> installedPlugin;

    const auto files = Utils::File::getFilesInDirRecursive(QDir{directory});
    for(const QString& filepath : files) {
        if(!QLibrary::isLibrary(filepath)) {
            continue;
        }

        const QPluginLoader loader{filepath};
        const QJsonObject metadata = loader.metaData();
        if(metadata.value("IID"_L1).toString() != QLatin1StringView{FOOYIN_PLUGIN_IID}
           || !metadata.contains("MetaData"_L1)) {
            continue;
        }

        static const QRegularExpression identifierRegex{u"^[a-z0-9][a-z0-9._-]*$"_s};

        const PluginInfo plugin{filepath, metadata};
        const QString identifier = plugin.identifier();
        if(installedPlugin || identifier.isEmpty() || plugin.version().isEmpty()
           || !identifierRegex.match(identifier).hasMatch()) {
            return {};
        }

        installedPlugin = InstalledPlugin{.identifier = identifier};
    }

    return installedPlugin;
}

bool moveStagingDirectory(const QString& stagingPath, const QString& destination, QTemporaryDir& stagingDir)
{
    if(!QDir{}.rename(stagingPath, destination)) {
        return false;
    }
    stagingDir.setAutoRemove(false);
    return true;
}
} // namespace

Result install(const QString& filepath, const QString& pluginsPath, bool overwrite)
{
    const QFileInfo source{filepath};
    if(!source.isFile() || !QDir{}.mkpath(pluginsPath)) {
        return Result::Failed;
    }

    QTemporaryDir stagingDirectory{QDir{pluginsPath}.filePath(u".staging-XXXXXX"_s)};
    if(!stagingDirectory.isValid()) {
        return Result::Failed;
    }

    const QString suffix = source.suffix().toLower();
    if(suffix == "fyplugin"_L1 || suffix == "zip"_L1) {
        if(!extractBundle(filepath, stagingDirectory.path())) {
            return Result::Failed;
        }
    }
    else {
        if(!QLibrary::isLibrary(filepath)
           || !QFile::copy(filepath, QDir{stagingDirectory.path()}.filePath(source.fileName()))) {
            return Result::Failed;
        }
    }

    const auto plugin = inspectPlugin(stagingDirectory.path());
    if(!plugin) {
        return Result::Failed;
    }

    const QString pluginPath = QDir{pluginsPath}.filePath(plugin->identifier);
    if(QFileInfo::exists(pluginPath) && !overwrite) {
        return Result::AlreadyInstalled;
    }

    if(!QFileInfo::exists(pluginPath)) {
        return moveStagingDirectory(stagingDirectory.path(), pluginPath, stagingDirectory) ? Result::Installed
                                                                                           : Result::Failed;
    }

    const QString updatesPath = QDir{pluginsPath}.filePath(u".updates"_s);
    const QString updatePath  = QDir{updatesPath}.filePath(plugin->identifier);
    if(!QDir{}.mkpath(updatesPath)) {
        return Result::Failed;
    }
    if(QFileInfo::exists(updatePath) && !QDir{updatePath}.removeRecursively()) {
        return Result::Failed;
    }

    return moveStagingDirectory(stagingDirectory.path(), updatePath, stagingDirectory) ? Result::Installed
                                                                                       : Result::Failed;
}

bool applyPendingUpdates(const QString& pluginsPath)
{
    const QDir updates{QDir{pluginsPath}.filePath(u".updates"_s)};
    if(!updates.exists()) {
        return true;
    }

    bool success{true};
    const QFileInfoList pendingUpdates = updates.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(const QFileInfo& pendingUpdate : pendingUpdates) {
        const QString targetPath = QDir{pluginsPath}.filePath(pendingUpdate.fileName());
        if((QFileInfo::exists(targetPath) && !QDir{targetPath}.removeRecursively())
           || !QDir{}.rename(pendingUpdate.absoluteFilePath(), targetPath)) {
            qCWarning(PLUGIN_INSTALLER) << "Could not apply plugin update" << pendingUpdate.fileName();
            success = false;
        }
    }

    if(updates.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        QDir{pluginsPath}.rmdir(u".updates"_s);
    }
    return success;
}
} // namespace Fooyin::PluginInstaller
