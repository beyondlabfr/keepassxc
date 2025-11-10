#ifndef KEEPASSXC_WEBDAVCLIENT_H
#define KEEPASSXC_WEBDAVCLIENT_H

#include <QByteArray>
#include <QUrl>
#include <QString>

class QNetworkReply;
class QNetworkRequest;

class QString;

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

    bool download(const RequestOptions& options, QByteArray& data, QString* error = nullptr) const;
    bool upload(const RequestOptions& options, const QByteArray& data, QString* error = nullptr) const;

private:
    static QUrl effectiveUrl(const QUrl& original);
    static void applyBasicAuth(const RequestOptions& options, QNetworkRequest& request);
    static bool waitForFinished(QNetworkReply* reply, int timeoutMsec, QString* error);
};

#endif // KEEPASSXC_WEBDAVCLIENT_H

