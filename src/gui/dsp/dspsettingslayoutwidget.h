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

#include "dspsettingscontroller.h"

#include <gui/fywidget.h>

class QCheckBox;
class QComboBox;
class QContextMenuEvent;
class QMenu;

namespace Fooyin {
class DspLayoutEditor;
class DspSettingsDialog;

[[nodiscard]] FyWidget* createDspSettingsLayoutWidget(DspSettingsController* controller, QString dspId,
                                                      QString displayName, QWidget* parent);

class DspLayoutWidgetBase : public FyWidget
{
public:
    DspLayoutWidgetBase(DspSettingsController* controller, QString dspId, QString displayName, QWidget* parent);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

    [[nodiscard]] DspSettingsController* controller() const;
    [[nodiscard]] QString dspId() const;
    [[nodiscard]] uint64_t currentInstanceId() const;

    void refreshInstances();
    void selectInstance(uint64_t instanceId);
    virtual void populateInstanceSelector(const std::vector<DspSettingsController::Target>& targets);
    virtual void updateSelectedInstance(uint64_t instanceId);
    virtual void updateEnabledState(bool enabled);
    virtual void saveEditorLayoutData(QJsonObject& layout);
    virtual void loadEditorLayoutData(const QJsonObject& layout);
    virtual void addEditorActions(QMenu* menu);
    virtual void connectEditor(const DspSettingsController::Target& target) = 0;
    virtual void disconnectEditor();
    virtual void loadEditorSettings(const QByteArray& settings) = 0;
    virtual void setEditorControlsEnabled(bool enabled)         = 0;

private:
    void addDspMenuActions(QMenu* menu);
    void addInstanceMenu(QMenu* menu);
    void addEnabledAction(QMenu* menu);
    void clearInstance();

    DspSettingsController* m_controller;
    QString m_dspId;
    QString m_displayName;
    uint64_t m_currentInstanceId;
};

class DspSettingsDialogLayoutWidget final : public DspLayoutWidgetBase
{
public:
    DspSettingsDialogLayoutWidget(DspSettingsController* controller, QString dspId, QString displayName,
                                  QWidget* parent);

protected:
    void populateInstanceSelector(const std::vector<DspSettingsController::Target>& targets) override;
    void updateSelectedInstance(uint64_t instanceId) override;
    void updateEnabledState(bool enabled) override;
    void connectEditor(const DspSettingsController::Target& target) override;
    void disconnectEditor() override;
    void loadEditorSettings(const QByteArray& settings) override;
    void setEditorControlsEnabled(bool enabled) override;

private:
    DspSettingsDialog* m_editor;
    QComboBox* m_instanceSelector;
    QCheckBox* m_enabledToggle;
    QMetaObject::Connection m_previewConnection;
    QMetaObject::Connection m_enabledConnection;
};

class DspCompactLayoutWidget final : public DspLayoutWidgetBase
{
public:
    DspCompactLayoutWidget(DspSettingsController* controller, QString dspId, QString displayName,
                           DspLayoutEditor* editor, QWidget* parent);

protected:
    void saveEditorLayoutData(QJsonObject& layout) override;
    void loadEditorLayoutData(const QJsonObject& layout) override;
    void addEditorActions(QMenu* menu) override;
    void connectEditor(const DspSettingsController::Target& target) override;
    void disconnectEditor() override;
    void loadEditorSettings(const QByteArray& settings) override;
    void setEditorControlsEnabled(bool enabled) override;

private:
    DspLayoutEditor* m_editor;
    QMetaObject::Connection m_previewConnection;
};
} // namespace Fooyin
