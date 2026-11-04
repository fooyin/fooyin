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

#include "guibackgroundpage.h"

#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <gui/widgets/backgroundconfigwidget.h>
#include <gui/widgets/hintlabel.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
class GuiBackgroundPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit GuiBackgroundPageWidget(SettingsManager* settings)
        : m_settings{settings}
        , m_background{new BackgroundConfigWidget(this)}
        , m_useSharedBase{new QCheckBox(tr("Use a shared base colour"), this)}
    {
        m_background->setOptions(BackgroundConfigWidget::TrackPreference | BackgroundConfigWidget::FadeDuration
                                 | BackgroundConfigWidget::NoGlobalSource);

        auto* hint = new HintLabel(
            tr("Choose ‘Global background’ in a widget’s background settings to use this image."), this);
        hint->setWordWrap(true);

        m_useSharedBase->setToolTip(
            tr("Uses an opaque theme base colour beneath the artwork to keep colouring consistent across widgets.\n"
               "When disabled, the artwork blends with each widget’s own background."));

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(hint);
        layout->addWidget(m_background);
        layout->addWidget(m_useSharedBase);
        layout->addStretch();
    }

    void load() override
    {
        const auto values
            = QJsonObject::fromVariantMap(m_settings->value<Settings::Gui::Internal::GlobalBackground>().toMap());
        BackgroundConfig config;
        config.loadLayout(values);
        m_background->setConfig(config);
        m_useSharedBase->setChecked(values.value("UseSharedBase"_L1).toBool());
    }

    void apply() override
    {
        QJsonObject config;
        m_background->config().saveLayout(config);
        config["UseSharedBase"_L1] = m_useSharedBase->isChecked();
        m_settings->set<Settings::Gui::Internal::GlobalBackground>(config.toVariantMap());
    }

    void reset() override
    {
        m_settings->reset<Settings::Gui::Internal::GlobalBackground>();
    }

private:
    SettingsManager* m_settings;
    BackgroundConfigWidget* m_background;
    QCheckBox* m_useSharedBase;
};

GuiBackgroundPage::GuiBackgroundPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::InterfaceBackground);
    setName(tr("Background"));
    setCategory({tr("Interface")});
    setWidgetCreator([settings] { return new GuiBackgroundPageWidget(settings); });
}
} // namespace Fooyin

#include "guibackgroundpage.moc"
#include "moc_guibackgroundpage.cpp"
