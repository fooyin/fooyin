/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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
        setConfig(m_widget->currentConfig());
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

private:
    WidgetType* m_widget;
};
} // namespace Fooyin
