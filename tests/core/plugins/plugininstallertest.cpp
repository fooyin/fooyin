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

#include <core/plugins/plugin.h>

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
    QString os;
#ifdef Q_OS_WIN
    os = "windows"_L1;
#elifdef Q_OS_LINUX
    os = "linux"_L1;
#elifdef Q_OS_MACOS
    os = "macos"_L1;
#elifdef Q_OS_FREEBSD
    os = "freebsd"_L1;
#else
    return {};
#endif
    const QString arch = QSysInfo::buildCpuArchitecture();
    if(arch == "x86_64"_L1 || arch == "amd64"_L1) {
        return os + "-x64"_L1;
    }
    if(arch == "arm64"_L1 || arch == "aarch64"_L1) {
        return os + "-arm64"_L1;
    }

    return {};
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
    const QTemporaryDir incompatibleSource;
    ASSERT_TRUE(plugins.isValid());
    ASSERT_TRUE(incompatibleSource.isValid());

    QFile originalPlugin{QStringLiteral(TEST_PLUGIN_PATH)};
    ASSERT_TRUE(originalPlugin.open(QIODevice::ReadOnly));
    QByteArray pluginData = originalPlugin.readAll();
    const QByteArray api{FOOYIN_PLUGIN_IID};
    ASSERT_TRUE(pluginData.contains(api));
    QByteArray incompatibleApi{api};
    incompatibleApi.back() = incompatibleApi.back() == '0' ? '1' : '0';
    pluginData.replace(api, incompatibleApi);

    const QString incompatiblePath = incompatibleSource.filePath(QFileInfo{originalPlugin}.fileName());
    QFile incompatiblePlugin{incompatiblePath};
    ASSERT_TRUE(incompatiblePlugin.open(QIODevice::WriteOnly));
    ASSERT_EQ(incompatiblePlugin.write(pluginData), pluginData.size());
    incompatiblePlugin.close();

    EXPECT_EQ(std::unexpected{PluginInstaller::InstallError::IncompatibleApi},
              PluginInstaller::install(incompatiblePath, plugins.path(), false));
    EXPECT_FALSE(QFileInfo{QDir{plugins.path()}.filePath(u"fooyin.bundletest"_s)}.exists());

    const auto installedPlugin = PluginInstaller::install(QStringLiteral(TEST_PLUGIN_PATH), plugins.path(), false);
    ASSERT_TRUE(installedPlugin.has_value());
    ASSERT_NE(installedPlugin->get(), nullptr);
    EXPECT_EQ((*installedPlugin)->identifier(), u"fooyin.bundletest"_s);
    EXPECT_EQ(std::unexpected{PluginInstaller::InstallError::SameVersionInstalled},
              PluginInstaller::install(QStringLiteral(TEST_PLUGIN_PATH), plugins.path(), false));

    const QString installedPath = QDir{plugins.path()}.filePath(
        u"fooyin.bundletest/"_s + QFileInfo{QStringLiteral(TEST_PLUGIN_PATH)}.fileName());
    EXPECT_EQ((*installedPlugin)->filepath(), installedPath);
    EXPECT_TRUE(QFileInfo{installedPath}.isFile());
}

