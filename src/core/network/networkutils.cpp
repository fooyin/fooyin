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

#include <core/network/networkutils.h>

#include "version.h"

#include <QUrl>

using namespace Qt::StringLiterals;

namespace Fooyin {
QByteArray networkUserAgent()
{
    return u"fooyin/%1 (https://www.fooyin.org)"_s.arg(QString::fromLatin1(VERSION)).toUtf8();
}

QNetworkRequest makeNetworkRequest(const QUrl& url, NetworkRequestOptions options)
{
    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", networkUserAgent());

    if(options.testFlag(NetworkRequestOption::AlwaysNetwork)) {
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    }
    if(options.testFlag(NetworkRequestOption::AcceptAny)) {
        request.setRawHeader("Accept", "*/*");
    }
    if(options.testFlag(NetworkRequestOption::IcyMetadata)) {
        request.setRawHeader("Icy-MetaData", "1");
    }

    return request;
}
} // namespace Fooyin
