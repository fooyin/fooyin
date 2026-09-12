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

#include "runservicespage.h"

#include "runservices.h"
#include "runservicesconstants.h"

#include <gui/widgets/scriptlineedit.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QUuid>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::RunServices {
namespace {
QString generateId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString quotedScriptPath(const QString& filepath)
{
    const QString nativePath = QDir::toNativeSeparators(filepath);
    QString escapedPath;
    escapedPath.reserve(nativePath.size() + 4);

    for(const QChar character : nativePath) {
        if(character == u'\\' || character == u'"') {
            escapedPath += u'\\';
        }
        escapedPath += character;
    }

    return u"\\\"%1\\\""_s.arg(escapedPath);
}

class RunServicesPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit RunServicesPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

    [[nodiscard]] QString validationError() const override;

private:
    void populateList(int currentRow = 0);
    void setCurrentService(int row);
    void addService();
    void removeService();
    void moveService(int offset);
    void updateCurrentService();

    SettingsManager* m_settings;

    QListWidget* m_serviceList;
    QPushButton* m_add;
    QPushButton* m_remove;
    QPushButton* m_moveUp;
    QPushButton* m_moveDown;
    QGroupBox* m_properties;
    ScriptLineEdit* m_name;
    ScriptLineEdit* m_path;
    QPushButton* m_browse;
    QSpinBox* m_simultaneousRuns;
    std::vector<RunService> m_services;
};

RunServicesPageWidget::RunServicesPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_serviceList{new QListWidget(this)}
    , m_add{new QPushButton(tr("Add"), this)}
    , m_remove{new QPushButton(tr("Remove"), this)}
    , m_moveUp{new QPushButton(tr("Move up"), this)}
    , m_moveDown{new QPushButton(tr("Move down"), this)}
    , m_properties{new QGroupBox(tr("Service properties"), this)}
    , m_name{new ScriptLineEdit(this)}
    , m_path{new ScriptLineEdit(this)}
    , m_browse{new QPushButton(tr("Browse…"), this)}
    , m_simultaneousRuns{new QSpinBox(this)}
{
    m_simultaneousRuns->setRange(1, 99);
    m_simultaneousRuns->setToolTip(tr("Run the service for up to this many selected tracks"));
    m_name->setPlaceholderText(tr("Name shown in the Run menu"));
    m_path->setPlaceholderText(tr("Application and arguments"));

    auto* buttons = new QVBoxLayout();
    buttons->addWidget(m_add);
    buttons->addWidget(m_remove);
    buttons->addWidget(m_moveUp);
    buttons->addWidget(m_moveDown);
    buttons->addStretch();

    auto* propertiesLayout = new QGridLayout(m_properties);

    int row{0};
    propertiesLayout->addWidget(new QLabel(tr("Name") + u":"_s, this), row, 0);
    propertiesLayout->addWidget(m_name, row++, 1, 1, 3);
    propertiesLayout->addWidget(new QLabel(tr("Path") + u":"_s, this), row, 0);
    propertiesLayout->addWidget(m_path, row, 1, 1, 2);
    propertiesLayout->addWidget(m_browse, row++, 3);
    propertiesLayout->addWidget(new QLabel(tr("Simultaneous runs") + u":"_s, this), row, 0);
    propertiesLayout->addWidget(m_simultaneousRuns, row++, 1);
    propertiesLayout->setColumnStretch(2, 1);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(m_serviceList, row, 0);
    layout->addLayout(buttons, row++, 1);
    layout->addWidget(new QLabel(u"🛈 "_s + tr("Checked services are shown in the Run menu.")), row++, 0, 1, 2);
    layout->addWidget(m_properties, row++, 0, 1, 2);

    QObject::connect(m_serviceList, &QListWidget::currentRowChanged, this, &RunServicesPageWidget::setCurrentService);
    QObject::connect(m_serviceList, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        const int itemRow = m_serviceList->row(item);
        if(itemRow >= 0 && std::cmp_less(itemRow, m_services.size())) {
            m_services.at(itemRow).enabled = item->checkState() == Qt::Checked;
        }
    });
    QObject::connect(m_add, &QPushButton::clicked, this, &RunServicesPageWidget::addService);
    QObject::connect(m_remove, &QPushButton::clicked, this, &RunServicesPageWidget::removeService);
    QObject::connect(m_moveUp, &QPushButton::clicked, this, [this] { moveService(-1); });
    QObject::connect(m_moveDown, &QPushButton::clicked, this, [this] { moveService(1); });
    QObject::connect(m_browse, &QPushButton::clicked, this, [this] {
        const QString filepath = QFileDialog::getOpenFileName(this, tr("Select application"));
        if(!filepath.isEmpty()) {
            m_path->setText(quotedScriptPath(filepath));
        }
    });

    QObject::connect(m_name, &QLineEdit::textChanged, this, &RunServicesPageWidget::updateCurrentService);
    QObject::connect(m_path, &QLineEdit::textChanged, this, &RunServicesPageWidget::updateCurrentService);
    QObject::connect(m_simultaneousRuns, &QSpinBox::valueChanged, this, &RunServicesPageWidget::updateCurrentService);
}

