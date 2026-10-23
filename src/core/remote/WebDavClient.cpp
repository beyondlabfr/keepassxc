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

#include "WebDavClient.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace
{
    constexpr int kMaxReadRetries = 2;
    constexpr int kRetryDelayMsec = 500;

    void waitFor(QNetworkReply* reply)
    {
        if (reply->isFinished()) {
            return;
        }
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }

    void pause(int msec)
    {
        QEventLoop loop;
        QTimer::singleShot(msec, &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
} // namespace

bool WebDavClient::isWebDavScheme(const QString& scheme)
{
    return scheme.compare(QLatin1String("http"), Qt::CaseInsensitive) == 0
           || scheme.compare(QLatin1String("https"), Qt::CaseInsensitive) == 0
           || scheme.compare(QLatin1String("webdav"), Qt::CaseInsensitive) == 0
           || scheme.compare(QLatin1String("webdavs"), Qt::CaseInsensitive) == 0;
}

bool WebDavClient::isWebDavUrl(const QString& path)
{
    // Requiring "://" keeps Windows drive letters (C:/...) and plain paths out
    if (!path.contains(QLatin1String("://"))) {
        return false;
    }
    const QUrl url(path);
    return url.isValid() && isWebDavScheme(url.scheme());
}

QUrl WebDavClient::toHttpUrl(const QUrl& original)
{
    QUrl copy(original);
    if (copy.scheme().compare(QLatin1String("webdav"), Qt::CaseInsensitive) == 0) {
        copy.setScheme(QStringLiteral("http"));
    } else if (copy.scheme().compare(QLatin1String("webdavs"), Qt::CaseInsensitive) == 0) {
        copy.setScheme(QStringLiteral("https"));
    }
    return copy;
}

QUrl WebDavClient::sanitizeUrl(const QUrl& url)
{
    QUrl copy(url);
    copy.setUserInfo(QString());
    return copy;
}

QString WebDavClient::normalizePath(const QUrl& url)
{
    return toHttpUrl(sanitizeUrl(url)).toString(QUrl::FullyEncoded);
}

bool WebDavClient::isStrongETag(const QByteArray& etag)
{
    return !etag.isEmpty() && !etag.startsWith("W/");
}

WebDavClient::Response WebDavClient::get(const RequestOptions& options)
{
    return sendWithRetry("GET", options);
}

WebDavClient::Response WebDavClient::head(const RequestOptions& options)
{
    return sendWithRetry("HEAD", options);
}

WebDavClient::Response
WebDavClient::put(const RequestOptions& options, const QByteArray& data, const QByteArray& ifMatch)
{
    QList<QPair<QByteArray, QByteArray>> headers;
    headers.append(qMakePair(QByteArray("Content-Type"), QByteArray("application/octet-stream")));
    if (!ifMatch.isEmpty()) {
        headers.append(qMakePair(QByteArray("If-Match"), ifMatch));
    }
    // Writes are not retried: a retry after a timeout could hit our own first write and report a false conflict
    return send("PUT", options, data, headers);
}

WebDavClient::Response WebDavClient::move(const RequestOptions& options, const QUrl& destination)
{
    return send(
        "MOVE", options, {}, {{"Destination", toHttpUrl(sanitizeUrl(destination)).toEncoded()}, {"Overwrite", "T"}});
}

WebDavClient::Response WebDavClient::remove(const RequestOptions& options)
{
    return send("DELETE", options);
}

WebDavClient::Response WebDavClient::sendWithRetry(const QByteArray& method, const RequestOptions& options)
{
    Response response;
    for (int attempt = 0; attempt <= kMaxReadRetries; ++attempt) {
        if (attempt > 0) {
            pause(kRetryDelayMsec * attempt);
        }
        response = send(method, options);
        // Only retry what may be transient: no answer at all, or a server-side error
        if (response.ok() || (response.status != 0 && response.status < 500)) {
            break;
        }
    }
    return response;
}

WebDavClient::Response WebDavClient::send(const QByteArray& method,
                                          const RequestOptions& options,
                                          const QByteArray& body,
                                          const QList<QPair<QByteArray, QByteArray>>& headers)
{
    QNetworkRequest request(toHttpUrl(options.url));
    // A redirected write would be reported as a success without storing anything
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    // Inactivity timeout: slow but progressing transfers of large databases are not cut off
    request.setTransferTimeout(options.timeoutMsec > 0 ? options.timeoutMsec : 30000);
    // Do not set Accept-Encoding by hand: Qt only decompresses replies when it negotiated compression itself
    if (options.useAuthentication && !options.username.isEmpty()) {
        const auto credentials = QStringLiteral("%1:%2").arg(options.username, options.password).toUtf8().toBase64();
        request.setRawHeader("Authorization", "Basic " + credentials);
    }
    for (const auto& header : headers) {
        request.setRawHeader(header.first, header.second);
    }

    // Use the dedicated calls where they exist: as a custom verb, HEAD would make Qt wait for a body
    // that never comes until the server drops the connection
    QNetworkReply* reply;
    auto* manager = networkManager();
    if (method == "GET") {
        reply = manager->get(request);
    } else if (method == "HEAD") {
        reply = manager->head(request);
    } else if (method == "PUT") {
        reply = manager->put(request, body);
    } else if (method == "DELETE") {
        reply = manager->deleteResource(request);
    } else {
        reply = manager->sendCustomRequest(request, method, body);
    }
    waitFor(reply);

    Response response;
    response.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    response.etag = reply->rawHeader("ETag");
    if (response.status >= 200 && response.status < 300) {
        response.body = reply->readAll();
    } else {
        response.error = describeFailure(reply, response.status, options);
    }
    reply->deleteLater();
    return response;
}

QString WebDavClient::describeFailure(QNetworkReply* reply, int status, const RequestOptions& options)
{
    // Check the HTTP status before reply->error(): Qt reports 401/403 as AuthenticationRequiredError
    if (status == 401 || status == 403) {
        if (!options.useAuthentication || options.username.isEmpty() || options.password.isEmpty()) {
            return tr("WebDAV: the server requires a user name and password (HTTP %1).").arg(status);
        }
        return tr("WebDAV: the server rejected the user name or password (HTTP %1).").arg(status);
    }
    if (status >= 300 && status < 400) {
        const auto location = reply->rawHeader("Location");
        return tr("WebDAV: the server redirects to %1, use this address instead (HTTP %2).")
            .arg(QString::fromUtf8(location))
            .arg(status);
    }
    if (status == 404) {
        return tr("WebDAV: file not found on the server (HTTP 404).");
    }
    if (status == 412) {
        return tr("WebDAV: the file was modified on the server (HTTP 412).");
    }
    if (reply->error() == QNetworkReply::OperationCanceledError || reply->error() == QNetworkReply::TimeoutError) {
        return tr("WebDAV: the server did not answer within %1 seconds.")
            .arg((options.timeoutMsec > 0 ? options.timeoutMsec : 30000) / 1000);
    }
    if (status != 0) {
        return tr("WebDAV: the server answered HTTP %1 %2.")
            .arg(status)
            .arg(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
    }
    return tr("WebDAV: %1").arg(reply->errorString());
}

QNetworkAccessManager* WebDavClient::networkManager()
{
    static QNetworkAccessManager* manager = [] {
        auto* nam = new QNetworkAccessManager(QCoreApplication::instance());
        // Each request carries its own transfer timeout
        nam->setTransferTimeout(0);
        return nam;
    }();
    return manager;
}
