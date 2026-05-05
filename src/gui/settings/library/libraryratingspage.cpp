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

#include "libraryratingspage.h"

#include <core/engine/input/ratingtagpolicy.h>
#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
enum class RatingScaleUsage
{
    Read,
    Write,
};

void addRatingTagItems(QComboBox* combo, bool includeAutomatic)
{
    if(includeAutomatic) {
        combo->addItem(QObject::tr("Automatic detection"), u"Automatic"_s);
    }
    combo->addItem(u"FMPS_RATING"_s, u"FMPS_RATING"_s);
    combo->addItem(u"RATING"_s, u"RATING"_s);
    combo->setEditable(true);
}

void addRatingScaleItems(QComboBox* combo, RatingScaleUsage usage)
{
    for(const auto& descriptor : ratingScaleDescriptors()) {
        const bool available
            = usage == RatingScaleUsage::Read ? descriptor.availableForRead : descriptor.availableForWrite;
        if(!available) {
            continue;
        }
        combo->addItem(QObject::tr(descriptor.label), ratingScaleKey(descriptor.scale));
    }
}

void addPopmMappingItems(QComboBox* combo)
{
    for(const auto& descriptor : popmMappingDescriptors()) {
        combo->addItem(QObject::tr(descriptor.label), popmMappingKey(descriptor.mapping));

        QString tooltip;
        switch(descriptor.mapping) {
            case PopmMapping::Default:
                tooltip = QObject::tr(
                    "Use fooyin's default POPM byte conversion when reading and writing MP3 ratings.\n"
                    "This supports intermediate rating steps, but does not treat the POPM byte as a fully linear "
                    "0-255 scale.");
                break;
            case PopmMapping::CommonFiveStar:
                tooltip = QObject::tr("Use common POPM byte values for whole-star ratings only.\n"
                                      "Intermediate ratings are rounded to one, two, three, four, or five stars.");
                break;
            case PopmMapping::LinearByte:
                tooltip = QObject::tr("Map fooyin ratings directly onto the full POPM byte range.");
                break;
        }

        combo->setItemData(combo->count() - 1, tooltip, Qt::ToolTipRole);
    }
}

QString comboSettingValue(const QComboBox* combo)
{
    const QVariant data = combo->currentData();
    return data.isValid() ? data.toString() : combo->currentText().trimmed();
}

void setComboSettingValue(QComboBox* combo, const QString& value)
{
    const int index = combo->findData(value);
    if(index >= 0) {
        combo->setCurrentIndex(index);
        return;
    }
    combo->setCurrentText(value);
}
} // namespace

class LibraryRatingsPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit LibraryRatingsPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QComboBox* m_ratingReadTag;
    QComboBox* m_ratingReadScale;
    QComboBox* m_ratingWriteTag;
    QComboBox* m_ratingWriteScale;
    QCheckBox* m_ratingReadPopm;
    QCheckBox* m_ratingWritePopm;
    QLineEdit* m_ratingPopmOwner;
    QComboBox* m_ratingPopmMapping;
    QCheckBox* m_ratingReadAsfSharedRating;
    QCheckBox* m_ratingWriteAsfSharedRating;
};

