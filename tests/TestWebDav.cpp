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

#include "TestWebDav.h"

#include <QSignalSpy>
#include <QTest>
#include <QUuid>

#include "core/Config.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/remote/WebDavClient.h"
#include "core/remote/WebDavCredentials.h"
#include "crypto/Crypto.h"
#include "keys/PasswordKey.h"
#include "util/TemporaryFile.h"

QTEST_GUILESS_MAIN(TestWebDav)

using Remember = Database::RemoteFileConfig::Remember;

/*
 * Areas served by tests/webdav/httpd.conf:
 *   dav/       regular WebDAV share with strong ETags
 *   noetag/    no ETag at all: changes are detected with the hash of the first block
 *   nomove/    MOVE refused: saves fall back to a direct upload
 *   gzip/      responses compressed with gzip
 *   redirect/  permanent redirect to dav/
 */

void TestWebDav::initTestCase()
{
    m_baseUrl = qEnvironmentVariable("KPXC_TEST_WEBDAV_URL");
    if (m_baseUrl.isEmpty()) {
        QSKIP("KPXC_TEST_WEBDAV_URL is not set, see scripts/test-webdav.sh");
    }
    if (!m_baseUrl.endsWith('/')) {
        m_baseUrl += '/';
    }
    m_username = qEnvironmentVariable("KPXC_TEST_WEBDAV_USER");
    m_password = qEnvironmentVariable("KPXC_TEST_WEBDAV_PASSWORD");

    QVERIFY(Crypto::init());
    QVERIFY(m_localConfig.open());
    Config::createConfigFromFile(TemporaryFile::createTempConfigFile(), m_localConfig.fileName());

    m_key = QSharedPointer<CompositeKey>::create();
    m_key->addKey(QSharedPointer<PasswordKey>::create("a"));
}

