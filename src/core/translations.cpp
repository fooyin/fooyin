/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "translations.h"

#include "corepaths.h"

#include <core/coresettings.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLoggingCategory>
#include <QTranslator>

Q_LOGGING_CATEGORY(TRANSLATIONS, "fy.translations")

namespace Fooyin {
Translations::Translations(SettingsManager* settings)
    : QObject{settings}
    , m_settings{settings}
{
    initialiseTranslations();
}

void Translations::initialiseTranslations()
{
    const QString customLanguage = m_settings->value<Settings::Core::Language>();
    QLocale locale;

    if(!customLanguage.isEmpty()) {
        locale = QLocale{customLanguage};
        if(customLanguage.compare(u"C", Qt::CaseInsensitive) != 0 && locale.language() == QLocale::C) {
            qCWarning(TRANSLATIONS) << "Custom locale (" << customLanguage << ") not found, using 'C' locale";
        }
        QLocale::setDefault(locale);
    }
    else {
        locale = QLocale{QLocale::system().name()};
    }

    if(locale.language() == QLocale::C) {
        qCDebug(TRANSLATIONS) << "Skipping loading of translations for C locale" << locale.name();
        return;
    }

    installTranslations(locale, QStringLiteral("qt"), QLibraryInfo::path(QLibraryInfo::TranslationsPath), false);

    const QString translationsPath = Core::translationsPath();
    if(translationsPath.isEmpty()) {
        return;
    }

    installTranslations(locale, QStringLiteral("fooyin"), translationsPath, true);
}

bool Translations::installTranslations(const QLocale& locale, const QString& translation, const QString& path,
                                       bool warn)
{
    auto* translator = new QTranslator(this);
    if(!translator->load(locale, translation, QStringLiteral("_"), path)) {
        if(warn) {
            qCWarning(TRANSLATIONS) << "Failed to load" << translation << "translations for locale" << locale.name()
                                    << "from" << path;
        }
        delete translator;
        return false;
    }

    qCDebug(TRANSLATIONS) << "Loaded" << translation << "translations for locale" << locale.name() << "from" << path;

    return QCoreApplication::installTranslator(translator);
}

} // namespace Fooyin
