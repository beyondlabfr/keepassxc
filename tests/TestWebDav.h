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

#ifndef KEEPASSXC_TESTWEBDAV_H
#define KEEPASSXC_TESTWEBDAV_H

#include <QObject>
#include <QSharedPointer>
#include <QTemporaryFile>

#include "core/Database.h"

class CompositeKey;

/**
 * Runs against a real WebDAV server, see tests/webdav/ and scripts/test-webdav.sh.
 * Skipped when KPXC_TEST_WEBDAV_URL is not set.
 */
class TestWebDav : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    // Must run before anything else touches the credential store
    void testLegacyPasswordLeavesConfig();
    void testSaveAndOpen_data();
    void testSaveAndOpen();
    void testNoTemporaryFileLeft();
    void testRemoteChangeDetected_data();
    void testRemoteChangeDetected();
    void testStaleIfMatchRejected();
    void testCredentialsRequired();
    void testWrongPassword();
    void testRedirectReported();
    void testSaveAsLocalFile();
    void testBackupKeepsServer();

private:
    QString urlFor(const QString& area) const;
    Database::RemoteFileConfig remoteFor(const QString& url, const QString& password) const;
    QSharedPointer<Database> newDatabase(const QString& title) const;
    QSharedPointer<Database> openDatabase(const QString& url, QString* error = nullptr) const;

    QString m_baseUrl;
    QString m_username;
    QString m_password;
    QSharedPointer<CompositeKey> m_key;
    QTemporaryFile m_localConfig;
};

#endif // KEEPASSXC_TESTWEBDAV_H
