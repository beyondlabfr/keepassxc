#include "gui/dialogs/WebDavOpenDialog.h"
#include "ui_WebDavOpenDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>

namespace
{
    bool isSupportedScheme(const QString& scheme)
    {
        return scheme.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
               || scheme.compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
               || scheme.compare(QStringLiteral("webdav"), Qt::CaseInsensitive) == 0
               || scheme.compare(QStringLiteral("webdavs"), Qt::CaseInsensitive) == 0;
    }

    QUrl sanitizeUrl(const QUrl& url)
    {
        QUrl copy(url);
        copy.setUserInfo(QString());
        return copy;
    }
} // namespace

WebDavOpenDialog::WebDavOpenDialog(QWidget* parent)
    : QDialog(parent)
    , m_ui(new Ui::WebDavOpenDialog)
{
    m_ui->setupUi(this);
    m_ui->timeoutSpinBox->setRange(5, 600);
    m_ui->timeoutSpinBox->setValue(30);

    connect(m_ui->urlLineEdit, &QLineEdit::textChanged, this, &WebDavOpenDialog::updateAcceptState);
    connect(m_ui->useAuthCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_ui->usernameLineEdit->setEnabled(checked);
        m_ui->passwordLineEdit->setEnabled(checked);
        m_ui->rememberCheckBox->setEnabled(checked);
        if (!checked) {
            m_ui->rememberCheckBox->setChecked(false);
        }
        updateAcceptState();
    });
    connect(m_ui->rememberCheckBox, &QCheckBox::toggled, this, &WebDavOpenDialog::updateAcceptState);

    m_ui->useAuthCheckBox->setChecked(false);
    m_ui->usernameLineEdit->setEnabled(false);
    m_ui->passwordLineEdit->setEnabled(false);
    m_ui->rememberCheckBox->setEnabled(false);
    updateAcceptState();
}

WebDavOpenDialog::~WebDavOpenDialog() = default;

Database::RemoteFileConfig WebDavOpenDialog::remoteConfig() const
{
    return m_remoteConfig;
}

void WebDavOpenDialog::setInitialConfig(const Database::RemoteFileConfig& config)
{
    m_remoteConfig = config;

    if (config.url.isValid()) {
        m_ui->urlLineEdit->setText(config.url.toString(QUrl::FullyEncoded));
    }

    m_ui->useAuthCheckBox->setChecked(config.useAuthentication);
    m_ui->usernameLineEdit->setText(config.username);
    m_ui->passwordLineEdit->setText(config.password);
    m_ui->rememberCheckBox->setChecked(config.useAuthentication && config.rememberCredentials);

    if (config.timeoutMsec > 0) {
        m_ui->timeoutSpinBox->setValue(config.timeoutMsec / 1000);
    }

    updateAcceptState();
}

void WebDavOpenDialog::accept()
{
    if (!updateRemoteConfig()) {
        updateAcceptState();
        return;
    }
    QDialog::accept();
}

void WebDavOpenDialog::updateAcceptState()
{
    const QUrl url(m_ui->urlLineEdit->text().trimmed());
    bool valid = url.isValid() && (isSupportedScheme(url.scheme()) || url.scheme().isEmpty());
    if (valid && m_ui->useAuthCheckBox->isChecked()) {
        valid = !m_ui->usernameLineEdit->text().trimmed().isEmpty();
    }
    auto* okButton = m_ui->buttonBox->button(QDialogButtonBox::Ok);
    if (okButton) {
        okButton->setEnabled(valid);
    }
}

bool WebDavOpenDialog::updateRemoteConfig()
{
    QUrl url(m_ui->urlLineEdit->text().trimmed());
    if (!url.isValid()) {
        return false;
    }

    if (url.scheme().isEmpty()) {
        url.setScheme(QStringLiteral("https"));
    }

    if (!isSupportedScheme(url.scheme())) {
        return false;
    }

    m_remoteConfig.type = Database::RemoteFileConfig::Type::WebDav;
    m_remoteConfig.url = sanitizeUrl(url);
    m_remoteConfig.useAuthentication = m_ui->useAuthCheckBox->isChecked();
    m_remoteConfig.username = m_remoteConfig.useAuthentication ? m_ui->usernameLineEdit->text().trimmed() : QString();
    m_remoteConfig.password = m_remoteConfig.useAuthentication ? m_ui->passwordLineEdit->text() : QString();
    m_remoteConfig.timeoutMsec = m_ui->timeoutSpinBox->value() * 1000;
    m_remoteConfig.rememberCredentials = m_remoteConfig.useAuthentication && m_ui->rememberCheckBox->isChecked();

    return true;
}

