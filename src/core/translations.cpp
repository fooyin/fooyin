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

#include <core/coresettings.h>
#include <utils/settings/settingsmanager.h>

#include <QLibraryInfo>
#include <QTranslator>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
Translations::Translations(SettingsManager* settings)
{
    const QString customLanguage = settings->value<Settings::Core::Language>();

    if(!customLanguage.isEmpty()) {
        const QLocale customLocale = QLocale{customLanguage};
        // Returns the 'C' locale if not valid
        if(customLanguage.compare(u"C"_s, Qt::CaseInsensitive) != 0 && customLocale.language() == QLocale::C) {
            qWarning() << "Custom locale (" << customLanguage << ") not found, using 'C' locale.";
        }
        QLocale::setDefault(customLocale);
    }

    QLocale locale;

    if(locale.language() == QLocale::C) {
        return;
    }

    if(locale.language() == QLocale::English
       && (locale.territory() == QLocale::UnitedKingdom || locale.territory() == QLocale::AnyCountry)) {
        return;
    }

    const bool foundQt
        = installTranslations(locale, u"qt"_s, QLibraryInfo::path(QLibraryInfo::TranslationsPath), false);

    const QString translationsPath = u"://translations"_s;

    if(!foundQt) {
        installTranslations(locale, u"qt"_s, translationsPath, true);
    }

    installTranslations(locale, u"fooyin"_s, translationsPath, true);
}

bool Translations::installTranslations(const QLocale& locale, const QString& translation, const QString& path,
                                       bool warn)
{
    auto* translator = new QTranslator(this);
    if(!translator->load(locale, translation, u"_"_s, path)) {
        if(warn) {
            qWarning() << "Failed to load" << translation << "translations for locale" << locale.name() << "from"
                       << path;
        }
        delete translator;
        return false;
    }

    qDebug() << "Loaded" << translation << "translations for locale" << locale.name() << "from" << path;

    qApp->installTranslator(translator);
    return true;
}

} // namespace Fooyin
