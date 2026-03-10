#pragma once

#include "fycore_export.h"

#include <core/scripting/scripttypes.h>

namespace Fooyin {
class LibraryManager;

/*!
 * Reusable environment adapter for library-backed scripting surfaces.
 *
 * This helper exposes library lookups plus a small evaluation-policy surface. It is
 * intended for callers that need basic library variables without implementing their
 * own `ScriptEnvironment`.
 */
class FYCORE_EXPORT LibraryScriptEnvironment : public ScriptEnvironment,
                                               public ScriptLibraryEnvironment,
                                               public ScriptEvaluationEnvironment
{
public:
    explicit LibraryScriptEnvironment(const LibraryManager* libraryManager);

    void setEvaluationPolicy(TrackListContextPolicy policy, QString placeholder = {}, bool escapeRichText = false,
                             bool useVariousArtists = false);

    [[nodiscard]] const ScriptLibraryEnvironment* libraryEnvironment() const override;
    [[nodiscard]] const ScriptEvaluationEnvironment* evaluationEnvironment() const override;

    [[nodiscard]] QString libraryName(const Track& track) const override;
    [[nodiscard]] QString libraryPath(const Track& track) const override;
    [[nodiscard]] QString relativePath(const Track& track) const override;
    [[nodiscard]] TrackListContextPolicy trackListContextPolicy() const override;
    [[nodiscard]] QString trackListPlaceholder() const override;
    [[nodiscard]] bool escapeRichText() const override;
    [[nodiscard]] bool useVariousArtists() const override;

private:
    const LibraryManager* m_libraryManager;
    TrackListContextPolicy m_trackListContextPolicy;
    QString m_trackListPlaceholder;
    bool m_escapeRichText;
    bool m_useVariousArtists;
};
} // namespace Fooyin
