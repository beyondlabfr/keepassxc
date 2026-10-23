/*
 *  Copyright (C) 2026 BeyondLab
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEEPASSXC_WEBDAVCLIENT_H
#define KEEPASSXC_WEBDAVCLIENT_H

#include <QByteArray>
#include <QCoreApplication>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * Minimal synchronous WebDAV client used to open and save databases stored on a WebDAV server.
 *
 * Requests run in a local event loop that ignores user input, so the GUI keeps painting
 * but cannot re-enter the caller while a request is in flight.
 */
class WebDavClient
{
    Q_DECLARE_TR_FUNCTIONS(WebDavClient)

public:
    struct RequestOptions
    {
        QUrl url;
        QString username;
        QString password;
        int timeoutMsec = 30000;
        bool useAuthentication = false;
    };

    struct Response
    {
        int status = 0; // HTTP status code, 0 if the server could not be reached
        QByteArray body;
        QByteArray etag;
        QString error; // set when the request failed

        // A response that was never sent counts as a failure
        bool ok() const
        {
            return status >= 200 && status < 300;
        }
    };

    Response get(const RequestOptions& options);
    Response head(const RequestOptions& options);
    // Sends If-Match when ifMatch is non-empty: a changed file answers 412 instead of being overwritten
    Response put(const RequestOptions& options, const QByteArray& data, const QByteArray& ifMatch = {});
    // Server-side rename, replacing the destination (Overwrite: T)
    Response move(const RequestOptions& options, const QUrl& destination);
    Response remove(const RequestOptions& options);

    static bool isWebDavScheme(const QString& scheme);
    static bool isWebDavUrl(const QString& path);
    static QUrl toHttpUrl(const QUrl& original);
    static QUrl sanitizeUrl(const QUrl& url);
    static QString normalizePath(const QUrl& url);
    // Weak ETags (W/"...") cannot be used with If-Match
    static bool isStrongETag(const QByteArray& etag);

private:
    Response send(const QByteArray& method,
                  const RequestOptions& options,
                  const QByteArray& body = {},
                  const QList<QPair<QByteArray, QByteArray>>& headers = {});
    Response sendWithRetry(const QByteArray& method, const RequestOptions& options);
    static QString describeFailure(QNetworkReply* reply, int status, const RequestOptions& options);

    static QNetworkAccessManager* networkManager();
};

#endif // KEEPASSXC_WEBDAVCLIENT_H
