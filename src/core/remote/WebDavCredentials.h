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

#ifndef KEEPASSXC_WEBDAVCREDENTIALS_H
#define KEEPASSXC_WEBDAVCREDENTIALS_H

#include <QCoreApplication>
#include <QHash>

#include "core/Database.h"

/**
 * Single place where WebDAV connection settings are remembered, like KeePass 2 does for URLs:
 * - for the whole session, in memory, whatever the remember mode (lock/unlock and reload need them);
 * - across sessions, in the local config without the password, and the password in the system
 *   keyring when the user asked to remember it.
 * Passwords are never written to the config file; ones left there by older builds are moved to the
 * keyring (or dropped if there is none) on first load.
 */
class WebDavCredentials
{
    Q_DECLARE_TR_FUNCTIONS(WebDavCredentials)

public:
    static WebDavCredentials* instance();

    // Fills the connection settings known for this URL; false if nothing is known
    bool lookup(const QUrl& url, Database::RemoteFileConfig& config);
    // False when the password was meant to be remembered but the keyring refused it
    bool store(const Database::RemoteFileConfig& config);
    void forget(const QUrl& url);
    // After the server rejected it: ask again next time, keyring left as is until the user enters a new one
    void forgetSessionPassword(const QUrl& url);
    // Moves passwords left in clear in the config by older builds; cheap when there are none
    void migrateLegacySettings();

    // True when a keyring backend is installed (it may still be locked or not running)
    static bool keyringAvailable();

private:
    WebDavCredentials() = default;

    void loadSettings();
    void saveSettings() const;

    static QString readPassword(const QString& key);
    static bool writePassword(const QString& key, const QString& password);
    static void deletePassword(const QString& key);

    QHash<QString, Database::RemoteFileConfig> m_session;
    QHash<QString, Database::RemoteFileConfig> m_persisted;
    bool m_loaded = false;
};

#endif // KEEPASSXC_WEBDAVCREDENTIALS_H
