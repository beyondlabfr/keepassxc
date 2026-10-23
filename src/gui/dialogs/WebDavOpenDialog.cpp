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

#include "WebDavOpenDialog.h"
#include "ui_WebDavOpenDialog.h"

#include <QPushButton>
#include <QStandardItemModel>

#include "core/remote/WebDavClient.h"
#include "core/remote/WebDavCredentials.h"

using Remember = Database::RemoteFileConfig::Remember;

WebDavOpenDialog::WebDavOpenDialog(QWidget* parent)
    : QDialog(parent)
    , m_ui(new Ui::WebDavOpenDialog)
{
    m_ui->setupUi(this);

    m_ui->rememberComboBox->addItem(tr("Nothing"), static_cast<int>(Remember::Nothing));
    m_ui->rememberComboBox->addItem(tr("User name"), static_cast<int>(Remember::UserName));
    m_ui->rememberComboBox->addItem(tr("User name and password"), static_cast<int>(Remember::UserNameAndPassword));
    if (!WebDavCredentials::keyringAvailable()) {
        auto* model = qobject_cast<QStandardItemModel*>(m_ui->rememberComboBox->model());
        auto* item = model ? model->item(2) : nullptr;
        if (item) {
            item->setEnabled(false);
            item->setToolTip(tr("No system keyring available: the password can only be kept until KeePassXC "
                                "is closed."));
        }
    }

    m_ui->warningLabel->setVisible(false);

    connect(m_ui->urlLineEdit, &QLineEdit::textChanged, this, &WebDavOpenDialog::updateState);
    connect(m_ui->usernameLineEdit, &QLineEdit::textChanged, this, &WebDavOpenDialog::updateState);
    connect(m_ui->useAuthCheckBox, &QCheckBox::toggled, this, &WebDavOpenDialog::updateState);
    connect(m_ui->rememberComboBox, &QComboBox::currentIndexChanged, this, &WebDavOpenDialog::updateState);

    updateState();
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
    m_ui->rememberComboBox->setCurrentIndex(m_ui->rememberComboBox->findData(static_cast<int>(config.remember)));
    if (config.timeoutMsec > 0) {
        m_ui->timeoutSpinBox->setValue(config.timeoutMsec / 1000);
    }
    // Focus the first thing left to type
    if (config.url.isValid() && config.useAuthentication) {
        (config.username.isEmpty() ? m_ui->usernameLineEdit : m_ui->passwordLineEdit)->setFocus();
    }

    updateState();
}

void WebDavOpenDialog::accept()
{
    if (!updateRemoteConfig()) {
        updateState();
        return;
    }
    QDialog::accept();
}

void WebDavOpenDialog::updateState()
{
    const bool useAuth = m_ui->useAuthCheckBox->isChecked();
    m_ui->usernameLineEdit->setEnabled(useAuth);
    m_ui->passwordLineEdit->setEnabled(useAuth);

    // Without authentication there is no password to remember
    if (!useAuth && m_ui->rememberComboBox->currentData().toInt() == static_cast<int>(Remember::UserNameAndPassword)) {
        m_ui->rememberComboBox->setCurrentIndex(m_ui->rememberComboBox->findData(static_cast<int>(Remember::UserName)));
    }

    QUrl url(m_ui->urlLineEdit->text().trimmed());
    if (url.scheme().isEmpty()) {
        url.setScheme(QStringLiteral("https"));
    }
    bool valid = url.isValid() && !url.host().isEmpty() && WebDavClient::isWebDavScheme(url.scheme());
    if (valid && useAuth) {
        valid = !m_ui->usernameLineEdit->text().trimmed().isEmpty();
    }
    if (auto* okButton = m_ui->buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setEnabled(valid);
    }

    const auto httpUrl = WebDavClient::toHttpUrl(url);
    const bool cleartext = httpUrl.scheme().compare(QLatin1String("http"), Qt::CaseInsensitive) == 0;
    m_ui->warningLabel->setVisible(valid && useAuth && cleartext);
    m_ui->warningLabel->setText(
        tr("This address is not encrypted (http): the password will travel in clear text. Use https if the "
           "server supports it."));
}

bool WebDavOpenDialog::updateRemoteConfig()
{
    QUrl url(m_ui->urlLineEdit->text().trimmed());
    if (url.scheme().isEmpty()) {
        url.setScheme(QStringLiteral("https"));
    }
    if (!url.isValid() || !WebDavClient::isWebDavScheme(url.scheme())) {
        return false;
    }

    m_remoteConfig.type = Database::RemoteFileConfig::Type::WebDav;
    m_remoteConfig.url = QUrl(WebDavClient::normalizePath(url));
    m_remoteConfig.useAuthentication = m_ui->useAuthCheckBox->isChecked();
    m_remoteConfig.username = m_remoteConfig.useAuthentication ? m_ui->usernameLineEdit->text().trimmed() : QString();
    m_remoteConfig.password = m_remoteConfig.useAuthentication ? m_ui->passwordLineEdit->text() : QString();
    m_remoteConfig.timeoutMsec = m_ui->timeoutSpinBox->value() * 1000;
    m_remoteConfig.remember = static_cast<Remember>(m_ui->rememberComboBox->currentData().toInt());

    return true;
}
