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

#include "WebDavCredentials.h"

#include "core/Config.h"
#include "core/remote/WebDavClient.h"

#include <QEventLoop>
#include <QVariantHash>

#ifdef KPXC_HAVE_QTKEYCHAIN
#include <qt6keychain/keychain.h>
#endif

namespace
{
    const QString kKeyringService = QStringLiteral("KeePassXC WebDAV");

    const QString kUsername = QStringLiteral("username");
    const QString kUseAuthentication = QStringLiteral("useAuthentication");
    const QString kTimeout = QStringLiteral("timeout");
    const QString kRemember = QStringLiteral("remember");
    // Written in clear by builds before the keyring support; only read to migrate it away
    const QString kLegacyPassword = QStringLiteral("password");

    QString keyFor(const QUrl& url)
    {
        return WebDavClient::normalizePath(url);
    }

#ifdef KPXC_HAVE_QTKEYCHAIN
    bool runJob(QKeychain::Job& job)
    {
        job.setAutoDelete(false);
        // Never let QtKeychain fall back to storing the secret in clear in a settings file
        job.setInsecureFallback(false);
        QEventLoop loop;
        QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
        job.start();
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        return job.error() == QKeychain::NoError;
    }
#endif
} // namespace

WebDavCredentials* WebDavCredentials::instance()
{
    static WebDavCredentials credentials;
    return &credentials;
}

bool WebDavCredentials::keyringAvailable()
{
#ifdef KPXC_HAVE_QTKEYCHAIN
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // With Secret Service integration on, KeePassXC may be the keyring itself: reading the
    // WebDAV password would ask the very database we are trying to open
    if (config()->get(Config::FdoSecrets_Enabled).toBool()) {
        return false;
    }
#endif
    return QKeychain::isAvailable();
#else
    return false;
#endif
}

bool WebDavCredentials::lookup(const QUrl& url, Database::RemoteFileConfig& config)
{
    const auto key = keyFor(url);
    auto session = m_session.constFind(key);
    if (session != m_session.constEnd()) {
        config = session.value();
        return true;
    }

    loadSettings();
    auto persisted = m_persisted.constFind(key);
    if (persisted == m_persisted.constEnd()) {
        return false;
    }
    config = persisted.value();
    if (config.useAuthentication && config.remember == Database::RemoteFileConfig::Remember::UserNameAndPassword) {
        config.password = readPassword(key);
    }
    m_session.insert(key, config);
    return true;
}

bool WebDavCredentials::store(const Database::RemoteFileConfig& config)
{
    using Remember = Database::RemoteFileConfig::Remember;

    const auto key = keyFor(config.url);
    m_session.insert(key, config);

    loadSettings();
    const bool hadPassword = m_persisted.value(key).remember == Remember::UserNameAndPassword;
    if (config.remember == Remember::Nothing) {
        if (m_persisted.remove(key) > 0) {
            if (hadPassword) {
                deletePassword(key);
            }
            saveSettings();
        }
        return true;
    }

    bool stored = true;
    auto persisted = config;
    persisted.password.clear();
    if (config.remember == Remember::UserNameAndPassword && config.useAuthentication) {
        if (!writePassword(key, config.password)) {
            persisted.remember = Remember::UserName;
            stored = false;
        }
    } else if (hadPassword) {
        deletePassword(key);
    }
    m_persisted.insert(key, persisted);
    saveSettings();
    return stored;
}

void WebDavCredentials::forget(const QUrl& url)
{
    const auto key = keyFor(url);
    m_session.remove(key);
    loadSettings();
    const bool hadPassword =
        m_persisted.value(key).remember == Database::RemoteFileConfig::Remember::UserNameAndPassword;
    if (m_persisted.remove(key) > 0) {
        if (hadPassword) {
            deletePassword(key);
        }
        saveSettings();
    }
}

void WebDavCredentials::forgetSessionPassword(const QUrl& url)
{
    auto it = m_session.find(keyFor(url));
    if (it != m_session.end()) {
        it->password.clear();
    }
}

void WebDavCredentials::migrateLegacySettings()
{
    loadSettings();
}

void WebDavCredentials::loadSettings()
{
    using Remember = Database::RemoteFileConfig::Remember;

    if (m_loaded) {
        return;
    }
    m_loaded = true;

    bool migrated = false;
    const auto stored = config()->get(Config::LastWebDavCredentials).toHash();
    for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
        const auto entry = it.value().toHash();

        Database::RemoteFileConfig remote;
        remote.type = Database::RemoteFileConfig::Type::WebDav;
        remote.url = QUrl(it.key());
        remote.username = entry.value(kUsername).toString();
        remote.useAuthentication = entry.value(kUseAuthentication, false).toBool();
        remote.timeoutMsec = entry.value(kTimeout, 30000).toInt();
        // Older builds stored a boolean "remember" together with the password in clear
        const auto rememberValue = entry.value(kRemember);
        if (rememberValue.typeId() == QMetaType::Bool) {
            remote.remember = rememberValue.toBool() ? Remember::UserNameAndPassword : Remember::Nothing;
            migrated = true;
        } else {
            remote.remember = static_cast<Remember>(qBound(0, rememberValue.toInt(), 2));
        }

        if (entry.contains(kLegacyPassword)) {
            migrated = true;
            const auto password = entry.value(kLegacyPassword).toString();
            if (remote.remember == Remember::UserNameAndPassword
                && (password.isEmpty() || !writePassword(it.key(), password))) {
                remote.remember = Remember::UserName;
            }
        }

        if (remote.url.isValid() && remote.remember != Remember::Nothing) {
            m_persisted.insert(it.key(), remote);
        }
    }

    if (migrated) {
        saveSettings();
    }
}

void WebDavCredentials::saveSettings() const
{
    QVariantHash stored;
    for (auto it = m_persisted.constBegin(); it != m_persisted.constEnd(); ++it) {
        QVariantHash entry;
        entry.insert(kUsername, it.value().username);
        entry.insert(kUseAuthentication, it.value().useAuthentication);
        entry.insert(kTimeout, it.value().timeoutMsec);
        entry.insert(kRemember, static_cast<int>(it.value().remember));
        stored.insert(it.key(), entry);
    }

    if (stored.isEmpty()) {
        config()->remove(Config::LastWebDavCredentials);
    } else {
        config()->set(Config::LastWebDavCredentials, stored);
    }
}

QString WebDavCredentials::readPassword(const QString& key)
{
#ifdef KPXC_HAVE_QTKEYCHAIN
    if (keyringAvailable()) {
        QKeychain::ReadPasswordJob job(kKeyringService);
        job.setKey(key);
        if (runJob(job)) {
            return job.textData();
        }
    }
#else
    Q_UNUSED(key);
#endif
    return {};
}

bool WebDavCredentials::writePassword(const QString& key, const QString& password)
{
#ifdef KPXC_HAVE_QTKEYCHAIN
    if (keyringAvailable()) {
        QKeychain::WritePasswordJob job(kKeyringService);
        job.setKey(key);
        job.setTextData(password);
        return runJob(job);
    }
#else
    Q_UNUSED(key);
    Q_UNUSED(password);
#endif
    return false;
}

void WebDavCredentials::deletePassword(const QString& key)
{
#ifdef KPXC_HAVE_QTKEYCHAIN
    if (keyringAvailable()) {
        QKeychain::DeletePasswordJob job(kKeyringService);
        job.setKey(key);
        runJob(job);
    }
#else
    Q_UNUSED(key);
#endif
}
