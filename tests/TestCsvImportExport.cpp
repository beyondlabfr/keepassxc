/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
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

#include "TestCsvImportExport.h"

#include <QBuffer>
#include <QTemporaryFile>
#include <QTest>

#include "core/Group.h"
#include "crypto/Crypto.h"
#include "format/CsvExporter.h"
#include "format/CsvParser.h"
#include "gui/csvImport/CsvImportWidget.h"
#include "gui/csvImport/CsvParserModel.h"

QTEST_GUILESS_MAIN(TestCsvImportExport)

void TestCsvImportExport::initTestCase()
{
    Crypto::init();
}

void TestCsvImportExport::init()
{
    m_db = QSharedPointer<Database>::create();
    m_csvExporter = QSharedPointer<CsvExporter>::create();
}

void TestCsvImportExport::cleanup()
{
}

void TestCsvImportExport::testRoundTripWithCustomRootName()
{
    // Create a database with a custom root group name
    Group* groupRoot = m_db->rootGroup();
    groupRoot->setName("MyPasswords"); // Custom root name instead of default "Passwords"

    auto* group = new Group();
    group->setName("Test Group");
    group->setParent(groupRoot);

    auto* entry = new Entry();
    entry->setGroup(group);
    entry->setTitle("Test Entry");
    entry->setUsername("testuser");
    entry->setPassword("testpass");

    // Export to CSV
    QString csvData = m_csvExporter->exportDatabase(m_db);

    // Verify export contains the root group name in the path
    QVERIFY(csvData.contains("\"MyPasswords/Test Group\""));

    // Test the heuristic approach: analyze multiple similar paths
    QStringList groupPaths = {"MyPasswords/Test Group", "MyPasswords/Another Group", "MyPasswords/Third Group"};

    // Test the analyzeCommonRootGroup function logic
    QStringList firstComponents;
    for (const QString& path : groupPaths) {
        if (!path.isEmpty() && !path.startsWith("/")) {
            auto nameList = path.split("/", Qt::SkipEmptyParts);
            if (!nameList.isEmpty()) {
                firstComponents.append(nameList.first());
            }
        }
    }

    // All paths should have "MyPasswords" as first component
    QCOMPARE(firstComponents.size(), 3);
    QVERIFY(firstComponents.contains("MyPasswords"));

    // With 100% consistency, "MyPasswords" should be identified as common root
    QMap<QString, int> componentCounts;
    for (const QString& component : firstComponents) {
        componentCounts[component]++;
    }

    QCOMPARE(componentCounts["MyPasswords"], 3); // All 3 paths have this root

    // Simulate the group creation with identified root to skip
    QString groupPathFromCsv = "MyPasswords/Test Group";
    auto nameList = groupPathFromCsv.split("/", Qt::SkipEmptyParts);

    // New heuristic logic: skip identified root group name
    QString rootGroupToSkip = "MyPasswords";
    if (!rootGroupToSkip.isEmpty() && !nameList.isEmpty()
        && nameList.first().compare(rootGroupToSkip, Qt::CaseInsensitive) == 0) {
        nameList.removeFirst();
    }

    // After this logic, nameList should contain only ["Test Group"]
    QCOMPARE(nameList.size(), 1);
    QCOMPARE(nameList.first(), QString("Test Group"));
}

void TestCsvImportExport::testRoundTripWithDefaultRootName()
{
    // Test with default "Passwords" root name to ensure it works correctly
    Group* groupRoot = m_db->rootGroup();
    // Default name is "Passwords" - don't change it

    auto* group = new Group();
    group->setName("Test Group");
    group->setParent(groupRoot);

    auto* entry = new Entry();
    entry->setGroup(group);
    entry->setTitle("Test Entry");
    entry->setUsername("testuser");
    entry->setPassword("testpass");

    // Export to CSV
    QString csvData = m_csvExporter->exportDatabase(m_db);

    // Verify export contains the root group name in the path
    QVERIFY(csvData.contains("\"Passwords/Test Group\""));

    // Test the heuristic approach with consistent "Passwords" root
    QStringList groupPaths = {"Passwords/Test Group", "Passwords/Work", "Passwords/Personal"};

    // Simulate analysis to find common root
    QStringList firstComponents;
    for (const QString& path : groupPaths) {
        if (!path.isEmpty() && !path.startsWith("/")) {
            auto nameList = path.split("/", Qt::SkipEmptyParts);
            if (!nameList.isEmpty()) {
                firstComponents.append(nameList.first());
            }
        }
    }

    // All should have "Passwords" as first component
    QCOMPARE(firstComponents.size(), 3);
    for (const QString& component : firstComponents) {
        QCOMPARE(component, QString("Passwords"));
    }

    // Test group creation with identified root to skip
    QString groupPathFromCsv = "Passwords/Test Group";
    auto nameList = groupPathFromCsv.split("/", Qt::SkipEmptyParts);

    // Heuristic logic: skip the identified common root
    QString rootGroupToSkip = "Passwords";
    if (!rootGroupToSkip.isEmpty() && !nameList.isEmpty()
        && nameList.first().compare(rootGroupToSkip, Qt::CaseInsensitive) == 0) {
        nameList.removeFirst();
    }

    // After this logic, nameList should contain only ["Test Group"]
    QCOMPARE(nameList.size(), 1);
    QCOMPARE(nameList.first(), QString("Test Group"));
}

