// Headless runner: the simulation on the sample world for years of in-game
// time, checking every rule each day. Use it to tune formulas.json and
// settings.json without starting Kenshi.
//
//   fw_headless [--years N] [--seed S] [--data DIR] [--user DIR] [--meet-all] [--quiet] [--log]
#include "DataDir.h"
#include "FakeWorld.h"
#include "SampleWorld.h"
#include "fw/Simulation.h"

#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

using namespace fw;

namespace {

void usage()
{
	std::cout << "fw_headless [--years N] [--seed S] [--data DIR] [--user DIR] [--meet-all] [--quiet] [--log]\n"
		"  --data DIR   FactionWars/ folder to load (default: the repo's data/FactionWars)\n"
		"  --user DIR   extra folder loaded last, like user/FactionWars/\n"
		"  --meet-all   the player has met every faction from day one\n"
		"  --log        print the simulation log at the end\n";
}

void printYear(int year, const Simulation& sim, const FakeWorld& w)
{
	std::vector<WorldFaction> fs;
	w.listFactions(fs);
	int wars = 0;
	for (size_t i = 0; i < fs.size(); ++i)
		for (size_t j = i + 1; j < fs.size(); ++j)
			if (w.atWar(fs[i].id, fs[j].id)) ++wars;

	const WarStats& s = sim.state().stats;
	std::cout << "Year " << year << ": " << wars << " wars now, " << s.warsDeclared << " declared, "
		<< s.peacesMade << " peace, " << s.battlesLaunched << " battles (" << s.attackerWins << " raids won, "
		<< s.defenderWins << " repelled, " << s.battlesCancelled << " cancelled, " << s.battlesExpired << " expired)\n";
	std::cout << "  wealth:";
	for (size_t i = 0; i < fs.size(); ++i)
	{
		std::map<FactionId, FactionState>::const_iterator it = sim.state().factions.find(fs[i].id);
		if (it == sim.state().factions.end()) continue;
		std::cout << " " << fs[i].id << "=" << (int)it->second.wealth << (sim.isActive(fs[i].id) ? "" : "(unmet)");
	}
	std::cout << "\n";
}

} // namespace

int main(int argc, char** argv)
{
	int years = 10;
	int seed = 7;
	bool quiet = false, meetAll = false, showLog = false;
	std::string data = FW_DATA_DIR, user;
	for (int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "--years" && i + 1 < argc) years = std::atoi(argv[++i]);
		else if (a == "--seed" && i + 1 < argc) seed = std::atoi(argv[++i]);
		else if (a == "--data" && i + 1 < argc) data = argv[++i];
		else if (a == "--user" && i + 1 < argc) user = argv[++i];
		else if (a == "--meet-all") meetAll = true;
		else if (a == "--quiet") quiet = true;
		else if (a == "--log") showLog = true;
		else { usage(); return a == "--help" ? 0 : 2; }
	}

	FakeWorld world;
	buildSampleWorld(world, !meetAll);
	world.setSeed((uint64_t)seed * 7919 + 1);

	ConfigLoader loader;
	int files = addDataDir(loader, LAYER_MAIN, data, "FactionWars/");
	if (!user.empty()) files += addDataDir(loader, LAYER_USER, user, "user/FactionWars/");
	Log configLog;
	Config config;
	loader.build(world, config, configLog);
	if (!seed) seed = 1;
	config.settings.seed = seed;
	std::cout << "Loaded " << files << " files from " << data << "\n";
	if (configLog.count(LOG_WARNING)) std::cout << configLog.format(LOG_WARNING);

	Log log;
	log.setLimit(20000);
	Simulation sim(world, log);
	sim.setConfig(config);
	sim.startNew();

	int violations = 0;
	int days = years * 365;
	for (int d = 1; d <= days; ++d)
	{
		world.advance(1);
		sim.update();
		std::vector<std::string> bad = checkInvariants(sim, world);
		for (size_t i = 0; i < bad.size(); ++i)
		{
			if (violations < 20) std::cout << "RULE BROKEN day " << d << ": " << bad[i] << "\n";
			++violations;
		}
		if (!quiet && d % 365 == 0) printYear(d / 365, sim, world);
	}
	if (quiet) printYear(years, sim, world);
	if (showLog) std::cout << log.format(LOG_INFO);

	if (violations)
	{
		std::cout << violations << " rule violations in " << years << " years\n";
		return 1;
	}
	std::cout << "No rule violations in " << years << " years\n";
	return 0;
}
