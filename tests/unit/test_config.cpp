#include "TestFramework.h"
#include "DataDir.h"
#include "FakeWorld.h"
#include "SampleWorld.h"
#include "fw/Config.h"
#include "fw/Log.h"

using namespace fw;

namespace {

void smallWorld(FakeWorld& w)
{
	w.addFaction("100-hive", "Western Hive");
	w.addFaction("200-shek", "Shek Kingdom");
	w.addFaction("300-hn", "Holy Nation");
	w.addFaction("400-dup", "Bandits");
	w.addFaction("401-dup", "Bandits");
	w.addTown("t-hive", "Hive Village", "100-hive", 0, 0, 30);
	w.addTown("t-admag", "Admag", "200-shek", 1000, 0, 80, false, true);
	w.addTown("t-out", "Little Outpost", "300-hn", 2000, 0, 5, true);
	w.addSquad("s-hive-1", "Soldiers", "100-hive", 5);
	w.addSquad("s-shek-1", "Soldiers", "200-shek", 6);
	w.addSquad("s-shek-2", "Warband", "200-shek", 10);
}

bool hasMessage(const Log& log, LogLevel level, const std::string& part)
{
	for (size_t i = 0; i < log.entries().size(); ++i)
		if (log.entries()[i].level == level && log.entries()[i].message.find(part) != std::string::npos) return true;
	return false;
}

} // namespace

TEST(config_defaults_without_files)
{
	FakeWorld w;
	smallWorld(w);
	ConfigLoader loader;
	Config c;
	Log log;
	CHECK_EQ(loader.build(w, c, log), 0u);
	CHECK_EQ(c.factions.size(), 5u);
	CHECK_EQ(c.towns.size(), 3u);
	CHECK_EQ(c.squads.size(), 3u);
	CHECK_NEAR(c.squads["s-shek-2"].cost, 20, 1e-12);       // size * 2
	CHECK(c.townImportance("t-admag") > c.townImportance("t-out"));
	CHECK(c.townImportance("t-out") < 1);                   // outposts guess low
	CHECK_EQ(c.factions["100-hive"].warSlots, 2);
}

TEST(config_comments_and_trailing_commas)
{
	FakeWorld w;
	smallWorld(w);
	ConfigLoader loader;
	loader.addFile(LAYER_MAIN, "main/settings.json", "settings.json",
		"// comment\n{ \"temperature\": 2.0, /* block */ \"maxWarSquads\": 40, \"notifications\": \"all\", }\n");
	Config c;
	Log log;
	CHECK_EQ(loader.build(w, c, log), 0u);
	CHECK_NEAR(c.settings.temperature, 2.0, 0);
	CHECK_EQ(c.settings.maxWarSquads, 40);
	CHECK(c.settings.notifications == NOTIFY_ALL);
}

TEST(config_parse_error_skips_file_and_names_line)
{
	FakeWorld w;
	smallWorld(w);
	ConfigLoader loader;
	loader.addFile(LAYER_MAIN, "main/factions/a.json", "factions/a.json", "{ \"faction\": \"Western Hive\", \"wealth\": 300 }");
	loader.addFile(LAYER_ADDON, "addon/factions/b.json", "factions/b.json", "{\n  \"faction\": \"Western Hive\",\n  \"wealth\": 9 9\n}");
	Config c;
	Log log;
	CHECK_EQ(loader.build(w, c, log), 1u);
	CHECK(hasMessage(log, LOG_ERROR, "line 3"));
	CHECK_NEAR(c.factions["100-hive"].wealth, 300, 0);
}

TEST(config_merges_per_field)
{
	FakeWorld w;
	smallWorld(w);
	ConfigLoader loader;
	loader.addFile(LAYER_MAIN, "main", "factions/hive.json",
		"{ \"faction\": \"Western Hive\", \"wealth\": 100, \"character\": { \"hostility\": -60, \"opportunistic\": -50 } }");
	loader.addFile(LAYER_ADDON, "addon", "factions/patch.json",
		"{ \"faction\": \"100-hive\", \"character\": { \"hostility\": 20 } }");
	Config c;
	Log log;
	CHECK_EQ(loader.build(w, c, log), 0u);
	const FactionProfile& p = c.factions["100-hive"];
	CHECK_NEAR(p.character.hostility, 20, 0);
	CHECK_NEAR(p.character.opportunistic, -50, 0);
	CHECK_NEAR(p.wealth, 100, 0);
}

