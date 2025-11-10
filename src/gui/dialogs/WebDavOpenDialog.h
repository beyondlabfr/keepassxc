#ifndef KEEPASSX_WEBDAVOPENDIALOG_H
#define KEEPASSX_WEBDAVOPENDIALOG_H

#include <QDialog>
#include <QScopedPointer>
#include <QUrl>

#include "core/Database.h"

namespace Ui
{
class WebDavOpenDialog;
}

class WebDavOpenDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WebDavOpenDialog(QWidget* parent = nullptr);
    ~WebDavOpenDialog() override;

    Database::RemoteFileConfig remoteConfig() const;
    void setInitialConfig(const Database::RemoteFileConfig& config);

public slots:
    void accept() override;

private slots:
    void updateAcceptState();

private:
    bool updateRemoteConfig();

    QScopedPointer<Ui::WebDavOpenDialog> m_ui;
    Database::RemoteFileConfig m_remoteConfig;
};

#endif // KEEPASSX_WEBDAVOPENDIALOG_H

