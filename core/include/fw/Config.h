// Mod settings: built from auto defaults (from the game's FCS data) with the
// JSON layers from every FactionWars/ folder merged on top, field by field.
#ifndef FW_CONFIG_H
#define FW_CONFIG_H

#include "fw/Common.h"
#include "fw/Formulas.h"

#include <map>
#include <string>
#include <vector>

namespace fw {

class IWorld;
class Log;

enum NotifyMode { NOTIFY_OFF, NOTIFY_NEARBY, NOTIFY_KNOWN, NOTIFY_ALL };

struct Settings
{
	double temperature;          // Calm 0.5 · Normal 1.0 · Chaotic 2.0
	double tickDays;
	bool factionMeetRequirement;
	bool showUnmetFactions;
	int maxWarSquads;
	double defenseReserve;       // share of maxWarSquads kept for defense
	double surgeChance;
	double surgeMultiplier;
	NotifyMode notifications;
	bool debug;
	int defaultWarSlots;
	double peaceTreatyDays;      // no new war between the pair for this long after peace
	double queueExpiryDays;      // queued attacks are dropped after this long
	int seed;                    // 0 = seed from the save

	Settings();
};

struct Character
{
	// -100 = peaceful/passive, +100 = aggressive/active for all three.
	double hostility;
	double opportunistic;
	double dormancy;
	Character() : hostility(0), opportunistic(0), dormancy(0) {}
};

struct FactionProfile
{
	FactionId id;
	std::string name;
	bool enabled;                // false = opted out of the war system
	bool ignoreMeetRequirement;
	double wealth;               // starting wealth
	double wealthGrowth;
	double maxWealth;
	double offenseGreed;
	double defenceGreed;
	double friendlyJoin;
	double hostileJoin;
	int warSlots;
	int maxSquads;               // 0 = only the global cap applies
	double surgeChance;          // < 0 = use the global setting
	double surgeMultiplier;      // < 0 = use the global setting
	Character character;
	std::vector<FactionId> likelyAllies;
	std::vector<FactionId> likelyEnemies;
	std::vector<FactionId> swornAllies;
	std::vector<FactionId> swornEnemies;

	FactionProfile();
};

struct TownProfile
{
	TownId id;
	std::string name;
	double importance;           // 0..4.x, see feature doc §6
	TownProfile() : importance(0) {}
};

struct SquadProfile
{
	SquadId id;
	std::string name;
	FactionId faction;
	double cost;
	bool war;                    // may be sent to war
	SquadProfile() : cost(1), war(true) {}
};

// World events are merged and kept for the event engine (later phase).
struct EventDef
{
	std::string id;
	double weight;
	std::string json;            // merged definition, compact JSON
	EventDef() : weight(1) {}
};

struct Config
{
	Settings settings;
	Formulas formulas;
	std::map<FactionId, FactionProfile> factions;
	std::map<TownId, TownProfile> towns;
	std::map<SquadId, SquadProfile> squads;
	std::map<std::string, EventDef> events;

	const FactionProfile* faction(const FactionId& id) const;
	double townImportance(const TownId& id) const;
};

enum LayerKind { LAYER_MAIN, LAYER_ADDON, LAYER_USER };

class ConfigLoader
{
public:
	// `relPath` is the path inside a FactionWars/ folder with '/' separators,
	// e.g. "settings.json" or "factions/western_hive.json". `origin` names
	// the file in log messages. Add files in layer order: main mod, addons in
	// mod load order, then user overrides.
	void addFile(LayerKind kind, const std::string& origin, const std::string& relPath, const std::string& text);

	// Builds the final config. Never fails: broken files and bad values are
	// logged and skipped. Returns the number of errors logged.
	size_t build(const IWorld& world, Config& out, Log& log) const;

	void clear() { files_.clear(); }

private:
	struct File
	{
		LayerKind kind;
		std::string origin;
		std::string relPath;
		std::string text;
	};
	std::vector<File> files_;
};

} // namespace fw

#endif
