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

#include "quicktaggerpage.h"

#include "quicktagger.h"
#include "quicktaggerconstants.h"
#include "quicktaggermodel.h"

#include <gui/widgets/extendabletableview.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin::QuickTagger {
namespace {
class QuickTaggerPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit QuickTaggerPageWidget(SettingsManager* settings)
        : m_settings{settings}
        , m_table{new ExtendableTableView{ExtendableTableView::Move, this}}
        , m_model{new QuickTaggerModel(this)}
        , m_confirmation{new QSpinBox(this)}
    {
        m_table->setExtendableModel(m_model);
        m_table->setExtendableColumn(1);

        m_table->verticalHeader()->hide();
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
        m_table->setColumnWidth(0, 140);
        m_table->setColumnWidth(1, 160);
        m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_table->setToolTip(tr("Separate multiple values with semicolons."));

        m_confirmation->setRange(0, 99999);

        auto* thresholdLabel = new QLabel(tr("Confirmation threshold") + u":"_s, this);
        m_confirmation->setToolTip(tr("Ask for confirmation when changing more than this many tracks. Set to 0 to "
                                      "disable confirmation."));

        auto* layout = new QGridLayout(this);
        layout->addWidget(m_table, 0, 0, 1, 3);
        layout->addWidget(thresholdLabel, 1, 0);
        layout->addWidget(m_confirmation, 1, 1);
        layout->setColumnStretch(2, 1);
    }

    void load() override
    {
        m_model->setTags(quickTags(*m_settings));
        m_confirmation->setValue(quickTaggerConfirmationThreshold(*m_settings));
    }

    void apply() override
    {
        setQuickTags(*m_settings, m_model->tags());
        setQuickTaggerConfirmationThreshold(*m_settings, m_confirmation->value());
    }

    void reset() override
    {
        m_settings->reset<Settings::QuickTagger::Fields>();
        m_settings->reset<Settings::QuickTagger::ConfirmationThreshold>();
    }

    [[nodiscard]] QString validationError() const override
    {
        return m_model->validationError();
    }

private:
    SettingsManager* m_settings;
    ExtendableTableView* m_table;
    QuickTaggerModel* m_model;
    QSpinBox* m_confirmation;
};
} // namespace

QuickTaggerPage::QuickTaggerPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::QuickTagger);
    setName(QObject::tr("Quick Tagger"));
    setCategory({QObject::tr("Tagging"), QObject::tr("Quick Tagger")});
    setWidgetCreator([settings] { return new QuickTaggerPageWidget{settings}; });
}
} // namespace Fooyin::QuickTagger

#include "quicktaggerpage.moc"
