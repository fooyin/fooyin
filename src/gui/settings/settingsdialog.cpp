/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "settingsdialog.h"

#include "settingspages.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QSize>
#include <QStackedWidget>

namespace Gui::Settings {
SettingsDialog::SettingsDialog(Core::Library::LibraryManager* libraryManager, Core::SettingsManager* settings,
                               QWidget* parent)
    : QDialog{parent}
    , m_libraryManager{libraryManager}
    , m_settings{settings}
    , m_contentsWidget{new QListWidget(this)}
    , m_pagesWidget{new QStackedWidget(this)}
{ }

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setupUi()
{
    m_contentsWidget->setViewMode(QListView::ListMode);
    //    m_contentsWidget->setIconSize(QSize(96, 84));
    m_contentsWidget->setMovement(QListView::Static);
    m_contentsWidget->setMaximumWidth(90);
    //    m_contentsWidget->setSpacing(10);

    m_pagesWidget->addWidget(new GeneralPage(m_settings, this));
    m_pagesWidget->addWidget(new LibraryPage(m_libraryManager, m_settings, this));
    m_pagesWidget->addWidget(new PlaylistPage(m_settings, this));

    auto* closeButton = new QPushButton("Close", this);

    // m_contentsWidget->setCurrentRow(0);

    connect(closeButton, &QAbstractButton::clicked, this, &QWidget::close);

    auto* horizontalLayout = new QHBoxLayout();
    horizontalLayout->addWidget(m_contentsWidget);
    horizontalLayout->addWidget(m_pagesWidget);

    auto* buttonsLayout = new QHBoxLayout();
    buttonsLayout->addStretch(1);
    buttonsLayout->addWidget(closeButton);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(horizontalLayout);
    mainLayout->addStretch(1);
    mainLayout->addSpacing(12);
    mainLayout->addLayout(buttonsLayout);

    setWindowTitle("Settings");

    createIcons();
    connect(m_contentsWidget, &QListWidget::currentItemChanged, this, &SettingsDialog::changePage);
}

void SettingsDialog::createIcons() const
{
    auto* generalButton = new QListWidgetItem(m_contentsWidget);
    generalButton->setText(tr("General"));
    generalButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    auto* libraryButton = new QListWidgetItem(m_contentsWidget);
    libraryButton->setText(tr("Library"));
    libraryButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    auto* playlistButton = new QListWidgetItem(m_contentsWidget);
    playlistButton->setText(tr("Playlist"));
    playlistButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

void SettingsDialog::changePage(QListWidgetItem* current, QListWidgetItem* previous)
{
    if(!current) {
        current = previous;
    }

    m_pagesWidget->setCurrentIndex(m_contentsWidget->row(current));
}

void SettingsDialog::openPage(Page page)
{
    auto index = static_cast<int>(page);
    m_contentsWidget->setCurrentRow(index);
    exec();
}
} // namespace Gui::Settings
