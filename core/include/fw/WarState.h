// Everything the mod saves with the game. Only data Kenshi has no slot for:
// relations, war flags and town owners stay in vanilla state.
#ifndef FW_WARSTATE_H
#define FW_WARSTATE_H

#include "fw/Common.h"

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

namespace fw {

struct FactionState
{
	double wealth;
	double weariness;        // grows with lost battles, eases over time
	bool met;                // once met, stays met
	FactionState() : wealth(0), weariness(0), met(false) {}
};

struct PairState
{
	double warStartDay;      // < 0 = not at war through us
	double peaceUntil;       // no new war before this day
	PairState() : warStartDay(-1), peaceUntil(-1) {}
};

enum BattleStatus { BATTLE_QUEUED, BATTLE_ACTIVE };
enum BattleRole { ROLE_ATTACK, ROLE_DEFEND };

// One force in the field or waiting to set out. An attack belongs to
// `attacker`; a counter-force (ROLE_DEFEND) belongs to `defender` and
// answers the attack `answers`. Counter-forces are never queued.
struct Battle
{
	int id;
	int handle;              // vanilla campaign handle while active
	BattleStatus status;
	BattleRole role;
	FactionId attacker;
	FactionId defender;
	TownId homeTown;         // where the squads set out from
	TownId targetTown;       // the town under attack
	std::vector<SquadId> squads;
	double spent;            // wealth taken from the force's owner
	double createdDay;
	bool surge;
	int answers;             // counter-force: id of the attack it answers
	double estimate;         // attack: the defender's guess of its strength, < 0 = not seen yet
	double estimateError;    // attack: error range of that guess
	Battle() : id(0), handle(0), status(BATTLE_QUEUED), role(ROLE_ATTACK), spent(0), createdDay(0), surge(false),
		answers(0), estimate(-1), estimateError(-1) {}

	const FactionId& owner() const { return role == ROLE_DEFEND ? defender : attacker; }
};

struct WarStats
{
	int warsDeclared;
	int peacesMade;
	int battlesLaunched;
	int attackerWins;
	int defenderWins;
	int battlesCancelled;
	int battlesExpired;
	int counterForces;       // counter-forces sent to defend a town
	WarStats() : warsDeclared(0), peacesMade(0), battlesLaunched(0), attackerWins(0), defenderWins(0),
		battlesCancelled(0), battlesExpired(0), counterForces(0) {}
};

struct WarState
{
	enum { kVersion = 2 };

	bool initialized;
	double lastTickDay;
	uint64_t rngState;
	int nextBattleId;
	std::map<FactionId, FactionState> factions;
	std::map<std::string, PairState> pairs;    // key: pairKey(a, b)
	std::vector<Battle> battles;               // queued and active
	WarStats stats;

	WarState() : initialized(false), lastTickDay(0), rngState(0), nextBattleId(1) {}

	PairState& pair(const FactionId& a, const FactionId& b) { return pairs[pairKey(a, b)]; }
	const PairState* findPair(const FactionId& a, const FactionId& b) const;

	int activeSquads() const;                     // every live squad
	int activeSquads(const FactionId& f) const;   // live squads owned by f
	int activeAttackSquads() const;               // live squads on attacks
	const Battle* findBattle(int id) const;

	std::string toJson() const;
	// On failure leaves the state unchanged and fills `error`.
	bool fromJson(const std::string& json, std::string& error);
};

} // namespace fw

#endif
