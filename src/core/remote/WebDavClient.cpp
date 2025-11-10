#include "core/remote/WebDavClient.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QTimer>

namespace
{
    constexpr int kDefaultTimeoutMsec = 30000;

    bool isWebDavScheme(const QString& scheme)
    {
        return scheme.compare(QStringLiteral("webdav"), Qt::CaseInsensitive) == 0
               || scheme.compare(QStringLiteral("webdavs"), Qt::CaseInsensitive) == 0;
    }
} // namespace

bool WebDavClient::download(const RequestOptions& options, QByteArray& data, QString* error) const
{
    QNetworkAccessManager manager;
    auto request = QNetworkRequest(effectiveUrl(options.url));
    applyBasicAuth(options, request);

    auto reply = manager.get(request);
    if (!waitForFinished(reply, options.timeoutMsec <= 0 ? kDefaultTimeoutMsec : options.timeoutMsec, error)) {
        reply->deleteLater();
        return false;
    }

    const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || (statusCode >= 400 && statusCode != 401)) {
        if (error) {
            *error = reply->errorString();
        }
        reply->deleteLater();
        return false;
    }

    data = reply->readAll();
    reply->deleteLater();
    return true;
}

bool WebDavClient::upload(const RequestOptions& options, const QByteArray& data, QString* error) const
{
    QNetworkAccessManager manager;
    auto request = QNetworkRequest(effectiveUrl(options.url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
    applyBasicAuth(options, request);

    auto reply = manager.put(request, data);
    if (!waitForFinished(reply, options.timeoutMsec <= 0 ? kDefaultTimeoutMsec : options.timeoutMsec, error)) {
        reply->deleteLater();
        return false;
    }

    const auto statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || (statusCode >= 400 && statusCode != 401 && statusCode != 204)) {
        if (error) {
            *error = reply->errorString();
        }
        reply->deleteLater();
        return false;
    }

    reply->deleteLater();
    return true;
}

QUrl WebDavClient::effectiveUrl(const QUrl& original)
{
    if (!isWebDavScheme(original.scheme())) {
        return original;
    }

    QUrl copy(original);
    if (copy.scheme().compare(QStringLiteral("webdav"), Qt::CaseInsensitive) == 0) {
        copy.setScheme(QStringLiteral("http"));
    } else if (copy.scheme().compare(QStringLiteral("webdavs"), Qt::CaseInsensitive) == 0) {
        copy.setScheme(QStringLiteral("https"));
    }
    return copy;
}

void WebDavClient::applyBasicAuth(const RequestOptions& options, QNetworkRequest& request)
{
    if (!options.useAuthentication || options.username.isEmpty()) {
        return;
    }

    const QByteArray credentials =
        QStringLiteral("%1:%2").arg(options.username, options.password).toUtf8().toBase64();
    request.setRawHeader("Authorization", "Basic " + credentials);
}

bool WebDavClient::waitForFinished(QNetworkReply* reply, int timeoutMsec, QString* error)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timer.start(timeoutMsec);
    loop.exec();

    if (timer.isActive()) {
        timer.stop();
        return true;
    }

    QObject::disconnect(reply, nullptr, &loop, nullptr);
    reply->abort();

    if (error) {
        *error = QObject::tr("WebDAV request timed out after %1 ms").arg(timeoutMsec);
    }
    return false;
}