TEST(config_list_add_remove_and_replace)
{
	FakeWorld w;
	smallWorld(w);
	ConfigLoader loader;
	loader.addFile(LAYER_MAIN, "main", "factions/hive.json",
		"{ \"faction\": \"Western Hive\", \"likelyAllies\": [\"Shek Kingdom\"], \"likelyEnemies\": [\"Holy Nation\"] }");
	loader.addFile(LAYER_ADDON, "addon", "factions/patch.json",
		"{ \"faction\": \"Western Hive\", \"likelyAllies\": { \"add\": [\"300-hn\"], \"remove\": [\"200-shek\"] }, \"likelyEnemies\": [\"Shek Kingdom\"] }");
	Config c;
	Log log;
	loader.build(w, c, log);
	const FactionProfile& p = c.factions["100-hive"];
	CHECK_EQ(p.likelyAllies.size(), 1u);
	CHECK(contains(p.likelyAllies, std::string("300-hn")));
	CHECK_EQ(p.likelyEnemies.size(), 1u);
	CHECK(contains(p.likelyEnemies, std::string("200-shek")));
}

TEST(config_unknown_and_ambiguous_factions)
{
	FakeWorld w;
	smallWorld(w);
	ConfigLoader loader;
	loader.addFile(LAYER_ADDON, "addon", "factions/x.json",
		"[ { \"faction\": \"Not Installed\", \"wealth\": 5 }, { \"faction\": \"Bandits\", \"wealth\": 5 }, { \"faction\": \"401-dup\", \"wealth\": 7 } ]");
	Config c;
	Log log;
	CHECK_EQ(loader.build(w, c, log), 0u);
	CHECK(hasMessage(log, LOG_INFO, "Not Installed"));
	CHECK(hasMessage(log, LOG_WARNING, "more than one faction"));
	CHECK_NEAR(c.factions["400-dup"].wealth, 100, 0);
	CHECK_NEAR(c.factions["401-dup"].wealth, 7, 0);
}

TEST(config_settings_only_from_main_and_user)
{
	FakeWorld w;
	smallWorld(w);
	ConfigLoader loader;
	loader.addFile(LAYER_MAIN, "main", "settings.json", "{ \"temperature\": 1.5 }");
	loader.addFile(LAYER_ADDON, "addon", "settings.json", "{ \"temperature\": 9 }");
	loader.addFile(LAYER_USER, "user", "settings.json", "{ \"maxWarSquads\": 5 }");
	Config c;
	Log log;
	loader.build(w, c, log);
	CHECK_NEAR(c.settings.temperature, 1.5, 0);
	CHECK_EQ(c.settings.maxWarSquads, 5);
	CHECK(hasMessage(log, LOG_WARNING, "only read from the main mod"));
}

TEST(config_bad_values_are_clamped_or_defaulted)
{
	FakeWorld w;
	smallWorld(w);
	ConfigLoader loader;
	loader.addFile(LAYER_MAIN, "main", "factions/h.json",
		"{ \"faction\": \"Western Hive\", \"offenseGreed\": 3, \"wealth\": \"lots\", \"hostilty\": 5, \"character\": { \"dormancy\": -500 } }");
	Config c;
	Log log;
	CHECK_EQ(loader.build(w, c, log), 1u);   // the type error
	const FactionProfile& p = c.factions["100-hive"];
	CHECK_NEAR(p.offenseGreed, 1, 0);
	CHECK_NEAR(p.wealth, 100, 0);
	CHECK_NEAR(p.character.dormancy, -100, 0);
	CHECK(hasMessage(log, LOG_WARNING, "unknown field 'hostilty'"));
	CHECK(hasMessage(log, LOG_WARNING, "clamped"));
}

