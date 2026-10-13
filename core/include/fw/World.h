// The core's only view of the game. The plugin implements this on KenshiLib;
// tests implement it with a fake world. Vanilla state (relations, war flags,
// town owners) is read and written through here and never copied into the
// core's own state.
#ifndef FW_WORLD_H
#define FW_WORLD_H

#include "fw/Common.h"

#include <string>
#include <vector>

namespace fw {

struct WorldFaction
{
	FactionId id;          // FCS string ID
	std::string name;      // display name
	bool enemyByDefault;   // vanilla "enemy of most factions" flag, seeds hostility
	WorldFaction() : enemyByDefault(false) {}
};

struct WorldTown
{
	TownId id;
	std::string name;
	FactionId owner;
	double x, y;           // world position
	int population;        // living residents
	bool outpost;
	bool hasHQ;
	WorldTown() : x(0), y(0), population(0), outpost(false), hasHQ(false) {}
};

struct WorldSquad
{
	SquadId id;            // FCS squad template ID
	std::string name;
	FactionId faction;
	int size;              // number of characters
	WorldSquad() : size(0) {}
};

// Everything needed to send one force through vanilla campaigns: an attack
// on an enemy town, or a counter-force marching to defend one of our own.
struct AttackOrder
{
	int battleId;
	FactionId attacker;
	FactionId defender;
	TownId homeTown;       // where the squads set out from
	TownId targetTown;     // the town under attack
	std::vector<SquadId> squads;
	double strength;       // total squad cost
	// Counter-force: the squads belong to `defender`, march from homeTown to
	// targetTown and fight the attack campaign `answersHandle` there.
	bool defending;
	int answersHandle;
	AttackOrder() : battleId(0), strength(0), defending(false), answersHandle(0) {}
};

enum CampaignOutcome { OUTCOME_ATTACKER_WON, OUTCOME_DEFENDER_WON, OUTCOME_CANCELLED };

struct CampaignResult
{
	int handle;
	// For a counter-force, the outcome of the attack it answered, or
	// OUTCOME_CANCELLED if it never reached the town.
	CampaignOutcome outcome;
	double survivors;      // 0..1 share of this campaign's own force still alive
	CampaignResult() : handle(0), outcome(OUTCOME_CANCELLED), survivors(0) {}
};

class IWorld
{
public:
	virtual ~IWorld() {}

	// In-game time in days since the start of the save.
	virtual double currentDay() const = 0;

	virtual void listFactions(std::vector<WorldFaction>& out) const = 0;
	virtual void listTowns(std::vector<WorldTown>& out) const = 0;
	virtual void listSquads(std::vector<WorldSquad>& out) const = 0;

	// Relations are symmetric, -100..100.
	virtual double relation(const FactionId& a, const FactionId& b) const = 0;
	virtual void setRelation(const FactionId& a, const FactionId& b, double value) = 0;

	virtual bool atWar(const FactionId& a, const FactionId& b) const = 0;
	virtual void setAtWar(const FactionId& a, const FactionId& b, bool war) = 0;
	virtual bool allied(const FactionId& a, const FactionId& b) const = 0;
	virtual void setAllied(const FactionId& a, const FactionId& b, bool allied) = 0;
	virtual void setPeaceTreaty(const FactionId& a, const FactionId& b, bool treaty) = 0;

	// Has the player met this faction (discovered a town, talked, fought...).
	virtual bool playerHasMet(const FactionId& f) const = 0;

	// Start a vanilla campaign. Returns a handle > 0, or 0 if it couldn't start.
	virtual int launchAttack(const AttackOrder& order) = 0;
	// Campaigns that ended since the last call. When an attack ends, for any
	// reason, every counter-force answering it ends in the same batch.
	virtual void pollFinishedCampaigns(std::vector<CampaignResult>& out) = 0;
	// Where a campaign's squads are now (their average position). False if
	// the campaign is unknown or its position can't be read.
	virtual bool campaignPosition(int handle, double& x, double& y) const = 0;

	// Short top-of-screen message. The adapter queues it for the UI thread.
	virtual void notify(const std::string& text) = 0;
};

} // namespace fw

#endif
