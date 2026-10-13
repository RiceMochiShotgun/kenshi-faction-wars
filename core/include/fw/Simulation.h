// The war brain: a periodic tick that grows wealth, drifts relations,
// declares war and peace, and launches attacks through vanilla campaigns.
// It never touches Kenshi directly; everything goes through IWorld.
#ifndef FW_SIMULATION_H
#define FW_SIMULATION_H

#include "fw/Config.h"
#include "fw/Log.h"
#include "fw/Random.h"
#include "fw/WarState.h"
#include "fw/World.h"

#include <map>
#include <string>
#include <vector>

namespace fw {

class Simulation
{
public:
	Simulation(IWorld& world, Log& log);

	// Replaces the config (on save load or the debug reload key). Safe to
	// call mid-game; state for factions that disappeared is kept but unused.
	void setConfig(const Config& config);
	const Config& config() const { return config_; }

	// New game, or a save without our data.
	void startNew();
	// Restores saved state. On failure logs it and starts fresh.
	bool loadState(const std::string& json);
	std::string saveState() const;

	// Call as often as you like; runs as many ticks as in-game time allows
	// (at most `maxTicks` per call, the rest catch up on later calls).
	void update(int maxTicks = 10);
	// One simulation step covering `days` of in-game time.
	void tick(double days);

	const WarState& state() const { return state_; }
	WarState& mutableState() { return state_; }

	// Enabled and met by the player (or exempt from the meet requirement).
	bool isActive(const FactionId& f) const;
	bool pairActive(const FactionId& a, const FactionId& b) const { return a != b && isActive(a) && isActive(b); }
	bool sworn(const FactionId& a, const FactionId& b, bool allies) const;
	int warCount(const FactionId& f) const;
	int offenseCapacity() const;

	// Apply a relation change with the sworn limits and the -100..100 range.
	void changeRelation(const FactionId& a, const FactionId& b, double delta);
	void setRelationClamped(const FactionId& a, const FactionId& b, double value);

	void declareWar(const FactionId& attacker, const FactionId& defender);
	void makePeace(const FactionId& a, const FactionId& b);

private:
	IWorld& world_;
	Log& log_;
	Config config_;
	WarState state_;
	Rng rng_;
	double now_;

	// Rebuilt each tick from the world.
	std::vector<WorldFaction> factions_;
	std::vector<WorldTown> towns_;
	std::map<FactionId, std::vector<size_t> > townsByOwner_;
	std::map<FactionId, std::vector<const SquadProfile*> > warSquads_;   // cheapest first

	void refreshWorld();
	void ensureFactionStates();
	void updateMet();
	void collectResults();
	void growWealth(double days);
	void driftRelations(double days);
	void updateWars(double days);
	void processQueue();
	void planAttacks(double days);
	void easeWeariness(double days);

	const FactionProfile* profile(const FactionId& f) const { return config_.faction(f); }
	FactionState& fstate(const FactionId& f) { return state_.factions[f]; }
	const WorldTown* town(const TownId& id) const;
	double distance(const WorldTown& a, const WorldTown& b) const;
	const WorldTown* nearestOwnTown(const FactionId& f, const WorldTown& target) const;
	bool canLaunch(const Battle& b) const;
	bool launch(Battle& b);
	void planBattle(const FactionId& attacker, const FactionId& defender, const WorldTown& target);
	void addWealth(const FactionId& f, double delta);
	void notify(const std::string& text, const FactionId& a, const FactionId& b, bool major);
	std::string factionName(const FactionId& f) const;
	std::string townName(const TownId& t) const;
};

// Rule checks used by tests, the headless runner and the in-game debug dump.
// Returns the list of broken rules (empty = all good).
std::vector<std::string> checkInvariants(const Simulation& sim, const IWorld& world);

} // namespace fw

#endif
