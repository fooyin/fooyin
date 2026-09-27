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

#include "core/plugins/plugininstaller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSysInfo>
#include <QTemporaryDir>

#include <archive.h>
#include <archive_entry.h>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
struct ArchiveWriteDeleter
{
    void operator()(archive* value) const
    {
        archive_write_close(value);
        archive_write_free(value);
    }
};

QString targetDirectory()
{
#ifdef Q_OS_WIN
    const QString os = u"windows"_s;
#elifdef Q_OS_MACOS
    const QString os = u"macos"_s;
#elifdef Q_OS_FREEBSD
    const QString os = u"freebsd"_s;
#else
    const QString os = u"linux"_s;
#endif
    const QString arch = QSysInfo::buildCpuArchitecture();
    return os + (arch == "arm64"_L1 || arch == "aarch64"_L1 ? u"-arm64"_s : u"-x64"_s);
}

bool createBundle(const QString& bundlePath, const QString& pluginPath, const QString& archivePluginPath)
{
    QFile plugin{pluginPath};
    if(!plugin.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray pluginData = plugin.readAll();

    const std::unique_ptr<archive, ArchiveWriteDeleter> writer{archive_write_new()};
    archive_write_set_format_zip(writer.get());
    if(archive_write_open_filename(writer.get(), QFile::encodeName(bundlePath).constData()) != ARCHIVE_OK) {
        return false;
    }

    archive_entry* entry = archive_entry_new();
    archive_entry_set_pathname_utf8(entry, archivePluginPath.toUtf8().constData());
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_entry_set_size(entry, pluginData.size());

    const bool success
        = archive_write_header(writer.get(), entry) == ARCHIVE_OK
       && archive_write_data(writer.get(), pluginData.constData(), pluginData.size()) == pluginData.size();
    archive_entry_free(entry);
    return success;
}
} // namespace

TEST(PluginInstallerTest, InstallsStandalonePluginIntoManagedDirectory)
{
    const QTemporaryDir plugins;
    ASSERT_TRUE(plugins.isValid());

    EXPECT_EQ(PluginInstaller::Result::Installed,
              PluginInstaller::install(QStringLiteral(TEST_PLUGIN_PATH), plugins.path(), false));
    EXPECT_EQ(PluginInstaller::Result::AlreadyInstalled,
              PluginInstaller::install(QStringLiteral(TEST_PLUGIN_PATH), plugins.path(), false));

    const QString installedPath = QDir{plugins.path()}.filePath(
        u"fooyin.bundletest/"_s + QFileInfo{QStringLiteral(TEST_PLUGIN_PATH)}.fileName());
    EXPECT_TRUE(QFileInfo{installedPath}.isFile());
}

TEST(PluginInstallerTest, InstallsPayloadForCurrentTarget)
{
    const QTemporaryDir temporaryDirectory;
    const QTemporaryDir plugins;
    ASSERT_TRUE(temporaryDirectory.isValid());
    ASSERT_TRUE(plugins.isValid());

    const QString bundlePath = temporaryDirectory.filePath(u"test.fyplugin"_s);
    const QString pluginName = QFileInfo{QStringLiteral(TEST_PLUGIN_PATH)}.fileName();
    ASSERT_TRUE(createBundle(bundlePath, QStringLiteral(TEST_PLUGIN_PATH), targetDirectory() + u'/' + pluginName));

    EXPECT_EQ(PluginInstaller::Result::Installed, PluginInstaller::install(bundlePath, plugins.path(), false));
    EXPECT_TRUE(QFileInfo{QDir{plugins.path()}.filePath(u"fooyin.bundletest/"_s + pluginName)}.isFile());
}

