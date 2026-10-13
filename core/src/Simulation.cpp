#include "fw/Simulation.h"

#include <algorithm>
#include <cmath>

namespace fw {

namespace {

bool cheaperFirst(const SquadProfile* a, const SquadProfile* b)
{
	return a->cost < b->cost || (a->cost == b->cost && a->id < b->id);
}

const double kRelationMin = -100, kRelationMax = 100;
const double kSwornAllyFloor = 25, kSwornEnemyCeiling = -25;
// Share of a battle's cost that comes back with each surviving squad.
const double kSurvivorRefund = 0.5;

std::string squadCount(size_t n) { return toString(n) + (n == 1 ? " squad" : " squads"); }

} // namespace

Simulation::Simulation(IWorld& world, Log& log)
	: world_(world), log_(log), now_(0)
{
}

void Simulation::setConfig(const Config& config)
{
	config_ = config;
	// A reload can lower maxWealth below what a faction already has.
	for (std::map<FactionId, FactionState>::iterator it = state_.factions.begin(); it != state_.factions.end(); ++it)
	{
		const FactionProfile* p = config_.faction(it->first);
		if (p && it->second.wealth > p->maxWealth) it->second.wealth = p->maxWealth;
	}
}

// ---- lifecycle -------------------------------------------------------------

void Simulation::startNew()
{
	state_ = WarState();
	now_ = world_.currentDay();

	uint64_t seed = config_.settings.seed
		? (uint64_t)config_.settings.seed
		: (uint64_t)(now_ * 1000.0) ^ 0xA5A5F00DCAFEULL ^ ((uint64_t)config_.factions.size() << 32);
	rng_.setState(seed);

	refreshWorld();
	ensureFactionStates();
	updateMet();

	// Sworn relationships start in force: sworn allies are allied, and sworn
	// enemies start at war if both have a free war slot.
	for (std::map<FactionId, FactionProfile>::const_iterator it = config_.factions.begin(); it != config_.factions.end(); ++it)
	{
		const FactionProfile& p = it->second;
		if (!p.enabled) continue;
		for (size_t i = 0; i < p.swornAllies.size(); ++i)
		{
			const FactionId& o = p.swornAllies[i];
			const FactionProfile* op = profile(o);
			if (!op || !op->enabled || o < p.id) continue;
			setRelationClamped(p.id, o, world_.relation(p.id, o));
			world_.setAtWar(p.id, o, false);
			world_.setAllied(p.id, o, true);
		}
		for (size_t i = 0; i < p.swornEnemies.size(); ++i)
		{
			const FactionId& o = p.swornEnemies[i];
			const FactionProfile* op = profile(o);
			if (!op || !op->enabled || o < p.id) continue;
			setRelationClamped(p.id, o, world_.relation(p.id, o));
			world_.setAllied(p.id, o, false);
			if (pairActive(p.id, o) && !world_.atWar(p.id, o) &&
				warCount(p.id) < p.warSlots && warCount(o) < op->warSlots)
				declareWar(p.id, o);
		}
	}

	state_.initialized = true;
	state_.lastTickDay = now_;
	state_.rngState = rng_.state();
}

bool Simulation::loadState(const std::string& json)
{
	std::string error;
	WarState loaded;
	if (!loaded.fromJson(json, error))
	{
		log_.error("save", "could not read Faction Wars save data (" + error + "); starting fresh");
		startNew();
		return false;
	}
	state_ = loaded;
	rng_.setState(state_.rngState);
	if (!state_.initialized) startNew();
	return true;
}

std::string Simulation::saveState() const
{
	WarState copy = state_;
	copy.rngState = rng_.state();
	return copy.toJson();
}

void Simulation::update(int maxTicks)
{
	if (!state_.initialized) startNew();
	double step = config_.settings.tickDays;
	double now = world_.currentDay();
	if (now < state_.lastTickDay) state_.lastTickDay = now;   // time went backwards (shouldn't happen)
	for (int i = 0; i < maxTicks && now - state_.lastTickDay >= step; ++i)
	{
		state_.lastTickDay += step;
		tick(step);
	}
}

void Simulation::tick(double days)
{
	now_ = world_.currentDay();
	refreshWorld();
	ensureFactionStates();
	updateMet();
	collectResults();
	growWealth(days);
	driftRelations(days);
	updateWars(days);
	processQueue();
	planAttacks(days);
	defendTowns();
	easeWeariness(days);
	state_.rngState = rng_.state();
}

// ---- world snapshot --------------------------------------------------------

void Simulation::refreshWorld()
{
	factions_.clear();
	towns_.clear();
	townsByOwner_.clear();
	warSquads_.clear();
	world_.listFactions(factions_);
	world_.listTowns(towns_);
	for (size_t i = 0; i < towns_.size(); ++i) townsByOwner_[towns_[i].owner].push_back(i);
	for (std::map<SquadId, SquadProfile>::const_iterator it = config_.squads.begin(); it != config_.squads.end(); ++it)
		if (it->second.war) warSquads_[it->second.faction].push_back(&it->second);
	for (std::map<FactionId, std::vector<const SquadProfile*> >::iterator it = warSquads_.begin(); it != warSquads_.end(); ++it)
		std::sort(it->second.begin(), it->second.end(), cheaperFirst);
}

void Simulation::ensureFactionStates()
{
	for (std::map<FactionId, FactionProfile>::const_iterator it = config_.factions.begin(); it != config_.factions.end(); ++it)
	{
		if (state_.factions.count(it->first)) continue;
		FactionState s;
		s.wealth = it->second.wealth;
		state_.factions[it->first] = s;
	}
}

void Simulation::updateMet()
{
	for (std::map<FactionId, FactionProfile>::const_iterator it = config_.factions.begin(); it != config_.factions.end(); ++it)
	{
		FactionState& s = fstate(it->first);
		if (!s.met && world_.playerHasMet(it->first))
		{
			s.met = true;
			log_.debug("sim", "player met " + it->second.name);
		}
	}
}

const WorldTown* Simulation::town(const TownId& id) const
{
	for (size_t i = 0; i < towns_.size(); ++i)
		if (towns_[i].id == id) return &towns_[i];
	return 0;
}

double Simulation::distance(const WorldTown& a, const WorldTown& b) const
{
	double dx = a.x - b.x, dy = a.y - b.y;
	return std::sqrt(dx * dx + dy * dy);
}

const WorldTown* Simulation::nearestOwnTown(const FactionId& f, const WorldTown& target, const WorldTown* exclude) const
{
	std::map<FactionId, std::vector<size_t> >::const_iterator it = townsByOwner_.find(f);
	if (it == townsByOwner_.end()) return 0;
	const WorldTown* best = 0;
	double bestD = 0;
	for (size_t i = 0; i < it->second.size(); ++i)
	{
		const WorldTown& t = towns_[it->second[i]];
		if (exclude && t.id == exclude->id) continue;
		double d = distance(t, target);
		if (!best || d < bestD) { best = &t; bestD = d; }
	}
	return best;
}

double Simulation::distanceToNearestTown(const FactionId& f, double x, double y) const
{
	WorldTown here;
	here.x = x;
	here.y = y;
	const WorldTown* t = nearestOwnTown(f, here);
	return t ? distance(*t, here) : 0;
}

// ---- queries -----------------------------------------------------------------

bool Simulation::isActive(const FactionId& f) const
{
	const FactionProfile* p = profile(f);
	if (!p || !p->enabled) return false;
	if (!config_.settings.factionMeetRequirement || p->ignoreMeetRequirement) return true;
	std::map<FactionId, FactionState>::const_iterator s = state_.factions.find(f);
	return s != state_.factions.end() && s->second.met;
}

bool Simulation::sworn(const FactionId& a, const FactionId& b, bool allies) const
{
	const FactionProfile* pa = profile(a);
	const FactionProfile* pb = profile(b);
	if (allies)
		return (pa && contains(pa->swornAllies, b)) || (pb && contains(pb->swornAllies, a));
	return (pa && contains(pa->swornEnemies, b)) || (pb && contains(pb->swornEnemies, a));
}

int Simulation::warCount(const FactionId& f) const
{
	int n = 0;
	for (size_t i = 0; i < factions_.size(); ++i)
		if (factions_[i].id != f && world_.atWar(f, factions_[i].id)) ++n;
	return n;
}

int Simulation::offenseCapacity() const
{
	const Settings& s = config_.settings;
	return (int)std::floor(s.maxWarSquads * (1.0 - s.defenseReserve));
}

double Simulation::garrisonStrength(const WorldTown& t) const
{
	double in[3] = { (double)t.population, config_.townImportance(t.id), t.outpost ? 1.0 : 0.0 };
	return std::max(0.0, finiteOr(config_.formulas.eval(F_GARRISON_STRENGTH, in), 0));
}

std::string Simulation::factionName(const FactionId& f) const
{
	const FactionProfile* p = profile(f);
	return p ? p->name : f;
}

std::string Simulation::townName(const TownId& t) const
{
	const WorldTown* w = town(t);
	return w ? w->name : t;
}

// ---- relations -----------------------------------------------------------------

void Simulation::setRelationClamped(const FactionId& a, const FactionId& b, double value)
{
	double v = clampd(finiteOr(value, 0), kRelationMin, kRelationMax);
	if (sworn(a, b, true) && v < kSwornAllyFloor) v = kSwornAllyFloor;
	if (sworn(a, b, false) && v > kSwornEnemyCeiling) v = kSwornEnemyCeiling;
	if (v != world_.relation(a, b)) world_.setRelation(a, b, v);
}

void Simulation::changeRelation(const FactionId& a, const FactionId& b, double delta)
{
	setRelationClamped(a, b, world_.relation(a, b) + delta);
}

void Simulation::driftRelations(double days)
{
	for (size_t i = 0; i < factions_.size(); ++i)
	{
		const FactionId& a = factions_[i].id;
		const FactionProfile* pa = profile(a);
		if (!pa) continue;
		for (size_t j = i + 1; j < factions_.size(); ++j)
		{
			const FactionId& b = factions_[j].id;
			const FactionProfile* pb = profile(b);
			if (!pb) continue;

			// Unmet or opted-out pairs stay frozen, but sworn limits always hold.
			if (!pairActive(a, b))
			{
				if (pa->enabled && pb->enabled && (sworn(a, b, true) || sworn(a, b, false)))
					setRelationClamped(a, b, world_.relation(a, b));
				continue;
			}

			int shared = 0;
			for (size_t k = 0; k < factions_.size(); ++k)
			{
				const FactionId& c = factions_[k].id;
				if (c != a && c != b && world_.atWar(a, c) && world_.atWar(b, c)) ++shared;
			}
			double in[8];
			in[0] = (pa->character.hostility + pb->character.hostility) / 2;
			in[1] = (contains(pa->likelyAllies, b) || contains(pb->likelyAllies, a)) ? 1 : 0;
			in[2] = (contains(pa->likelyEnemies, b) || contains(pb->likelyEnemies, a)) ? 1 : 0;
			in[3] = shared;
			in[4] = world_.atWar(a, b) ? 1 : 0;
			in[5] = world_.allied(a, b) ? 1 : 0;
			in[6] = rng_.range(-1, 1);
			in[7] = days;
			double delta = finiteOr(config_.formulas.eval(F_RELATION_DRIFT, in), 0);
			changeRelation(a, b, delta);
		}
	}
}

// ---- wealth ----------------------------------------------------------------------

void Simulation::addWealth(const FactionId& f, double delta)
{
	const FactionProfile* p = profile(f);
	FactionState& s = fstate(f);
	double maxW = p ? p->maxWealth : 1e9;
	s.wealth = clampd(s.wealth + finiteOr(delta, 0), 0, std::max(maxW, s.wealth));
}

void Simulation::growWealth(double days)
{
	for (std::map<FactionId, FactionProfile>::const_iterator it = config_.factions.begin(); it != config_.factions.end(); ++it)
	{
		const FactionProfile& p = it->second;
		if (!p.enabled) continue;
		FactionState& s = fstate(p.id);
		double importance = 0;
		int count = 0;
		std::map<FactionId, std::vector<size_t> >::const_iterator owned = townsByOwner_.find(p.id);
		if (owned != townsByOwner_.end())
		{
			for (size_t i = 0; i < owned->second.size(); ++i)
			{
				importance += config_.townImportance(towns_[owned->second[i]].id);
				++count;
			}
		}
		double in[6] = { s.wealth, p.maxWealth, p.wealthGrowth, importance, (double)count, days };
		double income = std::max(0.0, finiteOr(config_.formulas.eval(F_WEALTH_INCOME, in), 0));
		if (s.wealth < p.maxWealth) s.wealth = std::min(p.maxWealth, s.wealth + income);
	}
}

void Simulation::easeWeariness(double days)
{
	double keep = std::max(0.0, 1.0 - 0.01 * days);
	for (std::map<FactionId, FactionState>::iterator it = state_.factions.begin(); it != state_.factions.end(); ++it)
		it->second.weariness *= keep;
}

// ---- war and peace ---------------------------------------------------------------

void Simulation::declareWar(const FactionId& attacker, const FactionId& defender)
{
	world_.setAllied(attacker, defender, false);
	world_.setPeaceTreaty(attacker, defender, false);
	world_.setAtWar(attacker, defender, true);
	PairState& ps = state_.pair(attacker, defender);
	ps.warStartDay = now_;
	ps.peaceUntil = -1;
	++state_.stats.warsDeclared;
	log_.info("sim", factionName(attacker) + " declared war on " + factionName(defender));
	notify(factionName(attacker) + " has declared war on " + factionName(defender) + ".", attacker, defender, true);
}

void Simulation::makePeace(const FactionId& a, const FactionId& b)
{
	world_.setAtWar(a, b, false);
	world_.setPeaceTreaty(a, b, true);
	PairState& ps = state_.pair(a, b);
	ps.warStartDay = -1;
	ps.peaceUntil = now_ + config_.settings.peaceTreatyDays;
	fstate(a).weariness *= 0.5;
	fstate(b).weariness *= 0.5;
	++state_.stats.peacesMade;

	// Planned attacks between them are called off; ones already marching finish.
	for (size_t i = 0; i < state_.battles.size(); )
	{
		const Battle& bt = state_.battles[i];
		bool between = (bt.attacker == a && bt.defender == b) || (bt.attacker == b && bt.defender == a);
		if (between && bt.status == BATTLE_QUEUED)
		{
			++state_.stats.battlesCancelled;
			state_.battles.erase(state_.battles.begin() + i);
		}
		else ++i;
	}
	log_.info("sim", factionName(a) + " and " + factionName(b) + " made peace");
	notify(factionName(a) + " and " + factionName(b) + " have made peace.", a, b, true);
}

void Simulation::updateWars(double days)
{
	const Settings& set = config_.settings;
	for (size_t i = 0; i < factions_.size(); ++i)
	{
		const FactionId& a = factions_[i].id;
		const FactionProfile* pa = profile(a);
		if (!pa || !pa->enabled) continue;
		for (size_t j = i + 1; j < factions_.size(); ++j)
		{
			const FactionId& b = factions_[j].id;
			const FactionProfile* pb = profile(b);
			if (!pb || !pb->enabled) continue;

			PairState* ps = 0;
			std::map<std::string, PairState>::iterator found = state_.pairs.find(pairKey(a, b));
			if (found != state_.pairs.end()) ps = &found->second;

			if (ps && ps->peaceUntil >= 0 && now_ >= ps->peaceUntil)
			{
				world_.setPeaceTreaty(a, b, false);
				ps->peaceUntil = -1;
			}

			bool war = world_.atWar(a, b);
			if (war && sworn(a, b, true))
			{
				// Something else (vanilla, another mod) put sworn allies at war.
				makePeace(a, b);
				continue;
			}
			if (!pairActive(a, b)) continue;

			if (war)
			{
				PairState& p = state_.pair(a, b);
				if (p.warStartDay < 0) p.warStartDay = now_;   // a war we didn't start
				double in[5];
				in[0] = world_.relation(a, b);
				in[1] = std::max(fstate(a).weariness, fstate(b).weariness);
				in[2] = now_ - p.warStartDay;
				in[3] = sworn(a, b, false) ? 1 : 0;
				in[4] = days;
				double chance = clampd(finiteOr(config_.formulas.eval(F_PEACE_CHANCE, in), 0), 0, 1);
				if (rng_.chance(chance)) makePeace(a, b);
				continue;
			}

			if (sworn(a, b, true) || world_.allied(a, b)) continue;
			if (ps && ps->peaceUntil >= 0 && now_ < ps->peaceUntil) continue;
			if (warCount(a) >= pa->warSlots || warCount(b) >= pb->warSlots) continue;

			double rel = world_.relation(a, b);
			double isSworn = sworn(a, b, false) ? 1 : 0;
			double inA[6] = { rel, pa->character.hostility, pa->character.dormancy, isSworn, set.temperature, days };
			double inB[6] = { rel, pb->character.hostility, pb->character.dormancy, isSworn, set.temperature, days };
			double ca = clampd(finiteOr(config_.formulas.eval(F_DECLARE_WAR_CHANCE, inA), 0), 0, 1);
			double cb = clampd(finiteOr(config_.formulas.eval(F_DECLARE_WAR_CHANCE, inB), 0), 0, 1);
			double either = 1 - (1 - ca) * (1 - cb);
			if (either > 0 && rng_.chance(either))
			{
				if (ca >= cb) declareWar(a, b);
				else declareWar(b, a);
			}
		}
	}
}

// ---- battles -----------------------------------------------------------------

// Attacks share the offense part of the cap; counter-forces can also use the
// reserve, so a town can always be defended while attacks fill the world.
bool Simulation::canLaunch(const Battle& b) const
{
	int n = (int)b.squads.size();
	if (state_.activeSquads() + n > config_.settings.maxWarSquads) return false;
	if (b.role == ROLE_ATTACK && state_.activeAttackSquads() + n > offenseCapacity()) return false;
	const FactionProfile* p = profile(b.owner());
	if (p && p->maxSquads > 0 && state_.activeSquads(b.owner()) + n > p->maxSquads) return false;
	return true;
}

bool Simulation::launch(Battle& b)
{
	// Wealth may have dropped while queued: shed the priciest squads until it fits.
	FactionState& s = fstate(b.owner());
	while (!b.squads.empty() && b.spent > s.wealth)
	{
		std::map<SquadId, SquadProfile>::const_iterator sq = config_.squads.find(b.squads.back());
		b.spent -= sq != config_.squads.end() ? sq->second.cost : 0;
		b.squads.pop_back();
	}
	if (b.squads.empty() || b.spent <= 0) return false;

	AttackOrder order;
	order.battleId = b.id;
	order.attacker = b.attacker;
	order.defender = b.defender;
	order.homeTown = b.homeTown;
	order.targetTown = b.targetTown;
	order.squads = b.squads;
	order.strength = b.spent;
	if (b.role == ROLE_DEFEND)
	{
		const Battle* attack = state_.findBattle(b.answers);
		order.defending = true;
		order.answersHandle = attack ? attack->handle : 0;
	}
	int handle = world_.launchAttack(order);
	if (handle <= 0)
	{
		log_.warn("sim", "Kenshi refused the campaign for " + factionName(b.owner()) +
			(b.role == ROLE_DEFEND ? " defending " : " against ") + townName(b.targetTown));
		return false;
	}
	b.handle = handle;
	b.status = BATTLE_ACTIVE;
	s.wealth = std::max(0.0, s.wealth - b.spent);
	if (b.role == ROLE_DEFEND)
	{
		++state_.stats.counterForces;
		log_.info("sim", factionName(b.defender) + " sends " + squadCount(b.squads.size()) + " (" + toString(b.spent) +
			") from " + townName(b.homeTown) + " to defend " + townName(b.targetTown) + (b.surge ? ", surge" : ""));
		notify(factionName(b.defender) + " is sending troops to defend " + townName(b.targetTown) + ".", b.attacker, b.defender, false);
		return true;
	}
	++state_.stats.battlesLaunched;
	log_.info("sim", factionName(b.attacker) + " attacks " + townName(b.targetTown) + " with " +
		squadCount(b.squads.size()) + " (" + toString(b.spent) + ")" + (b.surge ? ", surge" : ""));
	notify(factionName(b.attacker) + " is raiding " + townName(b.targetTown) + ".", b.attacker, b.defender, false);
	return true;
}

void Simulation::processQueue()
{
	for (size_t i = 0; i < state_.battles.size(); )
	{
		Battle& b = state_.battles[i];
		if (b.status != BATTLE_QUEUED) { ++i; continue; }

		const WorldTown* t = town(b.targetTown);
		if (!world_.atWar(b.attacker, b.defender) || !t || t->owner != b.defender || !pairActive(b.attacker, b.defender))
		{
			++state_.stats.battlesCancelled;
			state_.battles.erase(state_.battles.begin() + i);
			continue;
		}
		if (now_ - b.createdDay > config_.settings.queueExpiryDays)
		{
			++state_.stats.battlesExpired;
			state_.battles.erase(state_.battles.begin() + i);
			continue;
		}
		if (canLaunch(b))
		{
			if (!launch(b))
			{
				++state_.stats.battlesCancelled;
				state_.battles.erase(state_.battles.begin() + i);
				continue;
			}
		}
		++i;
	}
}

void Simulation::planAttacks(double days)
{
	const Settings& set = config_.settings;
	for (size_t i = 0; i < factions_.size(); ++i)
	{
		const FactionId& a = factions_[i].id;
		const FactionProfile* pa = profile(a);
		if (!pa || !isActive(a)) continue;
		if (warSquads_[a].empty() || !townsByOwner_.count(a)) continue;

		// One plan at a time: a faction with an attack waiting in the queue
		// doesn't plan another.
		bool waiting = false;
		for (size_t k = 0; k < state_.battles.size() && !waiting; ++k)
			waiting = state_.battles[k].attacker == a && state_.battles[k].status == BATTLE_QUEUED;
		if (waiting) continue;

		const WorldTown* best = 0;
		double bestScore = 0;
		for (size_t t = 0; t < towns_.size(); ++t)
		{
			const WorldTown& target = towns_[t];
			const FactionId& d = target.owner;
			if (d.empty() || d == a || !world_.atWar(a, d) || !pairActive(a, d)) continue;

			bool targeted = false;
			for (size_t k = 0; k < state_.battles.size() && !targeted; ++k)
				targeted = state_.battles[k].attacker == a && state_.battles[k].targetTown == target.id;
			if (targeted) continue;

			const WorldTown* home = nearestOwnTown(a, target);
			if (!home) continue;
			const FactionProfile* pd = profile(d);
			double maxW = std::max(pa->maxWealth, pd ? pd->maxWealth : pa->maxWealth);
			double in[6];
			in[0] = world_.relation(a, d);
			in[1] = pa->character.opportunistic;
			in[2] = (fstate(a).wealth - fstate(d).wealth) / std::max(1.0, maxW);
			in[3] = config_.townImportance(target.id);
			in[4] = distance(*home, target);
			in[5] = pa->character.dormancy;
			double score = finiteOr(config_.formulas.eval(F_TARGET_SCORE, in), 0);
			if (!best || score > bestScore) { best = &target; bestScore = score; }
		}
		if (!best) continue;

		double in[3] = { bestScore, set.temperature, days };
		double chance = clampd(finiteOr(config_.formulas.eval(F_BATTLE_CHANCE, in), 0), 0, 1);
		if (rng_.chance(chance)) planBattle(a, best->owner, *best);
	}
}

void Simulation::planBattle(const FactionId& attacker, const FactionId& defender, const WorldTown& target)
{
	const FactionProfile* p = profile(attacker);
	const Settings& set = config_.settings;
	FactionState& s = fstate(attacker);
	const std::vector<const SquadProfile*>& squads = warSquads_[attacker];
	if (!p || squads.empty()) return;

	double importance = config_.townImportance(target.id);
	double in[3] = { s.wealth, p->offenseGreed, importance };
	double budget = clampd(finiteOr(config_.formulas.eval(F_OFFENSE_BUDGET, in), 0), 0, s.wealth);

	double surgeChance = p->surgeChance >= 0 ? p->surgeChance : set.surgeChance;
	double surgeMult = p->surgeMultiplier >= 0 ? p->surgeMultiplier : set.surgeMultiplier;
	bool surge = rng_.chance(surgeChance);
	if (surge) budget = std::min(s.wealth, budget * surgeMult);

	double cheapest = squads.front()->cost;
	if (budget < cheapest) return;

	// Spend somewhere between one cheap squad and the full budget, skewed up
	// for important targets and opportunistic factions.
	double skew = 1.0 / (1.0 + importance / 4.0 + std::max(0.0, p->character.opportunistic) / 100.0);
	double spend = cheapest + (budget - cheapest) * std::pow(rng_.uniform(), skew);

	int sizeLimit = offenseCapacity();
	if (p->maxSquads > 0) sizeLimit = std::min(sizeLimit, p->maxSquads);
	if (sizeLimit <= 0) return;

	Battle b;
	b.id = state_.nextBattleId++;
	b.attacker = attacker;
	b.defender = defender;
	b.targetTown = target.id;
	const WorldTown* home = nearestOwnTown(attacker, target);
	b.homeTown = home ? home->id : TownId();
	b.createdDay = now_;
	b.surge = surge;

	double left = spend;
	while ((int)b.squads.size() < sizeLimit)
	{
		size_t affordable = 0;
		while (affordable < squads.size() && squads[affordable]->cost <= left) ++affordable;
		if (!affordable) break;
		// Important targets get the stronger half of what's affordable.
		size_t lo = importance >= 3 && affordable > 1 ? affordable / 2 : 0;
		const SquadProfile* pick = squads[lo + rng_.index((unsigned)(affordable - lo))];
		b.squads.push_back(pick->id);
		b.spent += pick->cost;
		left -= pick->cost;
	}
	if (b.squads.empty()) return;

	if (canLaunch(b))
	{
		if (!launch(b)) { ++state_.stats.battlesCancelled; return; }
	}
	else
	{
		log_.debug("sim", factionName(attacker) + " queues an attack on " + target.name + " (squad cap reached)");
	}
	state_.battles.push_back(b);
}

// A defender sees each attack on its towns and guesses its strength, with
// an error that shrinks as the attackers get closer. If the guess beats the
// garrison plus the counter-forces already sent, it sends another. It looks
// again whenever the error would halve, so a low first guess can still be
// corrected by a second, late counter-force.
void Simulation::defendTowns()
{
	size_t count = state_.battles.size();   // counter-forces sent below are appended
	for (size_t i = 0; i < count; ++i)
	{
		const Battle& b = state_.battles[i];
		if (b.role != ROLE_ATTACK || b.status != BATTLE_ACTIVE) continue;
		const WorldTown* target = town(b.targetTown);
		const FactionProfile* pd = profile(b.defender);
		if (!target || target->owner != b.defender || !pd || !pd->enabled) continue;

		double x = target->x, y = target->y;
		if (!world_.campaignPosition(b.handle, x, y))
		{
			const WorldTown* home = town(b.homeTown);
			if (home) { x = home->x; y = home->y; }
		}
		double in[1] = { distanceToNearestTown(b.defender, x, y) };
		double error = clampd(finiteOr(config_.formulas.eval(F_ESTIMATE_ERROR, in), 0), 0, 0.95);
		if (b.estimate >= 0 && error > b.estimateError * 0.5) continue;

		double estimate = b.spent * rng_.range(1 - error, 1 + error);
		state_.battles[i].estimate = estimate;
		state_.battles[i].estimateError = error;

		double committed = garrisonStrength(*target);
		for (size_t k = 0; k < count; ++k)
		{
			const Battle& d = state_.battles[k];
			if (d.role == ROLE_DEFEND && d.answers == b.id) committed += d.spent;
		}
		double shortfall = estimate - committed;
		log_.debug("sim", factionName(b.defender) + " guesses the attack on " + target->name + " at " + toString(estimate) +
			" (+/-" + toString((int)(error * 100)) + "%), against " + toString(committed) + " on hand");
		if (shortfall > 0)
		{
			Battle attack = b;   // sendCounterForce appends to the list b lives in
			sendCounterForce(attack, *target, shortfall);
		}
	}
}

void Simulation::sendCounterForce(const Battle& attack, const WorldTown& target, double shortfall)
{
	const FactionId& f = attack.defender;
	const FactionProfile* p = profile(f);
	const Settings& set = config_.settings;
	FactionState& s = fstate(f);
	const std::vector<const SquadProfile*>& squads = warSquads_[f];
	if (!p || squads.empty()) return;

	// It walks from the nearest other town; a faction with one town has only its garrison.
	const WorldTown* source = nearestOwnTown(f, target, &target);
	if (!source) return;

	double importance = config_.townImportance(target.id);
	double in[4] = { s.wealth, p->defenceGreed, shortfall, importance };
	double budget = clampd(finiteOr(config_.formulas.eval(F_DEFENSE_BUDGET, in), 0), 0, s.wealth);
	double surgeChance = p->surgeChance >= 0 ? p->surgeChance : set.surgeChance;
	double surgeMult = p->surgeMultiplier >= 0 ? p->surgeMultiplier : set.surgeMultiplier;
	bool surge = rng_.chance(surgeChance);
	if (surge) budget = std::min(s.wealth, budget * surgeMult);
	if (budget < squads.front()->cost) return;

	int room = set.maxWarSquads - state_.activeSquads();
	if (p->maxSquads > 0) room = std::min(room, p->maxSquads - state_.activeSquads(f));
	if (room <= 0) return;

	Battle b;
	b.id = state_.nextBattleId++;
	b.role = ROLE_DEFEND;
	b.answers = attack.id;
	b.attacker = attack.attacker;
	b.defender = f;
	b.homeTown = source->id;
	b.targetTown = target.id;
	b.createdDay = now_;
	b.surge = surge;

	// Fill the budget with the strongest squads that still fit.
	double left = budget;
	while ((int)b.squads.size() < room)
	{
		size_t affordable = 0;
		while (affordable < squads.size() && squads[affordable]->cost <= left) ++affordable;
		if (!affordable) break;
		const SquadProfile* pick = squads[affordable - 1];
		b.squads.push_back(pick->id);
		b.spent += pick->cost;
		left -= pick->cost;
	}
	if (b.squads.empty()) return;
	if (launch(b)) state_.battles.push_back(b);
}

void Simulation::collectResults()
{
	std::vector<CampaignResult> results;
	world_.pollFinishedCampaigns(results);
	for (size_t r = 0; r < results.size(); ++r)
	{
		const CampaignResult& res = results[r];
		size_t idx = state_.battles.size();
		for (size_t i = 0; i < state_.battles.size(); ++i)
			if (state_.battles[i].status == BATTLE_ACTIVE && state_.battles[i].handle == res.handle) idx = i;
		if (idx == state_.battles.size())
		{
			log_.debug("sim", "campaign " + toString(res.handle) + " ended but isn't ours; ignored");
			continue;
		}

		Battle b = state_.battles[idx];
		state_.battles.erase(state_.battles.begin() + idx);

		double survivors = clampd(finiteOr(res.survivors, 0), 0, 1);
		addWealth(b.owner(), b.spent * survivors * kSurvivorRefund);

		// A counter-force's fight is scored with the attack it answered.
		if (b.role == ROLE_DEFEND)
		{
			log_.debug("sim", factionName(b.defender) + "'s counter-force at " + townName(b.targetTown) + " is back (" +
				toString((int)(survivors * 100)) + "% survived)");
			continue;
		}

		if (res.outcome == OUTCOME_CANCELLED)
		{
			++state_.stats.battlesCancelled;
			continue;
		}

		double hit = config_.formulas.weight("relationHitPerBattle", 5);
		changeRelation(b.attacker, b.defender, -hit);

		if (res.outcome == OUTCOME_ATTACKER_WON)
		{
			double in[3] = { fstate(b.defender).wealth, b.spent, config_.townImportance(b.targetTown) };
			double loot = clampd(finiteOr(config_.formulas.eval(F_RAID_LOOT, in), 0), 0, fstate(b.defender).wealth);
			addWealth(b.defender, -loot);
			addWealth(b.attacker, loot * config_.formulas.weight("lootShare", 0.5));
			fstate(b.defender).weariness += 1;
			++state_.stats.attackerWins;
			log_.info("sim", factionName(b.attacker) + " raided " + townName(b.targetTown) + " (loot " + toString(loot) + ")");
			notify(factionName(b.attacker) + " raided " + townName(b.targetTown) + ".", b.attacker, b.defender, false);
		}
		else
		{
			fstate(b.attacker).weariness += 1;
			++state_.stats.defenderWins;
			log_.info("sim", factionName(b.defender) + " drove " + factionName(b.attacker) + " off " + townName(b.targetTown));
			notify(factionName(b.defender) + " drove " + factionName(b.attacker) + " off " + townName(b.targetTown) + ".", b.attacker, b.defender, false);
		}
	}
}

// ---- notifications -------------------------------------------------------------

void Simulation::notify(const std::string& text, const FactionId& a, const FactionId& b, bool major)
{
	NotifyMode mode = config_.settings.notifications;
	if (mode == NOTIFY_OFF) return;
	if (mode != NOTIFY_ALL)
	{
		// "known" and (until the adapter can tell distance) "nearby": only
		// factions the player has met. Wars and peace always get through.
		bool known = fstate(a).met && fstate(b).met;
		if (!known && !major) return;
		if (!known && mode == NOTIFY_KNOWN) return;
	}
	world_.notify(text);
}

// ---- invariants -------------------------------------------------------------------

std::vector<std::string> checkInvariants(const Simulation& sim, const IWorld& world)
{
	std::vector<std::string> bad;
	const Config& cfg = sim.config();
	const WarState& st = sim.state();
	std::vector<WorldFaction> factions;
	world.listFactions(factions);

	for (size_t i = 0; i < factions.size(); ++i)
	{
		const FactionId& a = factions[i].id;
		const FactionProfile* pa = cfg.faction(a);
		for (size_t j = i + 1; j < factions.size(); ++j)
		{
			const FactionId& b = factions[j].id;
			const FactionProfile* pb = cfg.faction(b);
			double r = world.relation(a, b);
			std::string pair = a + "/" + b;
			if (!(r >= kRelationMin && r <= kRelationMax)) bad.push_back("relation out of range: " + pair + " = " + toString(r));
			if (!pa || !pb || !pa->enabled || !pb->enabled) continue;
			if (sim.sworn(a, b, true))
			{
				if (r < kSwornAllyFloor) bad.push_back("sworn allies below +25: " + pair + " = " + toString(r));
				if (world.atWar(a, b)) bad.push_back("sworn allies at war: " + pair);
			}
			if (sim.sworn(a, b, false))
			{
				if (r > kSwornEnemyCeiling) bad.push_back("sworn enemies above -25: " + pair + " = " + toString(r));
				if (world.allied(a, b)) bad.push_back("sworn enemies allied: " + pair);
			}
			if (world.atWar(a, b) && world.allied(a, b)) bad.push_back("at war and allied: " + pair);
			// Vanilla can have wars between factions the player hasn't met;
			// only a war this mod is tracking must involve active factions.
			const PairState* ps = st.findPair(a, b);
			if (world.atWar(a, b) && !sim.pairActive(a, b) && ps && ps->warStartDay >= 0)
				bad.push_back("war involving an inactive or unmet faction: " + pair);
		}
		if (pa && pa->enabled && sim.isActive(a) && sim.warCount(a) > pa->warSlots)
			bad.push_back("more wars than war slots: " + a + " has " + toString(sim.warCount(a)));
	}

	for (std::map<FactionId, FactionState>::const_iterator it = st.factions.begin(); it != st.factions.end(); ++it)
	{
		const FactionProfile* p = cfg.faction(it->first);
		double w = it->second.wealth;
		if (!(w >= 0)) bad.push_back("negative wealth: " + it->first + " = " + toString(w));
		if (p && w > p->maxWealth + 1e-6) bad.push_back("wealth above maxWealth: " + it->first + " = " + toString(w));
		if (!(it->second.weariness >= 0)) bad.push_back("negative weariness: " + it->first);
	}

	if (st.activeSquads() > cfg.settings.maxWarSquads)
		bad.push_back("live war squads " + toString(st.activeSquads()) + " above maxWarSquads " + toString(cfg.settings.maxWarSquads));
	if (st.activeAttackSquads() > sim.offenseCapacity())
		bad.push_back("live attack squads " + toString(st.activeAttackSquads()) + " eat into the defense reserve");
	for (size_t i = 0; i < st.battles.size(); ++i)
	{
		const Battle& b = st.battles[i];
		if (b.spent < 0) bad.push_back("battle with negative spend: " + toString(b.id));
		if (b.status == BATTLE_ACTIVE && b.squads.empty()) bad.push_back("active battle with no squads: " + toString(b.id));
		if (b.status == BATTLE_QUEUED && !sim.pairActive(b.attacker, b.defender))
			bad.push_back("queued battle involving an inactive faction: " + toString(b.id));
		if (b.role == ROLE_DEFEND)
		{
			const Battle* a = st.findBattle(b.answers);
			if (b.status != BATTLE_ACTIVE) bad.push_back("queued counter-force: " + toString(b.id));
			if (!a || a->role != ROLE_ATTACK || a->status != BATTLE_ACTIVE)
				bad.push_back("counter-force " + toString(b.id) + " outlived the attack it answers");
			else if (a->targetTown != b.targetTown || a->defender != b.defender)
				bad.push_back("counter-force " + toString(b.id) + " is not at the attacked town");
		}
		const FactionProfile* p = cfg.faction(b.owner());
		if (p && p->maxSquads > 0 && st.activeSquads(b.owner()) > p->maxSquads)
			bad.push_back("faction squad cap exceeded: " + b.owner());
	}
	return bad;
}

} // namespace fw
