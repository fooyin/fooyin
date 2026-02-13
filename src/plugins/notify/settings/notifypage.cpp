/*
 * Fooyin
 * Copyright 2025, ripdog <https://github.com/ripdog>
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

#include "notifypage.h"

#include "notifysettings.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin::Notify {
class NotifyPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit NotifyPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QCheckBox* m_enable;
    QLineEdit* m_titleField;
    QLineEdit* m_bodyField;
    QCheckBox* m_showAlbumArt;
    QSpinBox* m_timeout;
};

NotifyPageWidget::NotifyPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_enable{new QCheckBox(tr("Enabled"), this)}
    , m_titleField{new QLineEdit(this)}
    , m_bodyField{new QLineEdit(this)}
    , m_showAlbumArt{new QCheckBox(tr("Show album art"), this)}
    , m_timeout{new QSpinBox(this)}
{
    auto* fieldsGroup  = new QGroupBox(tr("Notification Content"), this);
    auto* fieldsLayout = new QGridLayout(fieldsGroup);

    m_titleField->setToolTip(tr("Notification title using fooyin script (e.g. %title%)"));
    m_bodyField->setToolTip(tr("Notification body using fooyin script (e.g. %artist%[ - %album%])"));

    int row{0};
    fieldsLayout->addWidget(new QLabel(tr("Title") + ":"_L1, this), row, 0);
    fieldsLayout->addWidget(m_titleField, row++, 1);
    fieldsLayout->addWidget(new QLabel(tr("Body") + ":"_L1, this), row, 0);
    fieldsLayout->addWidget(m_bodyField, row++, 1);

    m_timeout->setRange(-1, 60000);
    m_timeout->setSuffix(u" ms"_s);
    m_timeout->setSpecialValueText(tr("System default"));
    m_timeout->setSingleStep(500);
    m_timeout->setToolTip(tr("Notification display time in milliseconds (-1 = system default)"));

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(m_enable, row++, 0, 1, 2);
    layout->addWidget(m_showAlbumArt, row++, 0, 1, 2);
    layout->addWidget(new QLabel(tr("Timeout") + ":"_L1, this), row, 0);
    layout->addWidget(m_timeout, row++, 1);
    layout->addWidget(fieldsGroup, row++, 0, 1, 2);
    layout->setRowStretch(row, 1);
}

void NotifyPageWidget::load()
{
    m_enable->setChecked(m_settings->value<Settings::Notify::Enabled>());
    m_titleField->setText(m_settings->value<Settings::Notify::TitleField>());
    m_bodyField->setText(m_settings->value<Settings::Notify::BodyField>());
    m_showAlbumArt->setChecked(m_settings->value<Settings::Notify::ShowAlbumArt>());
    m_timeout->setValue(m_settings->value<Settings::Notify::Timeout>());
}

void NotifyPageWidget::apply()
{
    m_settings->set<Settings::Notify::Enabled>(m_enable->isChecked());
    m_settings->set<Settings::Notify::TitleField>(m_titleField->text());
    m_settings->set<Settings::Notify::BodyField>(m_bodyField->text());
    m_settings->set<Settings::Notify::ShowAlbumArt>(m_showAlbumArt->isChecked());
    m_settings->set<Settings::Notify::Timeout>(m_timeout->value());
}

void NotifyPageWidget::reset()
{
    m_settings->reset<Settings::Notify::Enabled>();
    m_settings->reset<Settings::Notify::TitleField>();
    m_settings->reset<Settings::Notify::BodyField>();
    m_settings->reset<Settings::Notify::ShowAlbumArt>();
    m_settings->reset<Settings::Notify::Timeout>();
}

NotifyPage::NotifyPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId("Fooyin.Page.Notify");
    setName(tr("General"));
    setCategory({tr("Notifications")});
    setWidgetCreator([settings] { return new NotifyPageWidget(settings); });
}
} // namespace Fooyin::Notify

#include "moc_notifypage.cpp"
#include "notifypage.moc"
