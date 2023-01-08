/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

namespace Core::Widgets {
struct SettingsDialog::Private
{
    Library::LibraryManager* libraryManager;
    QListWidget* contentsWidget;
    QStackedWidget* pagesWidget;

    explicit Private(Library::LibraryManager* libManager)
        : libraryManager(libManager)
    { }

    void createIcons() const
    {
        auto* generalButton = new QListWidgetItem(contentsWidget);
        generalButton->setText(tr("General"));
        generalButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        auto* libraryButton = new QListWidgetItem(contentsWidget);
        libraryButton->setText(tr("Library"));
        libraryButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        auto* playlistButton = new QListWidgetItem(contentsWidget);
        playlistButton->setText(tr("Playlist"));
        playlistButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    }
};

SettingsDialog::SettingsDialog(Library::LibraryManager* libManager, QWidget* parent)
    : QDialog(parent)
    , p(std::make_unique<Private>(libManager))
{
    setupUi();
    p->createIcons();
    connect(p->contentsWidget, &QListWidget::currentItemChanged, this, &SettingsDialog::changePage);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setupUi()
{
    p->contentsWidget = new QListWidget(this);
    p->contentsWidget->setViewMode(QListView::ListMode);
    //    p->contentsWidget->setIconSize(QSize(96, 84));
    p->contentsWidget->setMovement(QListView::Static);
    p->contentsWidget->setMaximumWidth(90);
    //    p->contentsWidget->setSpacing(10);

    p->pagesWidget = new QStackedWidget(this);
    p->pagesWidget->addWidget(new GeneralPage(this));
    p->pagesWidget->addWidget(new LibraryPage(p->libraryManager, this));
    p->pagesWidget->addWidget(new PlaylistPage(this));

    auto* closeButton = new QPushButton("Close", this);

    // p->contentsWidget->setCurrentRow(0);

    connect(closeButton, &QAbstractButton::clicked, this, &QWidget::close);

    auto* horizontalLayout = new QHBoxLayout();
    horizontalLayout->addWidget(p->contentsWidget);
    horizontalLayout->addWidget(p->pagesWidget);

    auto* buttonsLayout = new QHBoxLayout();
    buttonsLayout->addStretch(1);
    buttonsLayout->addWidget(closeButton);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(horizontalLayout);
    mainLayout->addStretch(1);
    mainLayout->addSpacing(12);
    mainLayout->addLayout(buttonsLayout);

    setWindowTitle("Settings");
}

void SettingsDialog::changePage(QListWidgetItem* current, QListWidgetItem* previous)
{
    if(!current) {
        current = previous;
    }

    p->pagesWidget->setCurrentIndex(p->contentsWidget->row(current));
}

void SettingsDialog::openPage(Page page)
{
    auto index = static_cast<int>(page);
    p->contentsWidget->setCurrentRow(index);
    show();
}
}; // namespace Core::Widgets
