#include "TestFramework.h"
#include "FakeWorld.h"
#include "SampleWorld.h"
#include "fw/Config.h"
#include "fw/Simulation.h"

using namespace fw;

namespace {

// Three hostile neighbours and a peaceful fourth faction.
void testWorld(FakeWorld& w)
{
	w.addFaction("a", "Alpha");
	w.addFaction("b", "Beta");
	w.addFaction("c", "Gamma");
	w.addFaction("d", "Delta");
	w.addTown("a1", "Alpha Town", "a", 0, 0, 40, false, true);
	w.addTown("a2", "Alpha Post", "a", 3000, 0, 10, true);
	w.addTown("b1", "Beta Town", "b", 6000, 0, 40, false, true);
	w.addTown("c1", "Gamma Town", "c", 0, 6000, 40, false, true);
	w.addTown("d1", "Delta Town", "d", 20000, 20000, 40, false, true);
	const char* ids[] = { "a", "b", "c", "d" };
	for (int i = 0; i < 4; ++i)
	{
		std::string f = ids[i];
		w.addSquad(f + "_s", f + " small", f, 3);
		w.addSquad(f + "_m", f + " medium", f, 6);
		w.addSquad(f + "_l", f + " large", f, 10);
	}
	w.setRelation("a", "b", -90);
	w.setRelation("a", "c", -90);
	w.setRelation("b", "c", -90);
	w.meetAll();
}

const char* kHostileProfiles =
	"[ { \"faction\": \"a\", \"wealth\": 500, \"maxWealth\": 1000, \"wealthGrowth\": 10, \"offenseGreed\": 0.5, \"character\": { \"hostility\": 80, \"dormancy\": 80 } },"
	"  { \"faction\": \"b\", \"wealth\": 500, \"maxWealth\": 1000, \"wealthGrowth\": 10, \"offenseGreed\": 0.5, \"character\": { \"hostility\": 80, \"dormancy\": 80 } },"
	"  { \"faction\": \"c\", \"wealth\": 500, \"maxWealth\": 1000, \"wealthGrowth\": 10, \"offenseGreed\": 0.5, \"character\": { \"hostility\": 80, \"dormancy\": 80 } },"
	"  { \"faction\": \"d\", \"character\": { \"hostility\": -100, \"dormancy\": -100 } } ]";

// Busy settings so a few hundred days exercise everything.
const char* kBusyFormulas = "{ \"weights\": { \"warDeclareRate\": 0.5, \"battleRate\": 0.5, \"peaceRate\": 0.02 } }";

Config makeConfig(const FakeWorld& w, const std::string& settings, const std::string& extraFaction = std::string())
{
	ConfigLoader loader;
	loader.addFile(LAYER_MAIN, "settings", "settings.json", settings.empty() ? "{ \"seed\": 42 }" : settings);
	loader.addFile(LAYER_MAIN, "formulas", "formulas.json", kBusyFormulas);
	loader.addFile(LAYER_MAIN, "profiles", "factions/p.json", kHostileProfiles);
	if (!extraFaction.empty()) loader.addFile(LAYER_USER, "extra", "factions/x.json", extraFaction);
	Config c;
	Log log;
	loader.build(w, c, log);
	if (log.count(LOG_WARNING)) std::cout << log.format(LOG_WARNING);
	return c;
}

// Advance world and simulation together, checking every rule each day.
int runDays(FakeWorld& w, Simulation& sim, int days)
{
	int violations = 0;
	for (int d = 0; d < days; ++d)
	{
		w.advance(1);
		sim.update();
		std::vector<std::string> bad = checkInvariants(sim, w);
		for (size_t i = 0; i < bad.size() && violations < 5; ++i) std::cout << "  day " << w.currentDay() << ": " << bad[i] << "\n";
		violations += (int)bad.size();
	}
	return violations;
}

} // namespace

