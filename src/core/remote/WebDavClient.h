#ifndef KEEPASSXC_WEBDAVCLIENT_H
#define KEEPASSXC_WEBDAVCLIENT_H

#include <QByteArray>
#include <QHash>
#include <QUrl>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class WebDavClient
{
public:
    WebDavClient() = default;

    struct RequestOptions
    {
        QUrl url;
        QString username;
        QString password;
        int timeoutMsec = 30000;
        bool useAuthentication = false;
    };

    bool download(const RequestOptions& options, QByteArray& data, QString* error = nullptr);
    bool upload(const RequestOptions& options, const QByteArray& data, QString* error = nullptr);

    static bool isWebDavScheme(const QString& scheme);
    static QUrl toHttpUrl(const QUrl& original);
    static QUrl sanitizeUrl(const QUrl& url);
    static QString normalizePath(const QUrl& url);

    /** Set env KEEPASSXC_DEBUG_WEBDAV=1 to log WebDAV credential flow (no passwords). */
    static bool isDebugEnabled();
    static void debugLog(const QString& message);

    void clearETagCache();

private:
    struct ETagEntry
    {
        QByteArray etag;
        QByteArray data;
    };

    static QNetworkAccessManager* networkManager();
    static void applyBasicAuth(const RequestOptions& options, QNetworkRequest& request);
    static bool waitForReply(QNetworkReply* reply, int timeoutMsec, QString* error);

    static constexpr int kMaxRetries = 3;
    static constexpr int kBaseRetryDelayMs = 500;

    static QHash<QUrl, ETagEntry> s_etagCache;
};

#endif // KEEPASSXC_WEBDAVCLIENT_H
