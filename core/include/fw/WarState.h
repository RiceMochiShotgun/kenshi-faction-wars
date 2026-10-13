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

struct Battle
{
	int id;
	int handle;              // vanilla campaign handle while active
	BattleStatus status;
	FactionId attacker;
	FactionId defender;
	TownId homeTown;
	TownId targetTown;
	std::vector<SquadId> squads;
	double spent;            // wealth taken from the attacker
	double createdDay;
	bool surge;
	Battle() : id(0), handle(0), status(BATTLE_QUEUED), spent(0), createdDay(0), surge(false) {}
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
	WarStats() : warsDeclared(0), peacesMade(0), battlesLaunched(0), attackerWins(0), defenderWins(0),
		battlesCancelled(0), battlesExpired(0) {}
};

struct WarState
{
	enum { kVersion = 1 };

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

	int activeSquads() const;
	int activeSquads(const FactionId& f) const;

	std::string toJson() const;
	// On failure leaves the state unchanged and fills `error`.
	bool fromJson(const std::string& json, std::string& error);
};

} // namespace fw

#endif