TEST(sim_wars_and_battles_happen_without_rule_violations)
{
	FakeWorld w;
	testWorld(w);
	Log log;
	Simulation sim(w, log);
	sim.setConfig(makeConfig(w, ""));
	sim.startNew();
	CHECK_EQ(runDays(w, sim, 365), 0);
	const WarStats& s = sim.state().stats;
	CHECK(s.warsDeclared > 0);
	CHECK(s.battlesLaunched > 0);
	CHECK(s.attackerWins + s.defenderWins > 0);
	// The peaceful faction far away never joined in.
	CHECK(!w.atWar("d", "a") && !w.atWar("d", "b") && !w.atWar("d", "c"));
}

TEST(sim_war_slots_are_respected)
{
	FakeWorld w;
	testWorld(w);
	Log log;
	Simulation sim(w, log);
	sim.setConfig(makeConfig(w, "{ \"seed\": 3, \"defaultWarSlots\": 1 }"));
	sim.startNew();
	for (int d = 0; d < 200; ++d)
	{
		w.advance(1);
		sim.update();
		CHECK(sim.warCount("a") <= 1);
		CHECK(sim.warCount("b") <= 1);
		CHECK(sim.warCount("c") <= 1);
	}
}

TEST(sim_sworn_allies_never_fight_and_sworn_enemies_start_at_war)
{
	FakeWorld w;
	testWorld(w);
	Log log;
	Simulation sim(w, log);
	sim.setConfig(makeConfig(w, "", "{ \"faction\": \"a\", \"swornAllies\": [\"b\"], \"swornEnemies\": [\"d\"] }"));
	sim.startNew();
	CHECK(w.allied("a", "b"));
	CHECK(w.relation("a", "b") >= 25);
	CHECK(w.atWar("a", "d"));
	CHECK(w.relation("a", "d") <= -25);
	CHECK_EQ(runDays(w, sim, 300), 0);
	CHECK(!w.atWar("a", "b"));
}

TEST(sim_unmet_factions_stay_out)
{
	FakeWorld w;
	testWorld(w);
	w.meet("a", 0);
	w.meet("b", 0);
	w.meet("c", 1000000);   // never, within this test
	w.meet("d", 1000000);
	Log log;
	Simulation sim(w, log);
	sim.setConfig(makeConfig(w, ""));
	sim.startNew();
	double frozen = w.relation("a", "c");
	CHECK_EQ(runDays(w, sim, 200), 0);
	CHECK(!w.atWar("a", "c") && !w.atWar("b", "c"));
	CHECK_NEAR(w.relation("a", "c"), frozen, 0);
	CHECK(sim.state().stats.warsDeclared > 0);   // a and b still fight

	// Once met, Gamma joins the system.
	w.meet("c", w.currentDay() + 1);
	runDays(w, sim, 5);
	CHECK(sim.isActive("c"));
}

TEST(sim_meet_requirement_can_be_turned_off_and_exempted)
{
	FakeWorld w;
	testWorld(w);
	w.meet("a", 1000000);
	w.meet("b", 1000000);
	w.meet("c", 1000000);
	w.meet("d", 1000000);
	Log log;
	Simulation sim(w, log);
	sim.setConfig(makeConfig(w, "{ \"seed\": 1, \"factionMeetRequirement\": false }"));
	sim.startNew();
	CHECK(sim.isActive("a"));

	Simulation sim2(w, log);
	sim2.setConfig(makeConfig(w, "", "{ \"faction\": \"a\", \"ignoreMeetRequirement\": true }"));
	sim2.startNew();
	CHECK(sim2.isActive("a"));
	CHECK(!sim2.isActive("b"));
	CHECK(!sim2.pairActive("a", "b"));   // both sides must be met (or exempt)
}

