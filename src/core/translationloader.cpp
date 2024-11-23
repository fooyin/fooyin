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

#include "translationloader.h"

#include "corepaths.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLoggingCategory>
#include <QTranslator>

Q_LOGGING_CATEGORY(TRANSLATIONS, "fy.translations")

using namespace Qt::StringLiterals;

namespace Fooyin {
TranslationLoader::TranslationLoader(QObject* parent)
    : QObject{parent}
{ }

void TranslationLoader::initialiseTranslations(const QString& customLanguage)
{
    QLocale locale;

    if(!customLanguage.isEmpty()) {
        locale = QLocale{customLanguage};
        if(customLanguage.compare("C"_L1, Qt::CaseInsensitive) != 0 && locale.language() == QLocale::C) {
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

    installTranslations(locale, u"qt"_s, QLibraryInfo::path(QLibraryInfo::TranslationsPath), false);

    const QString translationsPath = Core::translationsPath();
    if(translationsPath.isEmpty()) {
        return;
    }

    installTranslations(locale, u"fooyin"_s, translationsPath, true);
}

bool TranslationLoader::installTranslations(const QLocale& locale, const QString& translation, const QString& path,
                                            bool warn)
{
    auto* translator = new QTranslator(this);
    if(!translator->load(locale, translation, u"_"_s, path)) {
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
