/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "fygui_export.h"

#include <utils/timer.h>

#include <QDialog>

class QLabel;
class QProgressBar;
class QTextEdit;

namespace Fooyin {
class ElidedLabel;

class FYGUI_EXPORT ElapsedProgressDialog : public QDialog
{
    Q_OBJECT

public:
    ElapsedProgressDialog(const QString& labelText, const QString& cancelButtonText, int minimum, int maximum,
                          QWidget* parent = nullptr);

    [[nodiscard]] int value() const;
    void setValue(int value);

    void setText(const QString& text);
    void setMinimumDuration(std::chrono::milliseconds duration);
    void setShowRemaining(bool show);

    void startTimer();
    [[nodiscard]] bool wasCancelled() const;
    [[nodiscard]] std::chrono::milliseconds elapsedTime() const;

    [[nodiscard]] QSize sizeHint() const override;

signals:
    void cancelled();

private:
    void updateStatus();

    QTextEdit* m_text;
    QProgressBar* m_progressBar;
    bool m_isStarting;
    bool m_isFinished;
    bool m_wasCancelled;

    std::chrono::milliseconds m_minDuration;
    QTimer* m_updateTimer;
    Timer m_elapsedTimer;
    QLabel* m_elapsedLabel;
    QLabel* m_remainingLabel;
    QPushButton* m_cancelButton;
};
} // namespace Fooyin