TEST(sim_opted_out_faction_is_ignored)
{
	FakeWorld w;
	testWorld(w);
	Log log;
	Simulation sim(w, log);
	sim.setConfig(makeConfig(w, "", "{ \"faction\": \"c\", \"enabled\": false }"));
	sim.startNew();
	double ac = w.relation("a", "c");
	CHECK_EQ(runDays(w, sim, 200), 0);
	CHECK(!w.atWar("a", "c") && !w.atWar("b", "c"));
	CHECK_NEAR(w.relation("a", "c"), ac, 0);
}

TEST(sim_squad_cap_queues_attacks)
{
	FakeWorld w;
	testWorld(w);
	w.travelSpeed = 100;   // slow marches keep squads in the field
	Log log;
	Simulation sim(w, log);
	sim.setConfig(makeConfig(w, "{ \"seed\": 9, \"maxWarSquads\": 4, \"defenseReserve\": 0.25 }"));
	sim.startNew();
	int maxSeen = 0;
	bool sawQueued = false;
	for (int d = 0; d < 300; ++d)
	{
		w.advance(1);
		sim.update();
		maxSeen = std::max(maxSeen, sim.state().activeSquads());
		for (size_t i = 0; i < sim.state().battles.size(); ++i)
			sawQueued = sawQueued || sim.state().battles[i].status == BATTLE_QUEUED;
		CHECK(checkInvariants(sim, w).empty());
	}
	CHECK(maxSeen <= 3);     // 4 * (1 - 0.25)
	CHECK(sawQueued);
}

TEST(sim_refused_campaign_costs_nothing)
{
	FakeWorld w;
	testWorld(w);
	w.refuseCampaigns(true);
	Log log;
	Simulation sim(w, log);
	sim.setConfig(makeConfig(w, ""));
	sim.startNew();
	runDays(w, sim, 100);
	CHECK_EQ(sim.state().stats.battlesLaunched, 0);
	CHECK(sim.state().stats.battlesCancelled > 0);
	CHECK(sim.state().battles.empty());
	// Wealth only grew.
	CHECK(sim.state().factions.find("a")->second.wealth >= 500);
}

TEST(sim_battle_results_move_wealth_and_relations)
{
	FakeWorld w;
	testWorld(w);
	w.travelSpeed = 1;   // nothing arrives by itself
	Log log;
	Simulation sim(w, log);
	// Marches never end here, so give the cap room for everyone's attacks.
	sim.setConfig(makeConfig(w, "{ \"seed\": 42, \"maxWarSquads\": 1000 }"));
	sim.startNew();
	sim.declareWar("a", "b");

	// Run until Alpha has an attack in the field.
	const Battle* found = 0;
	for (int d = 0; d < 200 && !found; ++d)
	{
		w.advance(1);
		sim.update();
		for (size_t i = 0; i < sim.state().battles.size(); ++i)
		{
			const Battle& b = sim.state().battles[i];
			if (b.status == BATTLE_ACTIVE && b.attacker == "a" && b.defender == "b") found = &b;
		}
	}
	CHECK(found);
	if (!found) return;
	int handle = found->handle;
	double spent = found->spent;
	CHECK(spent > 0);

	double beforeA = sim.state().factions.find("a")->second.wealth;
	double beforeB = sim.state().factions.find("b")->second.wealth;
	double relBefore = w.relation("a", "b");
	w.finishCampaign(handle, OUTCOME_ATTACKER_WON, 1.0);
	sim.tick(0);   // results only, no time passes
	double afterA = sim.state().factions.find("a")->second.wealth;
	double afterB = sim.state().factions.find("b")->second.wealth;
	CHECK(afterB < beforeB);
	CHECK(afterA > beforeA);
	CHECK(w.relation("a", "b") <= std::max(-100.0, relBefore - 4.9 + 1.0));   // hit minus at most one tick of drift
	CHECK(sim.state().factions.find("b")->second.weariness > 0);
}

