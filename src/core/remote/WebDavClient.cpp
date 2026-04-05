#include "core/remote/WebDavClient.h"

#include <QDebug>
#include <QEventLoop>

#include <cstdio>
#include <cstring>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QThread>
#include <QTimer>

QHash<QUrl, WebDavClient::ETagEntry> WebDavClient::s_etagCache;

bool WebDavClient::isWebDavScheme(const QString& scheme)
{
    return scheme.compare(QLatin1String("http"), Qt::CaseInsensitive) == 0
           || scheme.compare(QLatin1String("https"), Qt::CaseInsensitive) == 0
           || scheme.compare(QLatin1String("webdav"), Qt::CaseInsensitive) == 0
           || scheme.compare(QLatin1String("webdavs"), Qt::CaseInsensitive) == 0;
}

QUrl WebDavClient::toHttpUrl(const QUrl& original)
{
    if (original.scheme().compare(QLatin1String("webdav"), Qt::CaseInsensitive) != 0
        && original.scheme().compare(QLatin1String("webdavs"), Qt::CaseInsensitive) != 0) {
        return original;
    }

    QUrl copy(original);
    if (copy.scheme().compare(QLatin1String("webdav"), Qt::CaseInsensitive) == 0) {
        copy.setScheme(QStringLiteral("http"));
    } else {
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
    QUrl copy = toHttpUrl(sanitizeUrl(url));
    return copy.toString(QUrl::FullyEncoded);
}

bool WebDavClient::isDebugEnabled()
{
    static const bool enabled = [] {
        const QByteArray v = qgetenv("KEEPASSXC_DEBUG_WEBDAV");
        return !v.isEmpty() && v != "0" && v != "false" && v != "no";
    }();
    return enabled;
}

void WebDavClient::debugLog(const QString& message)
{
    if (!isDebugEnabled()) {
        return;
    }
    static bool s_bannerPrinted = false;
    if (!s_bannerPrinted) {
        s_bannerPrinted = true;
        static const char banner[] =
            "[KeePassXC WebDAV] debug logging enabled (set KEEPASSXC_DEBUG_WEBDAV=1 before launch)\n";
        fwrite(banner, 1, strlen(banner), stderr);
        fflush(stderr);
    }
    const QString line = QStringLiteral("[KeePassXC WebDAV] ") + message + QLatin1Char('\n');
    const QByteArray utf8 = line.toUtf8();
    fwrite(utf8.constData(), 1, static_cast<size_t>(utf8.size()), stderr);
    fflush(stderr);
    qDebug().noquote() << "[KeePassXC WebDAV]" << message;
}

bool WebDavClient::download(const RequestOptions& options, QByteArray& data, QString* error)
{
    auto* manager = networkManager();
    const QUrl httpUrl = toHttpUrl(options.url);
    const int timeout = options.timeoutMsec > 0 ? options.timeoutMsec : 30000;

    if (isDebugEnabled()) {
        debugLog(QStringLiteral("download: url=%1 useAuth=%2 userEmpty=%3 passEmpty=%4")
                     .arg(httpUrl.toString(QUrl::FullyEncoded))
                     .arg(options.useAuthentication)
                     .arg(options.username.isEmpty())
                     .arg(options.password.isEmpty()));
    }

    for (int attempt = 0; attempt <= kMaxRetries; ++attempt) {
        QNetworkRequest request(httpUrl);
        request.setRawHeader("Accept-Encoding", "gzip, deflate");
        applyBasicAuth(options, request);

        auto cacheIt = s_etagCache.constFind(options.url);
        if (cacheIt != s_etagCache.constEnd() && !cacheIt->etag.isEmpty()) {
            request.setRawHeader("If-None-Match", cacheIt->etag);
        }

        auto* reply = manager->get(request);
        if (!waitForReply(reply, timeout, error)) {
            reply->deleteLater();
            if (attempt < kMaxRetries) {
                QThread::msleep(kBaseRetryDelayMs * (1 << attempt));
                continue;
            }
            return false;
        }

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (statusCode == 304 && cacheIt != s_etagCache.constEnd()) {
            data = cacheIt->data;
            reply->deleteLater();
            return true;
        }

        if (statusCode >= 500 && attempt < kMaxRetries) {
            reply->deleteLater();
            QThread::msleep(kBaseRetryDelayMs * (1 << attempt));
            continue;
        }

        // HTTP 401/403: Qt met quand même reply->error() à AuthenticationRequiredError — il faut
        // traiter le statut HTTP avant reply->error(), sinon le message « Host requires authentication »
        // apparaît à tort (ex. au déverrouillage après verrouillage d'une base WebDAV).
        if (statusCode == 401 || statusCode == 403) {
            s_etagCache.remove(options.url);
            if (isDebugEnabled()) {
                debugLog(QStringLiteral("download HTTP %1: missingCredsBranch=%2 (useAuth=%3 userEmpty=%4 passEmpty=%5)")
                             .arg(statusCode)
                             .arg(!options.useAuthentication || options.username.isEmpty()
                                  || options.password.isEmpty())
                             .arg(options.useAuthentication)
                             .arg(options.username.isEmpty())
                             .arg(options.password.isEmpty()));
            }
            if (error) {
                if (!options.useAuthentication || options.username.isEmpty() || options.password.isEmpty()) {
                    *error = QObject::tr(
                        "WebDAV: the server requires authentication. Re-open the database from the WebDAV "
                        "menu and enter your remote username and password (enable \"Remember credentials\" "
                        "to keep them after locking the database).");
                } else {
                    *error = QObject::tr("WebDAV: the server rejected the credentials (HTTP %1).").arg(statusCode);
                }
            }
            reply->deleteLater();
            return false;
        }

        if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
            if (error) {
                *error = reply->errorString();
            }
            reply->deleteLater();
            return false;
        }

        data = reply->readAll();

        const QByteArray etag = reply->rawHeader("ETag");
        if (!etag.isEmpty()) {
            s_etagCache.insert(options.url, {etag, data});
        }

        reply->deleteLater();
        return true;
    }

    return false;
}