TEST(PluginInstallerTest, InstallsPayloadForCurrentTarget)
{
    const QString target = targetDirectory();
    if(target.isEmpty()) {
        GTEST_SKIP() << "Plugin bundles are unsupported on this platform";
    }

    const QTemporaryDir temporaryDirectory;
    const QTemporaryDir plugins;
    ASSERT_TRUE(temporaryDirectory.isValid());
    ASSERT_TRUE(plugins.isValid());

    const QString bundlePath = temporaryDirectory.filePath(u"test.fyplugin"_s);
    const QString pluginName = QFileInfo{QStringLiteral(TEST_PLUGIN_PATH)}.fileName();
    ASSERT_TRUE(createBundle(bundlePath, QStringLiteral(TEST_PLUGIN_PATH), target + u'/' + pluginName));

    ASSERT_TRUE(QFile::copy(QStringLiteral(TEST_PLUGIN_PATH), plugins.filePath(pluginName)));
    EXPECT_EQ(std::unexpected{PluginInstaller::InstallError::SameVersionInstalled},
              PluginInstaller::install(bundlePath, plugins.path(), false));
    ASSERT_TRUE(QFile::remove(plugins.filePath(pluginName)));

    EXPECT_TRUE(PluginInstaller::install(bundlePath, plugins.path(), false).has_value());
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
    ASSERT_TRUE(updatedFile.open(QIODevice::ReadWrite));
    QByteArray updatedData = updatedFile.readAll();
    ASSERT_TRUE(updatedData.contains("1.2.3"));
    updatedData.replace("1.2.3", "1.2.4");
    ASSERT_TRUE(updatedFile.seek(0));
    ASSERT_EQ(updatedFile.write(updatedData), updatedData.size());
    ASSERT_EQ(updatedFile.write("updated"), 7);
    updatedFile.close();

    EXPECT_TRUE(PluginInstaller::install(originalPlugin, plugins.path(), false).has_value());
    EXPECT_EQ(std::unexpected{PluginInstaller::InstallError::AlreadyInstalled},
              PluginInstaller::install(updatedPlugin, plugins.path(), false));
    const auto pendingPlugin = PluginInstaller::install(updatedPlugin, plugins.path(), true);
    ASSERT_TRUE(pendingPlugin.has_value());
    ASSERT_NE(pendingPlugin->get(), nullptr);
    EXPECT_EQ((*pendingPlugin)->version(), u"1.2.4"_s);
    EXPECT_EQ(std::unexpected{PluginInstaller::InstallError::SameVersionInstalled},
              PluginInstaller::install(updatedPlugin, plugins.path(), false));
    EXPECT_EQ(std::unexpected{PluginInstaller::InstallError::AlreadyInstalled},
              PluginInstaller::install(originalPlugin, plugins.path(), false));

    const QString pluginName    = QFileInfo{originalPlugin}.fileName();
    const QString installedPath = QDir{plugins.path()}.filePath(u"fooyin.bundletest/"_s + pluginName);
    const QString updatePath    = QDir{plugins.path()}.filePath(u".updates/fooyin.bundletest/"_s + pluginName);
    EXPECT_EQ((*pendingPlugin)->filepath(), updatePath);
    EXPECT_EQ(QFileInfo{installedPath}.size(), QFileInfo{originalPlugin}.size());
    EXPECT_EQ(QFileInfo{updatePath}.size(), QFileInfo{updatedPlugin}.size());

    EXPECT_TRUE(PluginInstaller::applyPendingUpdates(plugins.path()));
    EXPECT_EQ(QFileInfo{installedPath}.size(), QFileInfo{updatedPlugin}.size());
    EXPECT_FALSE(QFileInfo{QDir{plugins.path()}.filePath(u".updates"_s)}.exists());
    EXPECT_FALSE(QFileInfo{QDir{plugins.path()}.filePath(u"fooyin.bundletest/versions"_s)}.exists());
    EXPECT_FALSE(QFileInfo{QDir{plugins.path()}.filePath(u"fooyin.bundletest/current"_s)}.exists());
    EXPECT_EQ(std::unexpected{PluginInstaller::InstallError::SameVersionInstalled},
              PluginInstaller::install(updatedPlugin, plugins.path(), false));
}