LibraryRatingsPageWidget::LibraryRatingsPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_ratingReadTag{new QComboBox(this)}
    , m_ratingReadScale{new QComboBox(this)}
    , m_ratingWriteTag{new QComboBox(this)}
    , m_ratingWriteScale{new QComboBox(this)}
    , m_ratingReadPopm{new QCheckBox(tr("Read ID3 POPM"), this)}
    , m_ratingWritePopm{new QCheckBox(tr("Write ID3 POPM"), this)}
    , m_ratingPopmOwner{new QLineEdit(this)}
    , m_ratingPopmMapping{new QComboBox(this)}
    , m_ratingReadAsfSharedRating{new QCheckBox(tr("Read WM/SharedUserRating"), this)}
    , m_ratingWriteAsfSharedRating{new QCheckBox(tr("Write WM/SharedUserRating"), this)}
{
    addRatingTagItems(m_ratingReadTag, true);
    addRatingTagItems(m_ratingWriteTag, false);
    addRatingScaleItems(m_ratingReadScale, RatingScaleUsage::Read);
    addRatingScaleItems(m_ratingWriteScale, RatingScaleUsage::Write);
    addPopmMappingItems(m_ratingPopmMapping);

    m_ratingReadTag->setToolTip(
        tr("Tag field to read rating values from. Automatic detection prefers FMPS_RATING, then RATING."));
    m_ratingReadScale->setToolTip(tr(
        "How text rating values are interpreted. Automatic detection handles common rating formats; other choices use "
        "the selected scale exactly."));
    m_ratingWriteTag->setToolTip(
        tr("Tag field used when saving ratings as text metadata. Unrated tracks remove this tag."));
    m_ratingWriteScale->setToolTip(tr(
        "Value range used when saving rated values as text metadata. Unrated tracks remove the rating tag instead of "
        "writing zero."));
    m_ratingReadPopm->setToolTip(tr("Read ratings from ID3 POPM frames in MP3 files before text rating tags."));
    m_ratingWritePopm->setToolTip(
        tr("Additionally save ratings to ID3 POPM frames when writing MP3 files. Other formats are unaffected."));
    m_ratingPopmOwner->setToolTip(
        tr("Owner identifier for POPM frames. Leave empty to read the first POPM frame and write an empty owner."));
    m_ratingPopmMapping->setToolTip(tr("Conversion table used between fooyin ratings and POPM byte values."));
    m_ratingReadAsfSharedRating->setToolTip(
        tr("Read ratings from WM/SharedUserRating attributes in ASF/WMA files after text rating tags."));
    m_ratingWriteAsfSharedRating->setToolTip(
        tr("Additionally save ratings to WM/SharedUserRating attributes when writing ASF/WMA files.\n"
           "This improves compatibility with other players, but stores whole-star values only."));

    auto* ratingTagsGroup  = new QGroupBox(tr("Rating Tags"), this);
    auto* ratingTagsLayout = new QGridLayout(ratingTagsGroup);
    auto* popmGroup        = new QGroupBox(tr("ID3 POPM"), ratingTagsGroup);
    auto* popmLayout       = new QGridLayout(popmGroup);
    auto* asfGroup         = new QGroupBox(tr("ASF/WMA"), ratingTagsGroup);
    auto* asfLayout        = new QGridLayout(asfGroup);

    int row{0};
    ratingTagsLayout->addWidget(new QLabel(tr("Read text rating from") + ":"_L1, this), row, 0);
    ratingTagsLayout->addWidget(m_ratingReadTag, row++, 1);
    ratingTagsLayout->addWidget(new QLabel(tr("Read value scale") + ":"_L1, this), row, 0);
    ratingTagsLayout->addWidget(m_ratingReadScale, row++, 1);
    ratingTagsLayout->addWidget(new QLabel(tr("Write text rating to") + ":"_L1, this), row, 0);
    ratingTagsLayout->addWidget(m_ratingWriteTag, row++, 1);
    ratingTagsLayout->addWidget(new QLabel(tr("Write value scale") + ":"_L1, this), row, 0);
    ratingTagsLayout->addWidget(m_ratingWriteScale, row++, 1);
    ratingTagsLayout->addWidget(popmGroup, row++, 0, 1, 2);
    ratingTagsLayout->addWidget(asfGroup, row++, 0, 1, 2);
    ratingTagsLayout->setColumnStretch(1, 1);

    row = 0;
    popmLayout->addWidget(m_ratingReadPopm, row++, 0, 1, 2);
    popmLayout->addWidget(m_ratingWritePopm, row++, 0, 1, 2);
    popmLayout->addWidget(new QLabel(tr("Owner") + ":"_L1, this), row, 0);
    popmLayout->addWidget(m_ratingPopmOwner, row++, 1);
    popmLayout->addWidget(new QLabel(tr("Mapping") + ":"_L1, this), row, 0);
    popmLayout->addWidget(m_ratingPopmMapping, row++, 1);
    popmLayout->setColumnStretch(1, 1);

    row = 0;
    asfLayout->addWidget(m_ratingReadAsfSharedRating, row++, 0);
    asfLayout->addWidget(m_ratingWriteAsfSharedRating, row++, 0);

    auto* mainLayout = new QGridLayout(this);
    row              = 0;
    mainLayout->addWidget(ratingTagsGroup, row++, 0);
    mainLayout->setRowStretch(row, 1);
}

