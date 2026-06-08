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

#include "radiostation.h"

#include <QBasicTimer>
#include <QDialog>

#include <array>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTimerEvent;
class QWidget;

namespace Fooyin {
class ClickableLabel;

namespace RadioBrowser {
class RadioBrowserController;

class RadioStationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RadioStationDialog(RadioBrowserController* controller, QWidget* parent = nullptr);
    explicit RadioStationDialog(RadioBrowserController* controller, const RadioStation& station,
                                QWidget* parent = nullptr);

    static RadioStationDialog* createDetailsDialog(RadioBrowserController* controller, const RadioStation& station,
                                                   QWidget* parent = nullptr);

    [[nodiscard]] RadioStation station() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    enum class DetailField : uint8_t
    {
        Homepage = 0,
        Favicon,
        Tags,
        Country,
        Language,
        Codec,
        Bitrate,
        Count,
    };

    enum class DetailSource : uint8_t
    {
        Empty = 0,
        Lookup,
        Manual,
    };

    enum class Mode : uint8_t
    {
        Add = 0,
        Edit,
        Details,
    };

    RadioStationDialog(RadioBrowserController* controller, const RadioStation& station, Mode mode,
                       QWidget* parent = nullptr);

    void initialise();
    void setReadOnly(bool readOnly);
    void handleStreamUrlChanged();
    void scheduleValidation();
    void validateStation();
    void setValidationPending();
    void setValidationFailed(const QString& error);
    void setValidationSucceeded(const RadioStation& station);
    void setLookupFinished(const RadioStationList& stations);
    void setLookupFailed();
    void updateAcceptState();
    void updateValidatedStationFromFields();
    void setDetailsVisible(bool visible);
    void populateDetails(const RadioStation& station, bool overwrite);
    void clearLookupDetails();
    void markDetailEdited(DetailField field);
    void setDetailText(DetailField field, const QString& value, DetailSource source);
    void setDetailValue(DetailField field, int value, DetailSource source);
    void updateIcon() const;

    [[nodiscard]] RadioStation inputStation() const;
    [[nodiscard]] RadioStation stationWithDetails(RadioStation station) const;
    [[nodiscard]] QLineEdit* detailLineEdit(DetailField field) const;
    [[nodiscard]] DetailSource detailSource(DetailField field) const;
    [[nodiscard]] bool canPopulateDetail(DetailField field, bool overwrite) const;

    RadioBrowserController* m_controller;

    QLabel* m_icon;
    QLabel* m_status;
    QLineEdit* m_name;
    QLineEdit* m_streamUrl;
    ClickableLabel* m_detailsToggle;
    QWidget* m_detailsWidget;
    QLineEdit* m_homepage;
    QLineEdit* m_favicon;
    QLineEdit* m_tags;
    QLineEdit* m_country;
    QLineEdit* m_language;
    QLineEdit* m_codec;
    QSpinBox* m_bitrate;
    QDialogButtonBox* m_buttons;
    QBasicTimer m_validationTimer;
    RadioStation m_initialStation;
    RadioStation m_validatedStation;
    QString m_lookupUrl;
    QString m_lookupFallbackUrl;
    std::array<DetailSource, static_cast<size_t>(DetailField::Count)> m_detailSources;
    Mode m_mode;
    bool m_validated;
    bool m_detailsVisible;
    bool m_updatingDetails;
};
} // namespace RadioBrowser
} // namespace Fooyin