TEST(PluginInstallerTest, RemovesManagedPluginAndPendingUpdate)
{
    const QTemporaryDir plugins;
    ASSERT_TRUE(plugins.isValid());

    const QString originalPlugin = QStringLiteral(TEST_PLUGIN_PATH);
    const QString pluginName     = QFileInfo{originalPlugin}.fileName();
    const auto installedPlugin   = PluginInstaller::install(originalPlugin, plugins.path(), false);
    ASSERT_TRUE(installedPlugin.has_value());
    ASSERT_NE(installedPlugin->get(), nullptr);
    EXPECT_TRUE(PluginInstaller::canRemove(**installedPlugin, plugins.path()));
    const QString installedPath = (*installedPlugin)->filepath();

    const auto pendingPlugin = PluginInstaller::install(originalPlugin, plugins.path(), true);
    ASSERT_TRUE(pendingPlugin.has_value());
    ASSERT_NE(pendingPlugin->get(), nullptr);
    EXPECT_TRUE(PluginInstaller::canRemove(**pendingPlugin, plugins.path()));
    const QString updatePath = (*pendingPlugin)->filepath();
    QFile updateFile{updatePath};
    ASSERT_TRUE(updateFile.open(QIODevice::Append));
    ASSERT_EQ(updateFile.write("updated"), 7);
    updateFile.close();

    const PluginInfo externalPlugin{originalPlugin, QPluginLoader{originalPlugin}.metaData()};
    EXPECT_FALSE(PluginInstaller::canRemove(externalPlugin, plugins.path()));

    const QString loosePath = plugins.filePath(pluginName);
    ASSERT_TRUE(QFile::copy(originalPlugin, loosePath));
    const PluginInfo loosePlugin{loosePath, QPluginLoader{loosePath}.metaData()};
    EXPECT_FALSE(PluginInstaller::canRemove(loosePlugin, plugins.path()));

    ASSERT_TRUE(QDir{}.mkpath(plugins.filePath(u"unrelated"_s)));
    const QString unrelatedPath = plugins.filePath(u"unrelated/"_s + pluginName);
    ASSERT_TRUE(QFile::copy(originalPlugin, unrelatedPath));

    EXPECT_FALSE(PluginInstaller::remove(u"../outside"_s, plugins.path()).has_value());
    EXPECT_TRUE(QFileInfo{installedPath}.exists());
    EXPECT_TRUE(QFileInfo{updatePath}.exists());
    EXPECT_TRUE(QFileInfo{loosePath}.exists());

    const QString identifier = (*installedPlugin)->identifier();
    EXPECT_TRUE(PluginInstaller::applyPendingUpdates(plugins.path(), {identifier}));
    EXPECT_TRUE(QFileInfo{updatePath}.exists());
    EXPECT_EQ(QFileInfo{installedPath}.size(), QFileInfo{originalPlugin}.size());

    ASSERT_TRUE(PluginInstaller::remove(identifier, plugins.path()).has_value());
    EXPECT_FALSE(QFileInfo{plugins.filePath(identifier)}.exists());
    EXPECT_FALSE(QFileInfo{plugins.filePath(u".updates"_s)}.exists());
    EXPECT_FALSE(QFileInfo{loosePath}.exists());
    EXPECT_TRUE(QFileInfo{unrelatedPath}.exists());
    EXPECT_TRUE(QFileInfo{originalPlugin}.exists());
    EXPECT_TRUE(PluginInstaller::remove(identifier, plugins.path()).has_value());
}

TEST(PluginInstallerTest, RejectsTraversalEntry)
{
    const QString target = targetDirectory();
    if(target.isEmpty()) {
        GTEST_SKIP() << "Plugin bundles are unsupported on this platform";
    }

    const QTemporaryDir temporaryDirectory;
    const QTemporaryDir plugins;
    ASSERT_TRUE(temporaryDirectory.isValid());
    ASSERT_TRUE(plugins.isValid());

    const QString bundlePath = temporaryDirectory.filePath(u"traversal.fyplugin"_s);
    ASSERT_TRUE(createBundle(bundlePath, QStringLiteral(TEST_PLUGIN_PATH),
                             target + u"/../"_s + QFileInfo{QStringLiteral(TEST_PLUGIN_PATH)}.fileName()));

    EXPECT_EQ(std::unexpected{PluginInstaller::InstallError::InvalidBundle},
              PluginInstaller::install(bundlePath, plugins.path(), false));
    EXPECT_FALSE(QFileInfo{QDir{plugins.path()}.filePath(u"fooyin.bundletest"_s)}.exists());
}

TEST(PluginInstallerTest, RejectsBundleWithoutCurrentTarget)
{
    const QString target = targetDirectory();
    if(target.isEmpty()) {
        GTEST_SKIP() << "Plugin bundles are unsupported on this platform";
    }

    const QTemporaryDir temporaryDirectory;
    const QTemporaryDir plugins;
    ASSERT_TRUE(temporaryDirectory.isValid());
    ASSERT_TRUE(plugins.isValid());

    const QString otherTarget = target.startsWith("windows"_L1) ? u"linux-x64"_s : u"windows-x64"_s;
    const QString bundlePath  = temporaryDirectory.filePath(u"wrong-target.fyplugin"_s);
    ASSERT_TRUE(createBundle(bundlePath, QStringLiteral(TEST_PLUGIN_PATH),
                             otherTarget + u'/' + QFileInfo{QStringLiteral(TEST_PLUGIN_PATH)}.fileName()));

    EXPECT_EQ(std::unexpected{PluginInstaller::InstallError::MissingPlatform},
              PluginInstaller::install(bundlePath, plugins.path(), false));
}
} // namespace Fooyin::Testing
