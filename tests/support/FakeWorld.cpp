#include "FakeWorld.h"

#include <algorithm>
#include <cmath>

FakeWorld::FakeWorld()
	: travelSpeed(2000), garrisonPerResident(0.5), day_(0), rng_(12345), refuse_(false), nextHandle_(1), launched_(0)
{
}

void FakeWorld::addFaction(const std::string& id, const std::string& name, bool enemyByDefault)
{
	fw::WorldFaction f;
	f.id = id;
	f.name = name;
	f.enemyByDefault = enemyByDefault;
	factions_.push_back(f);
}

void FakeWorld::addTown(const std::string& id, const std::string& name, const std::string& owner, double x, double y,
	int population, bool outpost, bool hasHQ)
{
	fw::WorldTown t;
	t.id = id;
	t.name = name;
	t.owner = owner;
	t.x = x;
	t.y = y;
	t.population = population;
	t.outpost = outpost;
	t.hasHQ = hasHQ;
	towns_.push_back(t);
}

void FakeWorld::addSquad(const std::string& id, const std::string& name, const std::string& faction, int size)
{
	fw::WorldSquad s;
	s.id = id;
	s.name = name;
	s.faction = faction;
	s.size = size;
	squads_.push_back(s);
}

void FakeWorld::meet(const std::string& faction, double onDay) { meetDay_[faction] = onDay; }

void FakeWorld::meetAll()
{
	for (size_t i = 0; i < factions_.size(); ++i) meetDay_[factions_[i].id] = 0;
}

bool FakeWorld::playerHasMet(const fw::FactionId& f) const
{
	std::map<std::string, double>::const_iterator it = meetDay_.find(f);
	return it != meetDay_.end() && day_ >= it->second;
}

double FakeWorld::relation(const fw::FactionId& a, const fw::FactionId& b) const
{
	if (a == b) return 100;
	std::map<std::string, double>::const_iterator it = relations_.find(fw::pairKey(a, b));
	return it == relations_.end() ? 0 : it->second;
}

void FakeWorld::setRelation(const fw::FactionId& a, const fw::FactionId& b, double value)
{
	relations_[fw::pairKey(a, b)] = value;
}

void FakeWorld::setAtWar(const fw::FactionId& a, const fw::FactionId& b, bool w)
{
	if (w) war_.insert(fw::pairKey(a, b));
	else war_.erase(fw::pairKey(a, b));
}

void FakeWorld::setAllied(const fw::FactionId& a, const fw::FactionId& b, bool al)
{
	if (al) allied_.insert(fw::pairKey(a, b));
	else allied_.erase(fw::pairKey(a, b));
}

void FakeWorld::setPeaceTreaty(const fw::FactionId& a, const fw::FactionId& b, bool t)
{
	if (t) treaty_.insert(fw::pairKey(a, b));
	else treaty_.erase(fw::pairKey(a, b));
}

const fw::WorldTown* FakeWorld::town(const std::string& id) const
{
	for (size_t i = 0; i < towns_.size(); ++i)
		if (towns_[i].id == id) return &towns_[i];
	return 0;
}

int FakeWorld::launchAttack(const fw::AttackOrder& order)
{
	if (refuse_) return 0;
	const fw::WorldTown* home = town(order.homeTown);
	const fw::WorldTown* target = town(order.targetTown);
	if (!home || !target) return 0;
	double dx = home->x - target->x, dy = home->y - target->y;
	Campaign c;
	c.handle = nextHandle_++;
	c.order = order;
	c.arriveDay = day_ + std::max(1.0, std::sqrt(dx * dx + dy * dy) / travelSpeed);
	campaigns_.push_back(c);
	++launched_;
	return c.handle;
}

void FakeWorld::finishCampaign(int handle, fw::CampaignOutcome outcome, double survivors)
{
	for (size_t i = 0; i < campaigns_.size(); ++i)
	{
		if (campaigns_[i].handle != handle) continue;
		fw::CampaignResult r;
		r.handle = handle;
		r.outcome = outcome;
		r.attackerSurvivors = survivors;
		finished_.push_back(r);
		campaigns_.erase(campaigns_.begin() + i);
		return;
	}
}

void FakeWorld::advance(double days)
{
	day_ += days;
	for (size_t i = 0; i < campaigns_.size(); )
	{
		Campaign& c = campaigns_[i];
		if (c.arriveDay > day_) { ++i; continue; }
		const fw::WorldTown* t = town(c.order.targetTown);
		double garrison = t ? t->population * garrisonPerResident : 0;
		double attack = c.order.strength * rng_.range(0.6, 1.4);
		double defense = garrison * rng_.range(0.6, 1.4);
		fw::CampaignResult r;
		r.handle = c.handle;
		if (attack > defense)
		{
			r.outcome = fw::OUTCOME_ATTACKER_WON;
			r.attackerSurvivors = 1.0 - 0.5 * defense / attack;
		}
		else
		{
			r.outcome = fw::OUTCOME_DEFENDER_WON;
			r.attackerSurvivors = 0.5 * attack / std::max(defense, 1e-9) * rng_.uniform();
		}
		finished_.push_back(r);
		campaigns_.erase(campaigns_.begin() + i);
	}
}

void FakeWorld::pollFinishedCampaigns(std::vector<fw::CampaignResult>& out)
{
	out.insert(out.end(), finished_.begin(), finished_.end());
	finished_.clear();
}
