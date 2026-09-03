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
 */

#include "iconpickerdialog.h"

#include <gui/iconloader.h>
#include <gui/widgets/expandedtreeview.h>

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

constexpr QSize IconItemSize = {140, 96};

namespace Fooyin {
IconPickerDialog::IconPickerDialog(const QIcon& commandIcon, QWidget* parent)
    : QDialog{parent}
    , m_icons{new ExpandedTreeView(this)}
    , m_filter{new QLineEdit(this)}
    , m_currentLabel{new QLabel(this)}
    , m_model{new QStandardItemModel(this)}
    , m_proxy{new QSortFilterProxyModel(this)}
{
    setWindowTitle(tr("Choose Icon"));
    setMinimumSize(650, 480);

    m_filter->setPlaceholderText(tr("Filter icons"));
    m_filter->setClearButtonEnabled(true);

    auto* defaultItem = new QStandardItem(commandIcon, tr("Use command icon"));
    defaultItem->setEditable(false);
    defaultItem->setData(true, DefaultIconRole);
    defaultItem->setSizeHint(IconItemSize);
    defaultItem->setToolTip(tr("Use the icon supplied by the selected command"));
    m_model->appendRow(defaultItem);

    const auto icons = Gui::availableThemeIcons();
    for(const QString& iconName : icons) {
        auto* item = new QStandardItem(Gui::iconFromTheme(iconName), iconName);
        item->setEditable(false);
        item->setData(iconName, ThemeNameRole);
        item->setSizeHint(IconItemSize);
        item->setToolTip(iconName);
        m_model->appendRow(item);
    }

    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_icons->setModel(m_proxy);
    m_icons->setViewMode(ExpandedTreeView::ViewMode::Icon);
    m_icons->setCaptionDisplay(ExpandedTreeView::CaptionDisplay::Bottom);
    m_icons->setHeaderHidden(true);
    m_icons->setSelectionMode(QAbstractItemView::SingleSelection);
    m_icons->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_icons->changeIconSize({48, 48});
    m_icons->setTextElideMode(Qt::ElideMiddle);

    auto* buttons      = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* browseButton = buttons->addButton(tr("Browse custom image…"), QDialogButtonBox::ActionRole);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Select"));

    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(m_currentLabel, 1);
    bottomLayout->addWidget(buttons);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_filter);
    layout->addWidget(m_icons, 1);
    layout->addLayout(bottomLayout);

    QObject::connect(m_filter, &QLineEdit::textChanged, m_proxy, &QSortFilterProxyModel::setFilterFixedString);
    QObject::connect(m_icons->selectionModel(), &QItemSelectionModel::currentChanged, this,
                     &IconPickerDialog::currentChanged);
    QObject::connect(m_icons, &QAbstractItemView::doubleClicked, this, &IconPickerDialog::activateCurrent);
    QObject::connect(browseButton, &QPushButton::clicked, this, &IconPickerDialog::browseForImage);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setSelection({});
}

IconSelection IconPickerDialog::selection() const
{
    return m_selection;
}

void IconPickerDialog::setSelection(const IconSelection& selection)
{
    m_selection.themeName  = selection.themeName.trimmed();
    m_selection.customPath = selection.customPath.trimmed();
    if(!m_selection.themeName.isEmpty()) {
        m_selection.customPath.clear();
    }

    QModelIndex sourceIndex;
    if(m_selection.themeName.isEmpty() && m_selection.customPath.isEmpty()) {
        sourceIndex = m_model->index(0, 0);
    }
    else if(!m_selection.themeName.isEmpty()) {
        const auto matches = m_model->match(m_model->index(0, 0), ThemeNameRole, m_selection.themeName);
        if(!matches.isEmpty()) {
            sourceIndex = matches.front();
        }
    }

    const QModelIndex proxyIndex = m_proxy->mapFromSource(sourceIndex);
    if(proxyIndex.isValid()) {
        m_icons->setCurrentIndex(proxyIndex);
        m_icons->scrollTo(proxyIndex, QAbstractItemView::EnsureVisible);
    }
    else {
        m_icons->clearSelection();
        m_icons->setCurrentIndex({});
    }

    updateCurrentLabel();
}

QString IconPickerDialog::imageFilter()
{
    const auto formats = QImageReader::supportedImageFormats();

    QStringList wildcards;
    for(const QByteArray& format : formats) {
        wildcards.emplace_back(u"*.%1"_s.arg(QString::fromLatin1(format).toLower()));
    }

    wildcards.removeDuplicates();
    wildcards.sort();

    const QString imageFiles
        = wildcards.isEmpty() ? tr("All files (*)") : tr("Images") + u" ("_s + wildcards.join(u' ') + u")"_s;
    return imageFiles + u";;"_s + tr("All files (*)");
}

void IconPickerDialog::currentChanged(const QModelIndex& current)
{
    if(!current.isValid()) {
        return;
    }

    if(current.data(DefaultIconRole).toBool()) {
        m_selection = {};
    }
    else {
        m_selection = {.themeName = current.data(ThemeNameRole).toString(), .customPath = {}};
    }
    updateCurrentLabel();
}

void IconPickerDialog::activateCurrent(const QModelIndex& index)
{
    if(index.isValid()) {
        accept();
    }
}

void IconPickerDialog::browseForImage()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Select Icon"), m_selection.customPath, imageFilter());
    if(path.isEmpty()) {
        return;
    }

    m_selection = {.themeName = {}, .customPath = path};
    accept();
}

void IconPickerDialog::updateCurrentLabel()
{
    if(!m_selection.customPath.isEmpty()) {
        m_currentLabel->setText(tr("Current: Custom image — %1").arg(QFileInfo{m_selection.customPath}.fileName()));
        m_currentLabel->setToolTip(m_selection.customPath);
    }
    else if(!m_selection.themeName.isEmpty()) {
        m_currentLabel->setText(tr("Current: %1").arg(m_selection.themeName));
        m_currentLabel->setToolTip(m_selection.themeName);
    }
    else {
        m_currentLabel->setText(tr("Current: Use command icon"));
        m_currentLabel->setToolTip({});
    }
}
} // namespace Fooyin

#include "moc_iconpickerdialog.cpp"
