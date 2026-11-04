"""Formatting definitions for forwarded arguments and the Qt commands used by fooyin."""

command_definitions = {
    "create_fooyin_plugin_internal": {
        "front_positional_arguments": ["plugin_name"],
        "options": ["NO_PCH", "NO_INSTALL"],
        "one_value_keywords": ["JSON_IN"],
        "multi_value_keywords": ["DEPENDS", "SOURCES"],
        "keyword_preprocessors": {"SOURCES": "sort"},
    },
    "qt_add_executable": {
        "front_positional_arguments": ["target"],
        "options": ["WIN32", "MACOSX_BUNDLE", "EXCLUDE_FROM_ALL", "MANUAL_FINALIZATION"],
    },
    "qt_add_resources": {
        "front_positional_arguments": ["outfiles"],
        "multi_value_keywords": ["OPTIONS"],
    },
    "qt_add_lrelease": {
        "options": ["NO_TARGET_DEPENDENCY", "EXCLUDE_FROM_ALL", "MERGE_QT_TRANSLATIONS", "NO_GLOBAL_TARGET"],
        "one_value_keywords": ["LRELEASE_TARGET", "QM_OUTPUT_DIRECTORY", "QM_FILES_OUTPUT_VARIABLE"],
        "multi_value_keywords": ["TS_FILES", "OPTIONS", "QT_TRANSLATION_CATALOGS"],
    },
    "qt_add_dbus_interface": {
        "front_positional_arguments": ["sources", "interface", "basename"],
    },
    "qt_add_dbus_adaptor": {
        "front_positional_arguments": ["sources", "xml_file", "include"],
    },
    "qt_standard_project_setup": {},
}
