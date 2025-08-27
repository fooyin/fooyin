/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include <utils/stringcollator.h>

#include <unicode/ucol.h>

#include <QLocale>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(COLLATOR, "fy.collator")

using namespace Qt::StringLiterals;

namespace Fooyin {
class StringCollatorPrivate
{
public:
    void init()
    {
        cleanup();

        if(m_locale.language() == QLocale::C) {
            return;
        }

        UErrorCode status     = U_ZERO_ERROR;
        const QByteArray name = m_locale.bcp47Name().replace('-'_L1, '_'_L1).toLatin1();

        m_collator = ucol_open(name.constData(), &status);

        if(U_FAILURE(status)) {
            qCWarning(COLLATOR) << "Could not create collator: " << status;
            m_dirty = false;
            return;
        }

        ucol_setAttribute(m_collator, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);

        status = U_ZERO_ERROR;
        ucol_setAttribute(m_collator, UCOL_STRENGTH,
                          (m_caseSensitivity == Qt::CaseSensitive) ? UCOL_DEFAULT_STRENGTH : UCOL_SECONDARY, &status);
        if(U_FAILURE(status)) {
            qCWarning(COLLATOR) << "Failed to set case sensitivity of collator: " << status;
        }

        status = U_ZERO_ERROR;
        ucol_setAttribute(m_collator, UCOL_NUMERIC_COLLATION, m_numericMode ? UCOL_ON : UCOL_OFF, &status);
        if(U_FAILURE(status)) {
            qCWarning(COLLATOR) << "Failed to set numeric collation of collator: " << status;
        }

        status = U_ZERO_ERROR;
        ucol_setAttribute(m_collator, UCOL_ALTERNATE_HANDLING, UCOL_NON_IGNORABLE, &status);
        if(U_FAILURE(status)) {
            qCWarning(COLLATOR) << "Failed to set numeric collation of collator: " << status;
        }

        m_dirty = false;
    }

    void cleanup()
    {
        if(m_collator) {
            ucol_close(m_collator);
        }
        m_collator = nullptr;
    }

    QLocale m_locale;
    Qt::CaseSensitivity m_caseSensitivity{Qt::CaseSensitive};
    bool m_numericMode{true};
    bool m_dirty{true};

    UCollator* m_collator{nullptr};
};

StringCollator::StringCollator()
    : p{std::make_unique<StringCollatorPrivate>()}
{
    p->init();
}

StringCollator::~StringCollator()
{
    p->cleanup();
}

Qt::CaseSensitivity StringCollator::caseSensitivity() const
{
    return p->m_caseSensitivity;
}

void StringCollator::setCaseSensitivity(Qt::CaseSensitivity sensitivity)
{
    if(std::exchange(p->m_caseSensitivity, sensitivity) != sensitivity) {
        p->m_dirty = true;
    }
}

bool StringCollator::numericMode() const
{
    return p->m_numericMode;
}

void StringCollator::setNumericMode(bool enabled)
{
    if(std::exchange(p->m_numericMode, enabled) != enabled) {
        p->m_dirty = true;
    }
}

int StringCollator::compare(QStringView s1, QStringView s2) const
{
    if(s1.isEmpty()) {
        return s2.isEmpty() ? 0 : -1;
    }

    if(s2.isEmpty()) {
        return 1;
    }

    if(p->m_dirty) {
        p->init();
    }

    if(!p->m_collator) {
        // Fallback: simple string comparison
        return s1.compare(s2, p->m_caseSensitivity);
    }

    return ucol_strcoll(p->m_collator, reinterpret_cast<const UChar*>(s1.data()), s1.size(),
                        reinterpret_cast<const UChar*>(s2.data()), s2.size());
}
} // namespace Fooyin