void RunServicesPageWidget::load()
{
    m_services = runServices(*m_settings);
    populateList();
}

void RunServicesPageWidget::apply()
{
    setRunServices(*m_settings, m_services);
}

void RunServicesPageWidget::reset()
{
    m_settings->reset<Settings::RunServices::Services>();
    load();
}

QString RunServicesPageWidget::validationError() const
{
    for(size_t i{0}; i < m_services.size(); ++i) {
        const RunService& service = m_services.at(i);
        if(service.name.trimmed().isEmpty()) {
            return tr("Service %1: Name is required.").arg(i + 1);
        }
        if(service.path.trimmed().isEmpty()) {
            return tr("Service %1: Path is required.").arg(i + 1);
        }
    }
    return {};
}

void RunServicesPageWidget::populateList(int currentRow)
{
    const QSignalBlocker blocker{m_serviceList};

    m_serviceList->clear();

    for(const RunService& service : m_services) {
        auto* item = new QListWidgetItem(service.name, m_serviceList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(service.enabled ? Qt::Checked : Qt::Unchecked);
    }

    if(m_services.empty()) {
        m_serviceList->setCurrentRow(-1);
        setCurrentService(-1);
    }
    else {
        m_serviceList->setCurrentRow(std::clamp(currentRow, 0, static_cast<int>(m_services.size()) - 1));
        setCurrentService(m_serviceList->currentRow());
    }
}

void RunServicesPageWidget::setCurrentService(int row)
{
    const bool valid = row >= 0 && std::cmp_less(row, m_services.size());
    m_properties->setEnabled(valid);
    m_remove->setEnabled(valid);
    m_moveUp->setEnabled(valid && row > 0);
    m_moveDown->setEnabled(valid && std::cmp_less(row + 1, m_services.size()));

    const QSignalBlocker labelBlocker{m_name};
    const QSignalBlocker pathBlocker{m_path};
    const QSignalBlocker runsBlocker{m_simultaneousRuns};

    if(!valid) {
        m_name->clear();
        m_path->clear();
        m_simultaneousRuns->setValue(1);
        return;
    }

    const RunService& service = m_services.at(row);
    m_name->setText(service.name);
    m_path->setText(service.path);
    m_simultaneousRuns->setValue(service.simultaneousRuns);
}

void RunServicesPageWidget::addService()
{
    m_services.push_back(
        {.id   = generateId(),
         .name = Utils::findUniqueString(tr("New service"), m_services, [](const RunService& rs) { return rs.name; }),
         .path = {}});
    populateList(static_cast<int>(m_services.size()) - 1);
    m_name->setFocus();
    m_name->selectAll();
}

void RunServicesPageWidget::removeService()
{
    const int row = m_serviceList->currentRow();
    if(row < 0 || !std::cmp_less(row, m_services.size())) {
        return;
    }

    m_services.erase(m_services.begin() + row);
    populateList(std::min(row, static_cast<int>(m_services.size()) - 1));
}

void RunServicesPageWidget::moveService(int offset)
{
    const int row       = m_serviceList->currentRow();
    const int targetRow = row + offset;
    if(row < 0 || targetRow < 0 || !std::cmp_less(targetRow, m_services.size())) {
        return;
    }

    std::swap(m_services.at(row), m_services.at(targetRow));
    populateList(targetRow);
}

void RunServicesPageWidget::updateCurrentService()
{
    const int row = m_serviceList->currentRow();
    if(row < 0 || !std::cmp_less(row, m_services.size())) {
        return;
    }

    RunService& service = m_services.at(row);
    service.name = Utils::findUniqueString(m_name->text(), m_services, [](const RunService& rs) { return rs.name; });
    service.path = m_path->text();
    service.simultaneousRuns = m_simultaneousRuns->value();
    m_serviceList->item(row)->setText(service.name);
}
} // namespace

RunServicesPage::RunServicesPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::RunServices);
    setName(QObject::tr("Run Services"));
    setCategory({tr("Integrations"), tr("Run Services")});
    setWidgetCreator([settings] { return new RunServicesPageWidget{settings}; });
}
} // namespace Fooyin::RunServices

#include "runservicespage.moc"