QString TestWebDav::urlFor(const QString& area) const
{
    return QStringLiteral("%1%2/%3.kdbx").arg(m_baseUrl, area, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

Database::RemoteFileConfig TestWebDav::remoteFor(const QString& url, const QString& password) const
{
    Database::RemoteFileConfig remote;
    remote.type = Database::RemoteFileConfig::Type::WebDav;
    remote.url = QUrl(url);
    remote.useAuthentication = true;
    remote.username = m_username;
    remote.password = password;
    remote.timeoutMsec = 10000;
    remote.remember = Remember::Nothing;
    return remote;
}

QSharedPointer<Database> TestWebDav::newDatabase(const QString& title) const
{
    auto db = QSharedPointer<Database>::create();
    db->setKey(m_key);
    auto* entry = new Entry();
    entry->setUuid(QUuid::createUuid());
    entry->setTitle(title);
    entry->setGroup(db->rootGroup());
    return db;
}

QSharedPointer<Database> TestWebDav::openDatabase(const QString& url, QString* error) const
{
    auto db = QSharedPointer<Database>::create();
    QString localError;
    if (!db->open(url, m_key, error ? error : &localError)) {
        return {};
    }
    return db;
}

static QStringList titles(const QSharedPointer<Database>& db)
{
    QStringList result;
    for (const auto* entry : db->rootGroup()->entries()) {
        result << entry->title();
    }
    result.sort();
    return result;
}

void TestWebDav::testLegacyPasswordLeavesConfig()
{
    // Older builds kept the password in clear in the local config
    const auto url = urlFor("dav");
    const auto key = WebDavClient::normalizePath(QUrl(url));
    QVariantHash entry;
    entry.insert("username", m_username);
    entry.insert("password", m_password);
    entry.insert("useAuthentication", true);
    entry.insert("remember", true);
    entry.insert("timeout", 30000);
    QVariantHash stored;
    stored.insert(key, entry);
    config()->set(Config::LastWebDavCredentials, stored);

    Database::RemoteFileConfig remote;
    QVERIFY(WebDavCredentials::instance()->lookup(QUrl(url), remote));
    QCOMPARE(remote.username, m_username);

    const auto migrated = config()->get(Config::LastWebDavCredentials).toHash().value(key).toHash();
    QVERIFY(!migrated.isEmpty());
    QVERIFY(!migrated.contains("password"));
    QCOMPARE(migrated.value("username").toString(), m_username);
    // isAvailable() only tells whether a keyring library is installed, so judge on the outcome:
    // either the password went to the keyring, or it was dropped and only the user name is kept
    const auto remember = static_cast<Remember>(migrated.value("remember").toInt());
    if (remember == Remember::UserNameAndPassword) {
        QCOMPARE(remote.password, m_password);
    } else {
        QCOMPARE(remember, Remember::UserName);
        QVERIFY(remote.password.isEmpty());
    }
    // Set when the test runs with a Secret Service, to make sure the keyring path is the one tested
    if (qEnvironmentVariableIsSet("KPXC_TEST_WEBDAV_EXPECT_KEYRING")) {
        QCOMPARE(remember, Remember::UserNameAndPassword);
    }

    WebDavCredentials::instance()->forget(QUrl(url));
    QVERIFY(config()->get(Config::LastWebDavCredentials).toHash().isEmpty());
}

void TestWebDav::testSaveAndOpen_data()
{
    QTest::addColumn<QString>("area");
    QTest::addColumn<int>("action");
    QTest::newRow("strong ETag") << "dav" << int(Database::Atomic);
    QTest::newRow("strong ETag, direct write") << "dav" << int(Database::DirectWrite);
    QTest::newRow("no ETag") << "noetag" << int(Database::Atomic);
    QTest::newRow("MOVE refused") << "nomove" << int(Database::Atomic);
    QTest::newRow("gzip") << "gzip" << int(Database::Atomic);
    QTest::newRow("gzip, direct write") << "gzip" << int(Database::DirectWrite);
}

void TestWebDav::testSaveAndOpen()
{
    QFETCH(QString, area);
    QFETCH(int, action);
    const auto saveAction = static_cast<Database::SaveAction>(action);
    const auto url = urlFor(area);
    WebDavCredentials::instance()->store(remoteFor(url, m_password));

    auto db = newDatabase("first");
    QString error;
    QVERIFY2(db->saveAs(url, saveAction, {}, &error), qPrintable(error));
    QVERIFY(db->hasRemoteFile());
    QVERIFY(!db->isModified());

    auto reopened = openDatabase(url, &error);
    QVERIFY2(reopened, qPrintable(error));
    QCOMPARE(titles(reopened), QStringList({"first"}));

    // A second save of the same database must not be mistaken for a change made by someone else
    auto* entry = new Entry();
    entry->setUuid(QUuid::createUuid());
    entry->setTitle("second");
    entry->setGroup(db->rootGroup());
    QVERIFY2(db->save(saveAction, {}, &error), qPrintable(error));

    reopened = openDatabase(url, &error);
    QVERIFY2(reopened, qPrintable(error));
    QCOMPARE(titles(reopened), QStringList({"first", "second"}));
}

void TestWebDav::testNoTemporaryFileLeft()
{
    const auto url = urlFor("dav");
    WebDavCredentials::instance()->store(remoteFor(url, m_password));

    auto db = newDatabase("first");
    QString error;
    QVERIFY2(db->saveAs(url, Database::Atomic, {}, &error), qPrintable(error));
    QVERIFY2(db->save(Database::Atomic, {}, &error), qPrintable(error));

    // The share has an index page listing its files
    WebDavClient client;
    WebDavClient::RequestOptions options;
    options.url = QUrl(m_baseUrl + "dav/");
    options.useAuthentication = true;
    options.username = m_username;
    options.password = m_password;
    const auto listing = client.get(options);
    QVERIFY2(listing.ok(), qPrintable(listing.error));
    QVERIFY(listing.body.contains(QUrl(url).fileName().toUtf8()));
    QVERIFY(!listing.body.contains(".tmp"));
}

void TestWebDav::testRemoteChangeDetected_data()
{
    QTest::addColumn<QString>("area");
    QTest::newRow("strong ETag") << "dav";
    QTest::newRow("no ETag") << "noetag";
    QTest::newRow("MOVE refused") << "nomove";
}

void TestWebDav::testRemoteChangeDetected()
{
    QFETCH(QString, area);
    const auto url = urlFor(area);
    WebDavCredentials::instance()->store(remoteFor(url, m_password));

    QString error;
    auto mine = newDatabase("shared");
    QVERIFY2(mine->saveAs(url, Database::Atomic, {}, &error), qPrintable(error));

    // Someone else saves a change
    auto theirs = openDatabase(url, &error);
    QVERIFY2(theirs, qPrintable(error));
    theirs->metadata()->setName("theirs");
    QVERIFY2(theirs->save(Database::Atomic, {}, &error), qPrintable(error));

    // Our save must stop and hand over to the merge prompt, like for a local file
    mine->metadata()->setName("mine");
    QSignalSpy spyFileChanged(mine.data(), &Database::databaseFileChanged);
    QVERIFY(!mine->save(Database::Atomic, {}, &error));
    QVERIFY(error.contains("unmerged"));
    QTRY_COMPARE(spyFileChanged.count(), 1);
    QCOMPARE(spyFileChanged.at(0).at(0).toBool(), true);
    QVERIFY(mine->isModified());

    auto onServer = openDatabase(url, &error);
    QVERIFY2(onServer, qPrintable(error));
    QCOMPARE(onServer->metadata()->name(), QString("theirs"));

    // "Overwrite" in the prompt
    mine->setIgnoreFileChangesUntilSaved(true);
    QVERIFY2(mine->save(Database::Atomic, {}, &error), qPrintable(error));
    QVERIFY(!mine->ignoreFileChangesUntilSaved());

    onServer = openDatabase(url, &error);
    QVERIFY2(onServer, qPrintable(error));
    QCOMPARE(onServer->metadata()->name(), QString("mine"));

    // And the next save goes through without prompting again
    mine->metadata()->setName("mine again");
    QVERIFY2(mine->save(Database::Atomic, {}, &error), qPrintable(error));
}

void TestWebDav::testStaleIfMatchRejected()
{
    // Covers the race between the change check and a direct upload
    const auto url = urlFor("dav");
    WebDavClient client;
    WebDavClient::RequestOptions options;
    options.url = QUrl(url);
    options.useAuthentication = true;
    options.username = m_username;
    options.password = m_password;

    auto first = client.put(options, "first");
    QVERIFY2(first.ok(), qPrintable(first.error));
    // Apache only gives a strong ETag once the file is more than a second old
    QTest::qWait(1100);
    const auto etag = client.head(options).etag;
    QVERIFY(WebDavClient::isStrongETag(etag));

    QVERIFY(client.put(options, "second").ok());
    const auto stale = client.put(options, "third", etag);
    QCOMPARE(stale.status, 412);
    QCOMPARE(client.get(options).body, QByteArray("second"));
}

void TestWebDav::testCredentialsRequired()
{
    const auto url = urlFor("dav");
    WebDavCredentials::instance()->forget(QUrl(url));

    QString error;
    QVERIFY(!openDatabase(url, &error));
    QVERIFY2(error.contains("requires a user name"), qPrintable(error));
}

void TestWebDav::testWrongPassword()
{
    const auto url = urlFor("dav");
    WebDavCredentials::instance()->store(remoteFor(url, "not the password"));

    QString error;
    QVERIFY(!openDatabase(url, &error));
    QVERIFY2(error.contains("rejected"), qPrintable(error));
    WebDavCredentials::instance()->forget(QUrl(url));
}

void TestWebDav::testRedirectReported()
{
    const auto url = urlFor("redirect");
    WebDavCredentials::instance()->store(remoteFor(url, m_password));

    QString error;
    QVERIFY(!openDatabase(url, &error));
    QVERIFY2(error.contains("redirects"), qPrintable(error));

    // A save must not be reported as successful either
    auto db = newDatabase("lost?");
    QVERIFY(!db->saveAs(url, Database::DirectWrite, {}, &error));
    QVERIFY2(error.contains("redirects"), qPrintable(error));
}

void TestWebDav::testSaveAsLocalFile()
{
    const auto url = urlFor("dav");
    WebDavCredentials::instance()->store(remoteFor(url, m_password));

    QString error;
    auto db = newDatabase("local copy");
    QVERIFY2(db->saveAs(url, Database::Atomic, {}, &error), qPrintable(error));

    TemporaryFile localFile;
    QVERIFY(localFile.open());
    localFile.close();
    QVERIFY2(db->saveAs(localFile.fileName(), Database::Atomic, {}, &error), qPrintable(error));
    QVERIFY(!db->hasRemoteFile());
    QCOMPARE(db->filePath(), localFile.fileName());
}

void TestWebDav::testBackupKeepsServer()
{
    // Without ETag the change check relies on the hash of the first block, which a backup must not alter
    const auto url = urlFor("noetag");
    WebDavCredentials::instance()->store(remoteFor(url, m_password));

    QString error;
    auto db = newDatabase("before backup");
    QVERIFY2(db->saveAs(url, Database::Atomic, {}, &error), qPrintable(error));

    TemporaryFile backupFile;
    QVERIFY(backupFile.open());
    backupFile.close();
    QVERIFY2(db->saveCopy(backupFile.fileName(), &error), qPrintable(error));
    QVERIFY(db->hasRemoteFile());
    QCOMPARE(db->filePath(), WebDavClient::normalizePath(QUrl(url)));

    auto backup = QSharedPointer<Database>::create();
    QVERIFY2(backup->open(backupFile.fileName(), m_key, &error), qPrintable(error));
    QCOMPARE(titles(backup), QStringList({"before backup"}));

    // The next save still goes to the server, without a false "modified on the server"
    db->metadata()->setName("after backup");
    QVERIFY2(db->save(Database::Atomic, {}, &error), qPrintable(error));
    auto onServer = openDatabase(url, &error);
    QVERIFY2(onServer, qPrintable(error));
    QCOMPARE(onServer->metadata()->name(), QString("after backup"));
}

static WebDavClient::RequestOptions optionsFor(const QString& url, const QString& user, const QString& password)
{
    WebDavClient::RequestOptions options;
    options.url = QUrl(url);
    options.useAuthentication = true;
    options.username = user;
    options.password = password;
    return options;
}

void TestWebDav::testUnchangedContentIsNoConflict()
{
    // A new ETag with the same content (touch, restore, server-side copy) must not block saving:
    // the reload that would follow sees no change and would never prompt
    const auto url = urlFor("dav");
    WebDavCredentials::instance()->store(remoteFor(url, m_password));

    QString error;
    auto db = newDatabase("touched");
    QVERIFY2(db->saveAs(url, Database::Atomic, {}, &error), qPrintable(error));

    WebDavClient client;
    const auto options = optionsFor(url, m_username, m_password);
    const auto content = client.get(options);
    QVERIFY(content.ok());
    QTest::qWait(1100);
    QVERIFY(client.put(options, content.body).ok());

    db->metadata()->setName("after touch");
    QVERIFY2(db->save(Database::Atomic, {}, &error), qPrintable(error));
}

void TestWebDav::testTruncatedServerFileOverwritten()
{
    // Not a database any more: overwrite it, as for a local file, instead of a conflict nobody can resolve
    const auto url = urlFor("noetag");
    WebDavCredentials::instance()->store(remoteFor(url, m_password));

    QString error;
    auto db = newDatabase("repaired");
    QVERIFY2(db->saveAs(url, Database::Atomic, {}, &error), qPrintable(error));

    WebDavClient client;
    QVERIFY(client.put(optionsFor(url, m_username, m_password), "x").ok());

    db->metadata()->setName("repaired");
    QVERIFY2(db->save(Database::Atomic, {}, &error), qPrintable(error));
    auto onServer = openDatabase(url, &error);
    QVERIFY2(onServer, qPrintable(error));
    QCOMPARE(onServer->metadata()->name(), QString("repaired"));
}

void TestWebDav::testRejectedPasswordAskedAgain()
{
    const auto url = urlFor("dav");
    auto remote = remoteFor(url, "not the password");
    remote.remember = Remember::UserName;
    WebDavCredentials::instance()->store(remote);

    QString error;
    QVERIFY(!openDatabase(url, &error));

    // The user name stays, the rejected password is gone so that the dialog shows up again
    Database::RemoteFileConfig known;
    QVERIFY(WebDavCredentials::instance()->lookup(QUrl(url), known));
    QCOMPARE(known.username, m_username);
    QVERIFY(known.password.isEmpty());

    WebDavCredentials::instance()->forget(QUrl(url));
}
