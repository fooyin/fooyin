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

#include <core/scripting/trackqueryfilter.h>

#include "scripting/scriptruntime.h"

#include <core/coresettings.h>
#include <core/scripting/scriptproviders.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stringutils.h>

#include <algorithm>

namespace Fooyin {
namespace {
QStringList searchWords(const QString& text)
{
    QStringList words;
    QString word;

    for(const QChar& ch : text) {
        if(ch.isLetterOrNumber()) {
            word.append(ch);
        }
        else if(!word.isEmpty()) {
            words.push_back(word);
            word.clear();
        }
    }

    if(!word.isEmpty()) {
        words.push_back(word);
    }

    return words;
}

bool matchesWordBeginnings(const QString& text, const QString& query)
{
    const QStringList queryWords = searchWords(query);
    if(queryWords.empty()) {
        return false;
    }

    const QStringList textWords = searchWords(text);
    for(const QString& queryWord : queryWords) {
        const auto match = std::ranges::find_if(textWords, [&queryWord](const QString& textWord) {
            return textWord.startsWith(queryWord, Qt::CaseInsensitive);
        });
        if(match == textWords.cend()) {
            return false;
        }
    }

    return true;
}
} // namespace

class TrackSearchFilterPrivate
{
public:
    TrackSearchFilterPrivate();
    explicit TrackSearchFilterPrivate(ScriptSearchOptions options);

    void addProvider(const ScriptVariableProvider& provider);
    void addProvider(const ScriptFunctionProvider& provider);

    [[nodiscard]] TrackList filter(const QString& search, const TrackList& tracks);
    [[nodiscard]] PlaylistTrackList filter(const QString& search, const PlaylistTrackList& tracks);

private:
    ScriptRuntime m_runtime;
    ScriptSearchOptions m_options;
};

TrackSearchFilterPrivate::TrackSearchFilterPrivate()
{
    const FySettings settings;
    m_options
        = {.script
           = settings.value(Settings::Core::SearchScriptKey, QString::fromLatin1(TrackQueryFilter::DefaultSearchScript))
                 .toString(),
           .mode = static_cast<ScriptSearchMode>(
               settings.value(Settings::Core::SearchModeKey, TrackQueryFilter::DefaultSearchMode).toInt())};
}

TrackSearchFilterPrivate::TrackSearchFilterPrivate(ScriptSearchOptions options)
    : m_options{std::move(options)}
{ }

void TrackSearchFilterPrivate::addProvider(const ScriptVariableProvider& provider)
{
    m_runtime.addProvider(provider);
}

void TrackSearchFilterPrivate::addProvider(const ScriptFunctionProvider& provider)
{
    m_runtime.addProvider(provider);
}

TrackList TrackSearchFilterPrivate::filter(const QString& search, const TrackList& tracks)
{
    return m_runtime.filterQuery(search, tracks, m_options);
}

PlaylistTrackList TrackSearchFilterPrivate::filter(const QString& search, const PlaylistTrackList& tracks)
{
    return m_runtime.filterQuery(search, tracks, m_options);
}

TrackQueryFilter::TrackQueryFilter()
    : p{std::make_unique<TrackSearchFilterPrivate>()}
{ }

TrackQueryFilter::TrackQueryFilter(ScriptSearchOptions options)
    : p{std::make_unique<TrackSearchFilterPrivate>(std::move(options))}
{ }

TrackQueryFilter::~TrackQueryFilter() = default;

void TrackQueryFilter::addProvider(const ScriptVariableProvider& provider)
{
    p->addProvider(provider);
}

void TrackQueryFilter::addProvider(const ScriptFunctionProvider& provider)
{
    p->addProvider(provider);
}

bool TrackQueryFilter::matchesSearch(const QString& text, const QString& search, ScriptSearchMode mode,
                                     bool phraseMatch)
{
    if(search.isEmpty()) {
        return true;
    }

    const QString foldedText = Utils::foldForSearch(text);
    if(phraseMatch) {
        return foldedText.contains(Utils::foldForSearch(search));
    }

    if(mode == ScriptSearchMode::MatchWordBeginnings) {
        return matchesWordBeginnings(text, search);
    }

    const QStringList terms = search.split(u' ', Qt::SkipEmptyParts);
    return std::ranges::all_of(
        terms, [&foldedText](const QString& term) { return foldedText.contains(Utils::foldForSearch(term)); });
}

TrackList TrackQueryFilter::filter(const QString& search, const TrackList& tracks)
{
    return p->filter(search, tracks);
}

PlaylistTrackList TrackQueryFilter::filter(const QString& search, const PlaylistTrackList& tracks)
{
    return p->filter(search, tracks);
}

} // namespace Fooyin