TEST(PluginInstallerTest, AppliesPendingUpdateWithoutKeepingOldVersion)
{
    const QTemporaryDir temporaryDirectory;
    const QTemporaryDir plugins;
    ASSERT_TRUE(temporaryDirectory.isValid());
    ASSERT_TRUE(plugins.isValid());

    const QString originalPlugin = QStringLiteral(TEST_PLUGIN_PATH);
    const QString updatedPlugin  = temporaryDirectory.filePath(QFileInfo{originalPlugin}.fileName());
    ASSERT_TRUE(QFile::copy(originalPlugin, updatedPlugin));
    QFile updatedFile{updatedPlugin};
    ASSERT_TRUE(updatedFile.open(QIODevice::Append));
    ASSERT_EQ(updatedFile.write("updated"), 7);
    updatedFile.close();

    EXPECT_EQ(PluginInstaller::Result::Installed, PluginInstaller::install(originalPlugin, plugins.path(), false));
    EXPECT_EQ(PluginInstaller::Result::AlreadyInstalled,
              PluginInstaller::install(updatedPlugin, plugins.path(), false));
    EXPECT_EQ(PluginInstaller::Result::Installed, PluginInstaller::install(updatedPlugin, plugins.path(), true));

    const QString pluginName    = QFileInfo{originalPlugin}.fileName();
    const QString installedPath = QDir{plugins.path()}.filePath(u"fooyin.bundletest/"_s + pluginName);
    const QString updatePath    = QDir{plugins.path()}.filePath(u".updates/fooyin.bundletest/"_s + pluginName);
    EXPECT_EQ(QFileInfo{installedPath}.size(), QFileInfo{originalPlugin}.size());
    EXPECT_EQ(QFileInfo{updatePath}.size(), QFileInfo{updatedPlugin}.size());

    EXPECT_TRUE(PluginInstaller::applyPendingUpdates(plugins.path()));
    EXPECT_EQ(QFileInfo{installedPath}.size(), QFileInfo{updatedPlugin}.size());
    EXPECT_FALSE(QFileInfo{QDir{plugins.path()}.filePath(u".updates"_s)}.exists());
    EXPECT_FALSE(QFileInfo{QDir{plugins.path()}.filePath(u"fooyin.bundletest/versions"_s)}.exists());
    EXPECT_FALSE(QFileInfo{QDir{plugins.path()}.filePath(u"fooyin.bundletest/current"_s)}.exists());
}

TEST(PluginInstallerTest, RejectsTraversalEntry)
{
    const QTemporaryDir temporaryDirectory;
    const QTemporaryDir plugins;
    ASSERT_TRUE(temporaryDirectory.isValid());
    ASSERT_TRUE(plugins.isValid());

    const QString bundlePath = temporaryDirectory.filePath(u"traversal.fyplugin"_s);
    ASSERT_TRUE(createBundle(bundlePath, QStringLiteral(TEST_PLUGIN_PATH),
                             targetDirectory() + u"/../"_s + QFileInfo{QStringLiteral(TEST_PLUGIN_PATH)}.fileName()));

    EXPECT_EQ(PluginInstaller::Result::Failed, PluginInstaller::install(bundlePath, plugins.path(), false));
    EXPECT_FALSE(QFileInfo{QDir{plugins.path()}.filePath(u"fooyin.bundletest"_s)}.exists());
}

TEST(PluginInstallerTest, RejectsBundleWithoutCurrentTarget)
{
    const QTemporaryDir temporaryDirectory;
    const QTemporaryDir plugins;
    ASSERT_TRUE(temporaryDirectory.isValid());
    ASSERT_TRUE(plugins.isValid());

    const QString otherTarget = targetDirectory().startsWith("windows"_L1) ? u"linux-x64"_s : u"windows-x64"_s;
    const QString bundlePath  = temporaryDirectory.filePath(u"wrong-target.fyplugin"_s);
    ASSERT_TRUE(createBundle(bundlePath, QStringLiteral(TEST_PLUGIN_PATH),
                             otherTarget + u'/' + QFileInfo{QStringLiteral(TEST_PLUGIN_PATH)}.fileName()));

    EXPECT_EQ(PluginInstaller::Result::Failed, PluginInstaller::install(bundlePath, plugins.path(), false));
}
} // namespace Fooyin::Testing
