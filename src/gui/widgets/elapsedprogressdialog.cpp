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

#include <gui/widgets/elapsedprogressdialog.h>

#include <utils/timer.h>
#include <utils/utils.h>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>

namespace Fooyin {
ElapsedProgressDialog::ElapsedProgressDialog(const QString& labelText, const QString& cancelButtonText, int minimum,
                                             int maximum, QWidget* parent)
    : QDialog{parent}
    , m_text{new QTextEdit(this)}
    , m_progressBar{new QProgressBar(this)}
    , m_wasCancelled{false}
    , m_updateTimer{new QTimer(this)}
    , m_elapsedLabel{new QLabel(this)}
    , m_remainingLabel{new QLabel(this)}
    , m_cancelButton{new QPushButton(this)}
{
    auto* statusLayout = new QHBoxLayout();
    statusLayout->addWidget(m_elapsedLabel);
    statusLayout->addWidget(m_remainingLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_cancelButton);

    auto* layout = new QGridLayout(this);
    layout->addWidget(m_text, 0, 0);
    layout->addWidget(m_progressBar, 1, 0);
    layout->addLayout(statusLayout, 2, 0);

    m_text->setText(labelText);
    m_text->setReadOnly(true);
    m_cancelButton->setText(cancelButtonText);
    m_progressBar->setRange(minimum, maximum);
    m_progressBar->setValue(minimum);

    m_updateTimer->setInterval(1000);
    QObject::connect(m_updateTimer, &QTimer::timeout, this, &ElapsedProgressDialog::updateStatus);
    QObject::connect(m_cancelButton, &QAbstractButton::clicked, this, [this]() {
        m_wasCancelled = true;
        m_updateTimer->stop();
        hide();
    });

    m_updateTimer->start();
    m_elapsedTimer.reset();
    updateStatus();
    show();
}

int ElapsedProgressDialog::value() const
{
    return m_progressBar->value();
}

void ElapsedProgressDialog::setValue(int value)
{
    m_progressBar->setValue(value);
}

void ElapsedProgressDialog::setText(const QString& text)
{
    m_text->setText(text);
}

void ElapsedProgressDialog::startTimer()
{
    m_elapsedTimer.reset();
}

bool ElapsedProgressDialog::wasCancelled() const
{
    return m_wasCancelled;
}

std::chrono::milliseconds ElapsedProgressDialog::elapsedTime() const
{
    return m_elapsedTimer.elapsed();
}

QSize ElapsedProgressDialog::sizeHint() const
{
    return {400, 150};
}

void ElapsedProgressDialog::updateStatus()
{
    m_elapsedLabel->setText(tr("Time elapsed") + u": " + Utils::msToString(m_elapsedTimer.elapsed(), false));

    const int current = m_progressBar->value();
    const int max     = m_progressBar->maximum();
    const int min     = m_progressBar->minimum();

    const QString remainingText = tr("Estimated") + u": ";

    if(current <= min) {
        m_remainingLabel->setText(remainingText + tr("Calculating…"));
        return;
    }

    const auto elapsedMs     = static_cast<double>(m_elapsedTimer.elapsed().count());
    const double elapsedSecs = elapsedMs / 1000.0;

    const int remaining        = max - current;
    const double remainingSecs = remaining / (current / elapsedSecs);
    const auto remainingMs     = static_cast<std::chrono::milliseconds>(static_cast<int>(remainingSecs * 1000));

    m_remainingLabel->setText(remainingText + Utils::msToString(remainingMs, false));
}
} // namespace Fooyin
