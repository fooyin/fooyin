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

#pragma once

#include "fygui_export.h"

#include <QDialog>

#include <optional>

class QGridLayout;

namespace Fooyin {
/*!
 * Base dialog for widget configuration UIs.
 *
 * ConfigDialog provides a shared button layout for applying the current config to
 * the active widget instance, saving the current values as defaults for new
 * instances, and restoring the editor to either saved defaults or the original
 * factory defaults.
 *
 * Subclasses are responsible for building their controls in @fn contentLayout()
 * and implementing the four actions below.
 */
class FYGUI_EXPORT ConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigDialog(const QString& title, QWidget* parent = nullptr);
    ~ConfigDialog() override = default;

    /*!
     * Applies the dialog's current values to the target widget instance.
     */
    virtual void apply() = 0;
    /*!
     * Saves the dialog's current values as the defaults for newly created instances.
     */
    virtual void saveDefaults() = 0;
    /*!
     * Replaces the dialog's current values with the saved defaults for new instances.
     */
    virtual void restoreSavedDefaults() = 0;
    /*!
     * Replaces the dialog's current values with the application factory defaults.
     */
    virtual void restoreFactoryDefaults() = 0;
    /*!
     * Clears any user-saved defaults so future instances fall back to factory defaults.
     */
    virtual void clearSavedDefaults() = 0;

protected:
    /*!
     * Returns the layout reserved for subclass-owned editor controls.
     */
    [[nodiscard]] QGridLayout* contentLayout() const;

private:
    QGridLayout* m_contentLayout;
};

/*!
 * Convenience base for dialogs that edit a widget-specific config struct.
 *
 * WidgetType is expected to provide:
 * - `ConfigType defaultConfig() const`
 * - `ConfigType factoryConfig() const`
 * - `const ConfigType& currentConfig() const`
 * - `void applyConfig(const ConfigType&)`
 * - `void saveDefaults(const ConfigType&) const`
 *
 * Subclasses only need to translate between UI controls and the config value by
 * implementing @fn config() and @fn setConfig(). Call @fn loadCurrentConfig()
 * during construction to initialise the editor from the widget's current state.
 * Dialogs that observe external config changes should connect the notification to
 * @fn syncCurrentConfig() and override @fn mergeExternalConfig() to preserve draft values.
 */
template <typename WidgetType, typename ConfigType>
class WidgetConfigDialog : public ConfigDialog
{
public:
    WidgetConfigDialog(WidgetType* widget, const QString& title, QWidget* parent = nullptr)
        : ConfigDialog{title, parent}
        , m_widget{widget}
    { }

    void apply() override
    {
        m_widget->applyConfig(config());
        loadCurrentConfig();
    }

    void saveDefaults() override
    {
        m_widget->saveDefaults(config());
    }

    void restoreSavedDefaults() override
    {
        setConfig(m_widget->defaultConfig());
    }

    void restoreFactoryDefaults() override
    {
        setConfig(m_widget->factoryConfig());
    }

    void clearSavedDefaults() override
    {
        m_widget->clearSavedDefaults();
        setConfig(m_widget->factoryConfig());
    }

protected:
    /*!
     * Loads the widget's current instance config into the editor UI.
     */
    void loadCurrentConfig()
    {
        const ConfigType currentConfig = m_widget->currentConfig();
        setConfig(currentConfig);
        m_syncedConfig = currentConfig;
    }

    /*!
     * Merges changes made outside the dialog into the editor state.
     */
    void syncCurrentConfig()
    {
        const ConfigType currentConfig = m_widget->currentConfig();
        if(!m_syncedConfig) {
            setConfig(currentConfig);
        }
        else {
            mergeExternalConfig(*m_syncedConfig, currentConfig);
        }

        m_syncedConfig = currentConfig;
    }

    /*!
     * Copies an externally changed value into the dialog's draft.
     */
    template <typename Value>
    static void mergeExternalValue(const Value& previous, const Value& current, Value& draft)
    {
        if(previous != current) {
            draft = current;
        }
    }

    /*!
     * Copies externally changed member values from @p current into @p draft.
     *
     * @p members must be pointers to members of @c Object. Draft values are
     * preserved for members that have not changed since @p previous.
     */
    template <typename Object, typename... Members>
    static void mergeExternalFieldValues(const Object& previous, const Object& current, Object& draft,
                                         Members... members)
    {
        (mergeExternalValue(previous.*members, current.*members, draft.*members), ...);
    }

    /*!
     * Merges externally changed ConfigType members into the dialog's draft.
     *
     * @p members must be pointers to members of ConfigType. The merged draft is
     * written back to the editor with setConfig().
     */
    template <typename... Members>
    void mergeExternalFields(const ConfigType& previous, const ConfigType& current, Members... members)
    {
        auto draft{config()};
        mergeExternalFieldValues(previous, current, draft, members...);
        setConfig(draft);
    }

    /*!
     * Returns the widget instance being configured.
     */
    [[nodiscard]] WidgetType* widget() const
    {
        return m_widget;
    }

    /*!
     * Builds a config value from the current editor state.
     */
    [[nodiscard]] virtual ConfigType config() const = 0;
    /*!
     * Updates the editor state from a config value.
     */
    virtual void setConfig(const ConfigType& config) = 0;
    /*!
     * Updates only values changed outside the dialog.
     * The default replaces the entire editor state.
     */
    virtual void mergeExternalConfig(const ConfigType& /*previous*/, const ConfigType& current)
    {
        setConfig(current);
    }

private:
    WidgetType* m_widget;
    std::optional<ConfigType> m_syncedConfig;
};
} // namespace Fooyin