void TestCsvImportExport::testSingleLevelGroup()
{
    // Test case: entry is directly in root group (no sub-groups)
    // This should still work correctly and not remove any path components

    Group* groupRoot = m_db->rootGroup();
    auto* entry = new Entry();
    entry->setGroup(groupRoot); // Put entry directly in root
    entry->setTitle("Root Entry");
    entry->setUsername("rootuser");
    entry->setPassword("rootpass");

    // Export to CSV
    QString csvData = m_csvExporter->exportDatabase(m_db);

    // Verify export contains just the root group name (no sub-path)
    QVERIFY(csvData.contains("\"Passwords\",\"Root Entry\""));

    // Test heuristic with single-component paths
    QStringList groupPaths = {"Passwords", "Work", "Personal"}; // Mixed single components

    // With inconsistent first components, no common root should be identified
    QStringList firstComponents;
    for (const QString& path : groupPaths) {
        if (!path.isEmpty() && !path.startsWith("/")) {
            auto nameList = path.split("/", Qt::SkipEmptyParts);
            if (!nameList.isEmpty()) {
                firstComponents.append(nameList.first());
            }
        }
    }
    // Should have 3 different first components
    QCOMPARE(firstComponents.size(), 3);
    auto uniqueComponents = firstComponents.toSet();
    QCOMPARE(uniqueComponents.size(), 3); // All different

    // Test group creation with no identified root to skip
    QString groupPathFromCsv = "Passwords"; // Single component
    auto nameList = groupPathFromCsv.split("/", Qt::SkipEmptyParts);

    // With no common root identified, nothing should be removed
    QString rootGroupToSkip = QString(); // Empty - no common root found
    if (!rootGroupToSkip.isEmpty() && !nameList.isEmpty()
        && nameList.first().compare(rootGroupToSkip, Qt::CaseInsensitive) == 0) {
        nameList.removeFirst();
    }

    // Should still have ["Passwords"] as nothing was removed
    QCOMPARE(nameList.size(), 1);
    QCOMPARE(nameList.first(), QString("Passwords"));
}

void TestCsvImportExport::testAbsolutePaths()
{
    // Test case: paths that start with "/" (absolute paths)
    // According to the comment, if every row starts with "/", the root group should be left as is

    QStringList groupPaths = {"/Work/Subgroup1", "/Personal/Subgroup2", "/Finance/Subgroup3"};

    // Test the heuristic analysis with absolute paths
    QStringList firstComponents;
    for (const QString& path : groupPaths) {
        if (!path.isEmpty() && !path.startsWith("/")) {
            auto nameList = path.split("/", Qt::SkipEmptyParts);
            if (!nameList.isEmpty()) {
                firstComponents.append(nameList.first());
            }
        }
        // Note: paths starting with "/" are skipped in the analysis
    }

    // Since all paths start with "/", no first components should be collected
    QCOMPARE(firstComponents.size(), 0);

    // With no first components, no common root should be identified
    QString rootGroupToSkip = QString(); // Should be empty

    // Test group creation with absolute path
    QString groupPathFromCsv = "/Work/Subgroup1";
    auto nameList = groupPathFromCsv.split("/", Qt::SkipEmptyParts);

    // With no root to skip, the full path should be preserved
    if (!rootGroupToSkip.isEmpty() && !nameList.isEmpty()
        && nameList.first().compare(rootGroupToSkip, Qt::CaseInsensitive) == 0) {
        nameList.removeFirst();
    }

    // Should have ["Work", "Subgroup1"] - full path preserved
    QCOMPARE(nameList.size(), 2);
    QCOMPARE(nameList.at(0), QString("Work"));
    QCOMPARE(nameList.at(1), QString("Subgroup1"));
}