TEST(sim_peace_sets_a_treaty_and_blocks_redeclaring)
{
	FakeWorld w;
	testWorld(w);
	Log log;
	Simulation sim(w, log);
	sim.setConfig(makeConfig(w, "{ \"seed\": 5, \"peaceTreatyDays\": 50 }"));
	sim.startNew();
	sim.declareWar("a", "b");
	sim.makePeace("a", "b");
	CHECK(!w.atWar("a", "b"));
	CHECK(w.treaty("a", "b"));
	for (int d = 0; d < 49; ++d)
	{
		w.advance(1);
		sim.update();
		CHECK(!w.atWar("a", "b"));
	}
	runDays(w, sim, 5);
	CHECK(!w.treaty("a", "b"));
}

TEST(sim_notifications_follow_the_setting)
{
	FakeWorld w;
	testWorld(w);
	Log log;
	Simulation sim(w, log);
	sim.setConfig(makeConfig(w, "{ \"seed\": 2, \"notifications\": \"off\" }"));
	sim.startNew();
	runDays(w, sim, 100);
	CHECK(w.messages().empty());

	FakeWorld w2;
	testWorld(w2);
	Simulation sim2(w2, log);
	sim2.setConfig(makeConfig(w2, "{ \"seed\": 2, \"notifications\": \"all\" }"));
	sim2.startNew();
	runDays(w2, sim2, 100);
	CHECK(!w2.messages().empty());
}

TEST(sim_save_and_load_continue_identically)
{
	// Run A: 120 days straight. Run B: 60 days, save, load into a new
	// Simulation, 60 more days. Both must end in the same place.
	FakeWorld wa, wb;
	testWorld(wa);
	testWorld(wb);
	Log log;
	Config ca = makeConfig(wa, "");
	Config cb = makeConfig(wb, "");

	Simulation simA(wa, log);
	simA.setConfig(ca);
	simA.startNew();
	runDays(wa, simA, 120);

	std::string saved;
	{
		Simulation simB(wb, log);
		simB.setConfig(cb);
		simB.startNew();
		runDays(wb, simB, 60);
		saved = simB.saveState();
	}
	Simulation simB2(wb, log);
	simB2.setConfig(cb);
	CHECK(simB2.loadState(saved));
	runDays(wb, simB2, 60);

	CHECK_EQ(simA.saveState(), simB2.saveState());
	CHECK_NEAR(wa.relation("a", "b"), wb.relation("a", "b"), 0);
	CHECK_EQ(wa.atWar("a", "c"), wb.atWar("a", "c"));
}

TEST(sim_bad_save_data_starts_fresh)
{
	FakeWorld w;
	testWorld(w);
	Log log;
	Simulation sim(w, log);
	sim.setConfig(makeConfig(w, ""));
	CHECK(!sim.loadState("{ not json"));
	CHECK(sim.state().initialized);
	CHECK(log.count(LOG_ERROR) > 0);
	CHECK(!sim.loadState("{ \"version\": 99 }"));
}

TEST(sim_config_reload_clamps_wealth)
{
	FakeWorld w;
	testWorld(w);
	Log log;
	Simulation sim(w, log);
	sim.setConfig(makeConfig(w, ""));
	sim.startNew();
	CHECK_NEAR(sim.state().factions.find("a")->second.wealth, 500, 0);
	sim.setConfig(makeConfig(w, "", "{ \"faction\": \"a\", \"maxWealth\": 100 }"));
	CHECK_NEAR(sim.state().factions.find("a")->second.wealth, 100, 0);
}

TEST(sim_sample_world_two_years)
{
	FakeWorld w;
	buildSampleWorld(w, true);
	Log log;
	Simulation sim(w, log);
	ConfigLoader loader;
	loader.addFile(LAYER_MAIN, "settings", "settings.json", "{ \"seed\": 11 }");
	Config c;
	loader.build(w, c, log);
	sim.setConfig(c);
	sim.startNew();
	CHECK_EQ(runDays(w, sim, 730), 0);
}
