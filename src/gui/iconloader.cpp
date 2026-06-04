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

#include <gui/guiconstants.h>
#include <gui/iconloader.h>

#include <QAction>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QObject>
#include <QPixmap>
#include <QPixmapCache>
#include <QSet>
#include <QStringList>

using namespace Qt::StringLiterals;

constexpr auto DefaultIconSize       = 20;
constexpr auto ThemeIconNameProperty = "_fy_themeIconName";

namespace {
struct IconThemeState
{
    QString primaryTheme;
    QString fallbackTheme;
};

IconThemeState& iconThemeState()
{
    static IconThemeState state;
    return state;
}

std::pair<QString, QString> currentThemes()
{
    auto& state = iconThemeState();
    return {state.primaryTheme, state.fallbackTheme};
}

QIcon iconFromResourceTheme(const QString& theme, const QString& iconName)
{
    if(theme.isEmpty()) {
        return {};
    }

    const QString iconPath = u":/icons/%1/scalable/actions/%2.svg"_s.arg(theme, iconName);
    if(!QFile::exists(iconPath)) {
        return {};
    }

    const QIcon icon{iconPath};
    return icon.isNull() ? QIcon{} : icon;
}

class IconLoader
{
public:
    IconLoader() = delete;

    static bool setThemeOverrides(const QString& primaryTheme, const QString& fallbackTheme)
    {
        auto& state = iconThemeState();

        if(state.primaryTheme == primaryTheme && state.fallbackTheme == fallbackTheme) {
            return false;
        }

        state.primaryTheme  = primaryTheme;
        state.fallbackTheme = fallbackTheme;
        return true;
    }

    [[nodiscard]] static QIcon icon(const QString& iconName)
    {
        const auto [primaryTheme, fallbackTheme] = currentThemes();

        if(const QIcon primaryIcon = iconFromResourceTheme(primaryTheme, iconName); !primaryIcon.isNull()) {
            return primaryIcon;
        }

        if(const QIcon systemIcon = QIcon::fromTheme(iconName); !systemIcon.isNull()) {
            return systemIcon;
        }

        return iconFromResourceTheme(fallbackTheme, iconName);
    }

    [[nodiscard]] static QPixmap pixmap(const QString& iconName, const QSize& size)
    {
        const auto [primaryTheme, fallbackTheme] = currentThemes();
        const QString key
            = u"ThemeIcon|%1|%2|%3|%4x%5"_s.arg(iconName, primaryTheme, fallbackTheme).arg(size.width(), size.height());

        QPixmap pixmap;
        if(QPixmapCache::find(key, &pixmap)) {
            return pixmap;
        }

        pixmap = icon(iconName).pixmap(size);
        QPixmapCache::insert(key, pixmap);
        return pixmap;
    }
};
} // namespace

namespace Fooyin::Gui {
bool setThemeIconOverrides(const QString& primaryTheme, const QString& fallbackTheme)
{
    return IconLoader::setThemeOverrides(primaryTheme, fallbackTheme);
}

QIcon iconFromTheme(const QString& icon)
{
    return IconLoader::icon(icon);
}

QIcon iconFromTheme(const char* icon)
{
    return iconFromTheme(QString::fromLatin1(icon));
}

QStringList availableThemeIcons()
{
    QSet<QString> icons;

    for(const QString& theme : {QString::fromLatin1(Fooyin::Constants::LightIconTheme),
                                QString::fromLatin1(Fooyin::Constants::DarkIconTheme)}) {
        const QDir iconDir{u":/icons/%1/scalable/actions"_s.arg(theme)};
        const QStringList entries = iconDir.entryList({u"*.svg"_s}, QDir::Files);

        for(const QString& entry : entries) {
            icons.insert(entry.chopped(4));
        }
    }

    QStringList sorted = icons.values();
    sorted.sort(Qt::CaseInsensitive);
    return sorted;
}

void setThemeIcon(QAction* action, const QString& icon)
{
    if(!action) {
        return;
    }

    action->setProperty(ThemeIconNameProperty, icon);
    action->setIcon(iconFromTheme(icon));
}

void setThemeIcon(QAction* action, const char* icon)
{
    setThemeIcon(action, QString::fromLatin1(icon));
}

QString themeIconName(const QAction* action)
{
    return action ? action->property(ThemeIconNameProperty).toString() : QString{};
}

bool refreshThemeIcon(QAction* action)
{
    if(!action) {
        return false;
    }

    const QString iconName = themeIconName(action);
    if(iconName.isEmpty()) {
        return false;
    }

    action->setIcon(iconFromTheme(iconName));
    return true;
}

void refreshThemeIcons(QObject* object)
{
    if(!object) {
        return;
    }

    if(auto* action = qobject_cast<QAction*>(object)) {
        refreshThemeIcon(action);
    }

    for(QAction* action : object->findChildren<QAction*>()) {
        refreshThemeIcon(action);
    }
}

QPixmap pixmapFromTheme(const char* icon)
{
    return pixmapFromTheme(icon, {DefaultIconSize, DefaultIconSize});
}

QPixmap pixmapFromTheme(const char* icon, const QSize& size)
{
    return IconLoader::pixmap(QString::fromLatin1(icon), size);
}
} // namespace Fooyin::Gui