bool WebDavClient::upload(const RequestOptions& options, const QByteArray& data, QString* error)
{
    auto* manager = networkManager();
    const QUrl httpUrl = toHttpUrl(options.url);
    const int timeout = options.timeoutMsec > 0 ? options.timeoutMsec : 30000;

    if (isDebugEnabled()) {
        debugLog(QStringLiteral("upload: url=%1 useAuth=%2 userEmpty=%3 passEmpty=%4")
                     .arg(httpUrl.toString(QUrl::FullyEncoded))
                     .arg(options.useAuthentication)
                     .arg(options.username.isEmpty())
                     .arg(options.password.isEmpty()));
    }

    for (int attempt = 0; attempt <= kMaxRetries; ++attempt) {
        QNetworkRequest request(httpUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
        applyBasicAuth(options, request);

        auto* reply = manager->put(request, data);
        if (!waitForReply(reply, timeout, error)) {
            reply->deleteLater();
            if (attempt < kMaxRetries) {
                QThread::msleep(kBaseRetryDelayMs * (1 << attempt));
                continue;
            }
            return false;
        }

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (statusCode >= 500 && attempt < kMaxRetries) {
            reply->deleteLater();
            QThread::msleep(kBaseRetryDelayMs * (1 << attempt));
            continue;
        }

        if (statusCode == 401 || statusCode == 403) {
            s_etagCache.remove(options.url);
            if (isDebugEnabled()) {
                debugLog(QStringLiteral("upload HTTP %1: missingCredsBranch=%2 (useAuth=%3 userEmpty=%4 passEmpty=%5)")
                             .arg(statusCode)
                             .arg(!options.useAuthentication || options.username.isEmpty()
                                  || options.password.isEmpty())
                             .arg(options.useAuthentication)
                             .arg(options.username.isEmpty())
                             .arg(options.password.isEmpty()));
            }
            if (error) {
                if (!options.useAuthentication || options.username.isEmpty() || options.password.isEmpty()) {
                    *error = QObject::tr(
                        "WebDAV: the server requires authentication. Re-open the database from the WebDAV "
                        "menu and enter your remote username and password (enable \"Remember credentials\" "
                        "to keep them after locking the database).");
                } else {
                    *error = QObject::tr("WebDAV: the server rejected the credentials (HTTP %1).").arg(statusCode);
                }
            }
            reply->deleteLater();
            return false;
        }

        if (reply->error() != QNetworkReply::NoError
            || (statusCode >= 400 && statusCode != 204)) {
            if (error) {
                *error = reply->errorString();
            }
            reply->deleteLater();
            return false;
        }

        s_etagCache.remove(options.url);

        reply->deleteLater();
        return true;
    }

    return false;
}

void WebDavClient::clearETagCache()
{
    s_etagCache.clear();
}

QNetworkAccessManager* WebDavClient::networkManager()
{
    static auto* instance = new QNetworkAccessManager();
    return instance;
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

bool WebDavClient::waitForReply(QNetworkReply* reply, int timeoutMsec, QString* error)
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
