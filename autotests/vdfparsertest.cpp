// SPDX-FileCopyrightText: 2026 Thomas Kastenholz
// SPDX-License-Identifier: LGPL-2.1-or-later

// The text VDF parser GameLauncher reads Steam's libraryfolders.vdf and appmanifest_*.acf with.

#include <QTemporaryFile>
#include <QTest>

#include "helpers/VdfParser.h"

class VdfParserTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testAppManifest()
    {
        const QVariantMap result = VdfParser::parseString(QStringLiteral(R"("AppState"
{
	"appid"		"1623730"
	"Universe"		"1"
	"name"		"Palworld"
	"StateFlags"		"4"
	"installdir"		"Palworld"
	"InstalledDepots"
	{
		"1623731"
		{
			"manifest"		"123456789"
			"size"		"1234"
		}
	}
	"UserConfig"
	{
		"language"		"english"
	}
}
)"));
        const QVariantMap app = result.value(QStringLiteral("appstate")).toMap();
        QCOMPARE(app.value(QStringLiteral("appid")).toString(), QStringLiteral("1623730"));
        QCOMPARE(app.value(QStringLiteral("name")).toString(), QStringLiteral("Palworld"));
        // keys are case-insensitive (lowercased), values keep their case
        QCOMPARE(app.value(QStringLiteral("installdir")).toString(), QStringLiteral("Palworld"));
        QCOMPARE(app.value(QStringLiteral("stateflags")).toString(), QStringLiteral("4"));
        const QVariantMap depot = app.value(QStringLiteral("installeddepots")).toMap().value(QStringLiteral("1623731")).toMap();
        QCOMPARE(depot.value(QStringLiteral("manifest")).toString(), QStringLiteral("123456789"));
        // parsing carries on after a nested block closes
        QCOMPARE(app.value(QStringLiteral("userconfig")).toMap().value(QStringLiteral("language")).toString(), QStringLiteral("english"));
    }

    void testLibraryFolders()
    {
        const QVariantMap result = VdfParser::parseString(QStringLiteral(R"("libraryfolders"
{
	"0"
	{
		"path"		"/home/someone/.local/share/Steam"
		"label"		""
		"apps"
		{
			"228980"		"451573"
			"1623730"		"20145893051"
		}
	}
	"1"
	{
		"path"		"/mnt/games/Steam Library"
		"apps"
		{
		}
	}
}
)"));
        const QVariantMap folders = result.value(QStringLiteral("libraryfolders")).toMap();
        QCOMPARE(folders.size(), 2);
        const QVariantMap first = folders.value(QStringLiteral("0")).toMap();
        QCOMPARE(first.value(QStringLiteral("path")).toString(), QStringLiteral("/home/someone/.local/share/Steam"));
        QCOMPARE(first.value(QStringLiteral("label")).toString(), QString());
        QCOMPARE(first.value(QStringLiteral("apps")).toMap().keys(), QStringList({QStringLiteral("1623730"), QStringLiteral("228980")}));
        // a quoted value keeps its spaces
        QCOMPARE(folders.value(QStringLiteral("1")).toMap().value(QStringLiteral("path")).toString(), QStringLiteral("/mnt/games/Steam Library"));
        QVERIFY(folders.value(QStringLiteral("1")).toMap().value(QStringLiteral("apps")).toMap().isEmpty());
    }

    void testCommentsBlankLinesAndInlineBraces()
    {
        const QVariantMap result = VdfParser::parseString(QStringLiteral(R"(// written by hand
"root" {

	// a comment inside
	unquoted_key   unquoted_value
	"child" {
		"k" "v"
	}
	"after" "yes"
}
)"));
        const QVariantMap root = result.value(QStringLiteral("root")).toMap();
        QCOMPARE(root.value(QStringLiteral("unquoted_key")).toString(), QStringLiteral("unquoted_value"));
        QCOMPARE(root.value(QStringLiteral("child")).toMap().value(QStringLiteral("k")).toString(), QStringLiteral("v"));
        QCOMPARE(root.value(QStringLiteral("after")).toString(), QStringLiteral("yes"));
    }

    void testTruncatedInput()
    {
        // a half-written file must not hang or crash; what was read is kept
        const QVariantMap result = VdfParser::parseString(QStringLiteral("\"AppState\"\n{\n\t\"appid\"\t\"42\"\n\t\"Nested\"\n\t{\n\t\t\"a\""));
        QCOMPARE(result.value(QStringLiteral("appstate")).toMap().value(QStringLiteral("appid")).toString(), QStringLiteral("42"));
        QVERIFY(VdfParser::parseString(QString()).isEmpty());
        QVERIFY(VdfParser::parseString(QStringLiteral("}\n}\n")).isEmpty());
    }

    void testParseFile()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        file.write("\"AppState\"\n{\n\t\"name\"\t\"Caf\xc3\xa9 Game\"\n}\n");
        file.close();
        const QVariantMap result = VdfParser::parseFile(file.fileName());
        QCOMPARE(result.value(QStringLiteral("appstate")).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Café Game"));

        QVERIFY(VdfParser::parseFile(QStringLiteral("/nonexistent/appmanifest_1.acf")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(VdfParserTest)

#include "vdfparsertest.moc"