void LibraryRatingsPageWidget::load()
{
    setComboSettingValue(
        m_ratingReadTag,
        m_settings->fileValue(RatingSettings::ReadTag, QLatin1StringView{RatingSettings::DefaultAutomatic}).toString());
    setComboSettingValue(
        m_ratingReadScale,
        m_settings->fileValue(RatingSettings::ReadScale, QLatin1StringView{RatingSettings::DefaultAutomatic})
            .toString());
    setComboSettingValue(
        m_ratingWriteTag,
        m_settings->fileValue(RatingSettings::WriteTag, QLatin1StringView{RatingSettings::DefaultFmpsRatingTag})
            .toString());
    setComboSettingValue(
        m_ratingWriteScale,
        m_settings
            ->fileValue(RatingSettings::WriteScale, QLatin1StringView{RatingSettings::DefaultNormalizedRatingScale})
            .toString());
    m_ratingReadPopm->setChecked(
        m_settings->fileValue(RatingSettings::ReadId3Popm, RatingSettings::DefaultReadId3Popm).toBool());
    m_ratingWritePopm->setChecked(
        m_settings->fileValue(RatingSettings::WriteId3Popm, RatingSettings::DefaultWriteId3Popm).toBool());
    m_ratingPopmOwner->setText(m_settings->fileValue(RatingSettings::PopmOwner, QString{}).toString());
    setComboSettingValue(
        m_ratingPopmMapping,
        m_settings->fileValue(RatingSettings::PopmMapping, QLatin1StringView{RatingSettings::DefaultPopmMapping})
            .toString());
    m_ratingReadAsfSharedRating->setChecked(
        m_settings->fileValue(RatingSettings::ReadAsfSharedRating, RatingSettings::DefaultReadAsfSharedRating)
            .toBool());
    m_ratingWriteAsfSharedRating->setChecked(
        m_settings->fileValue(RatingSettings::WriteAsfSharedRating, RatingSettings::DefaultWriteAsfSharedRating)
            .toBool());
}

void LibraryRatingsPageWidget::apply()
{
    m_settings->fileSet(RatingSettings::ReadTag, comboSettingValue(m_ratingReadTag));
    m_settings->fileSet(RatingSettings::ReadScale, comboSettingValue(m_ratingReadScale));
    m_settings->fileSet(RatingSettings::WriteTag, comboSettingValue(m_ratingWriteTag));
    m_settings->fileSet(RatingSettings::WriteScale, comboSettingValue(m_ratingWriteScale));
    m_settings->fileSet(RatingSettings::ReadId3Popm, m_ratingReadPopm->isChecked());
    m_settings->fileSet(RatingSettings::WriteId3Popm, m_ratingWritePopm->isChecked());
    m_settings->fileSet(RatingSettings::PopmOwner, m_ratingPopmOwner->text().trimmed());
    m_settings->fileSet(RatingSettings::PopmMapping, comboSettingValue(m_ratingPopmMapping));
    m_settings->fileSet(RatingSettings::ReadAsfSharedRating, m_ratingReadAsfSharedRating->isChecked());
    m_settings->fileSet(RatingSettings::WriteAsfSharedRating, m_ratingWriteAsfSharedRating->isChecked());
}

void LibraryRatingsPageWidget::reset()
{
    m_settings->fileRemove(RatingSettings::ReadTag);
    m_settings->fileRemove(RatingSettings::ReadScale);
    m_settings->fileRemove(RatingSettings::WriteTag);
    m_settings->fileRemove(RatingSettings::WriteScale);
    m_settings->fileRemove(RatingSettings::ReadId3Popm);
    m_settings->fileRemove(RatingSettings::WriteId3Popm);
    m_settings->fileRemove(RatingSettings::PopmOwner);
    m_settings->fileRemove(RatingSettings::PopmMapping);
    m_settings->fileRemove(RatingSettings::ReadAsfSharedRating);
    m_settings->fileRemove(RatingSettings::WriteAsfSharedRating);
}

LibraryRatingsPage::LibraryRatingsPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::LibraryRatings);
    setName(tr("Ratings"));
    setCategory({tr("Library"), tr("Metadata")});
    setWidgetCreator([settings] { return new LibraryRatingsPageWidget(settings); });
}
} // namespace Fooyin

#include "libraryratingspage.moc"
#include "moc_libraryratingspage.cpp"
