#pragma once

#include <QString>

#include <cstdint>
#include <vector>

namespace Fooyin {
enum class ScriptReferenceKind : uint8_t
{
    Variable = 0,
    Function,
};

struct ScriptReferenceEntry
{
    ScriptReferenceKind kind{ScriptReferenceKind::Variable};
    QString label;
    QString insertText;
    QString category;
    QString description;
    int cursorOffset{0};
};

const std::vector<ScriptReferenceEntry>& scriptReferenceEntries();
} // namespace Fooyin