TEST(config_towns_and_squads)
{
	FakeWorld w;
	smallWorld(w);
	ConfigLoader loader;
	loader.addFile(LAYER_MAIN, "main", "towns/t.json", "{ \"towns\": { \"Admag\": 4.0, \"Hive Village\": { \"importance\": 2.3 }, \"Nowhere\": 1 } }");
	loader.addFile(LAYER_MAIN, "main", "squads/s.json",
		"{ \"faction\": \"Shek Kingdom\", \"squads\": { \"Soldiers\": 12, \"Warband\": { \"cost\": 30, \"war\": false } } }");
	loader.addFile(LAYER_ADDON, "addon", "squads/s2.json", "{ \"squads\": { \"Soldiers\": 3 } }");
	Config c;
	Log log;
	CHECK_EQ(loader.build(w, c, log), 0u);
	CHECK_NEAR(c.townImportance("t-admag"), 4.0, 0);
	CHECK_NEAR(c.townImportance("t-hive"), 2.3, 0);
	CHECK(hasMessage(log, LOG_INFO, "Nowhere"));
	CHECK_NEAR(c.squads["s-shek-1"].cost, 12, 0);           // "Soldiers" in the Shek file means Shek's
	CHECK_NEAR(c.squads["s-shek-2"].cost, 30, 0);
	CHECK(!c.squads["s-shek-2"].war);
	CHECK(hasMessage(log, LOG_WARNING, "more than one squad"));   // no faction given in s2.json
}

TEST(config_events_merge_by_id_and_remove)
{
	FakeWorld w;
	smallWorld(w);
	ConfigLoader loader;
	loader.addFile(LAYER_MAIN, "main", "events/e.json",
		"[ { \"id\": \"boom\", \"weight\": 5, \"durationDays\": 10 }, { \"id\": \"border_tension\", \"weight\": 3 } ]");
	loader.addFile(LAYER_ADDON, "addon", "events/e.json", "[ { \"id\": \"boom\", \"weight\": 8 }, { \"id\": \"border_tension\", \"remove\": true } ]");
	Config c;
	Log log;
	CHECK_EQ(loader.build(w, c, log), 0u);
	CHECK_EQ(c.events.size(), 1u);
	CHECK_NEAR(c.events["boom"].weight, 8, 0);
	CHECK(c.events["boom"].json.find("\"durationDays\":10") != std::string::npos);
}

TEST(config_sworn_lists_are_mutual_and_consistent)
{
	FakeWorld w;
	smallWorld(w);
	ConfigLoader loader;
	loader.addFile(LAYER_MAIN, "main", "factions/f.json",
		"[ { \"faction\": \"Western Hive\", \"swornAllies\": [\"Shek Kingdom\", \"Holy Nation\"], \"swornEnemies\": [\"Holy Nation\", \"Western Hive\"] } ]");
	Config c;
	Log log;
	loader.build(w, c, log);
	const FactionProfile& hive = c.factions["100-hive"];
	CHECK(contains(hive.swornAllies, std::string("200-shek")));
	CHECK(!contains(hive.swornAllies, std::string("300-hn")));     // enemy wins over ally
	CHECK(!contains(hive.swornEnemies, std::string("100-hive")));  // can't be own enemy
	CHECK(contains(c.factions["200-shek"].swornAllies, std::string("100-hive")));
	CHECK(contains(c.factions["300-hn"].swornEnemies, std::string("100-hive")));
}

TEST(config_reports_addon_conflicts)
{
	FakeWorld w;
	smallWorld(w);
	ConfigLoader loader;
	loader.addFile(LAYER_ADDON, "addonA", "factions/a.json", "{ \"faction\": \"Western Hive\", \"wealth\": 10 }");
	loader.addFile(LAYER_ADDON, "addonB", "factions/b.json", "{ \"faction\": \"Western Hive\", \"wealth\": 20 }");
	Config c;
	Log log;
	loader.build(w, c, log);
	CHECK_NEAR(c.factions["100-hive"].wealth, 20, 0);
	CHECK(hasMessage(log, LOG_INFO, "also set by addonA"));
}

TEST(config_shipped_data_loads_cleanly)
{
	FakeWorld w;
	buildSampleWorld(w, false);
	ConfigLoader loader;
	int files = addDataDir(loader, LAYER_MAIN, FW_DATA_DIR, "FactionWars/");
	CHECK(files >= 5);
	Config c;
	Log log;
	CHECK_EQ(loader.build(w, c, log), 0u);
	CHECK_EQ(log.count(LOG_WARNING), 0u);
	if (log.count(LOG_WARNING)) std::cout << log.format(LOG_WARNING);
	CHECK(c.factions["uc"].ignoreMeetRequirement);
	CHECK_NEAR(c.townImportance("heft"), 4.0, 0);
	// formulas.json mirrors the built-in defaults.
	for (int i = 0; i < F_COUNT; ++i)
		CHECK_EQ(c.formulas.text((FormulaId)i), std::string(Formulas::defaultText((FormulaId)i)));
	CHECK_EQ(c.events.size(), 2u);
}
