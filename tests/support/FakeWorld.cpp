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
	c.departDay = day_;
	c.arriveDay = day_ + std::max(1.0, std::sqrt(dx * dx + dy * dy) / travelSpeed);
	c.fromX = home->x;
	c.fromY = home->y;
	c.toX = target->x;
	c.toY = target->y;
	campaigns_.push_back(c);
	++launched_;
	return c.handle;
}

bool FakeWorld::campaignPosition(int handle, double& x, double& y) const
{
	for (size_t i = 0; i < campaigns_.size(); ++i)
	{
		const Campaign& c = campaigns_[i];
		if (c.handle != handle) continue;
		double t = std::min(1.0, std::max(0.0, (day_ - c.departDay) / std::max(1e-9, c.arriveDay - c.departDay)));
		x = c.fromX + (c.toX - c.fromX) * t;
		y = c.fromY + (c.toY - c.fromY) * t;
		return true;
	}
	return false;
}

void FakeWorld::finish(int handle, fw::CampaignOutcome outcome, double survivors)
{
	for (size_t i = 0; i < campaigns_.size(); ++i)
	{
		if (campaigns_[i].handle != handle) continue;
		fw::CampaignResult r;
		r.handle = handle;
		r.outcome = outcome;
		r.survivors = survivors;
		finished_.push_back(r);
		campaigns_.erase(campaigns_.begin() + i);
		return;
	}
}

void FakeWorld::endCounterForces(int attackHandle, fw::CampaignOutcome outcome, double survivors)
{
	std::vector<int> handles;
	for (size_t i = 0; i < campaigns_.size(); ++i)
		if (campaigns_[i].order.defending && campaigns_[i].order.answersHandle == attackHandle) handles.push_back(campaigns_[i].handle);
	for (size_t i = 0; i < handles.size(); ++i) finish(handles[i], outcome, survivors);
}

void FakeWorld::finishCampaign(int handle, fw::CampaignOutcome outcome, double survivors)
{
	finish(handle, outcome, survivors);
	endCounterForces(handle, outcome, 1.0);
}

// The attack at `index` has reached its target: fight it out with the
// garrison and every counter-force already there.
void FakeWorld::resolve(size_t index)
{
	Campaign c = campaigns_[index];
	const fw::WorldTown* t = town(c.order.targetTown);
	double defense = t ? t->population * garrisonPerResident : 0;
	std::vector<int> arrived, late;
	for (size_t i = 0; i < campaigns_.size(); ++i)
	{
		const Campaign& d = campaigns_[i];
		if (!d.order.defending || d.order.answersHandle != c.handle) continue;
		if (d.arriveDay <= c.arriveDay) { defense += d.order.strength; arrived.push_back(d.handle); }
		else late.push_back(d.handle);
	}

	double attack = c.order.strength * rng_.range(0.6, 1.4);
	defense *= rng_.range(0.6, 1.4);
	fw::CampaignOutcome outcome;
	double attackers, defenders;
	if (attack > defense)
	{
		outcome = fw::OUTCOME_ATTACKER_WON;
		attackers = 1.0 - 0.5 * defense / attack;
		defenders = 0.5 * defense / attack * rng_.uniform();
	}
	else
	{
		outcome = fw::OUTCOME_DEFENDER_WON;
		attackers = 0.5 * attack / std::max(defense, 1e-9) * rng_.uniform();
		defenders = 1.0 - 0.5 * attack / std::max(defense, 1e-9);
	}
	finish(c.handle, outcome, attackers);
	for (size_t i = 0; i < arrived.size(); ++i) finish(arrived[i], outcome, defenders);
	// Still on the road: they turn back unharmed.
	for (size_t i = 0; i < late.size(); ++i) finish(late[i], fw::OUTCOME_CANCELLED, 1.0);
}

void FakeWorld::advance(double days)
{
	day_ += days;
	// Earliest arrival first, so counter-forces are counted against the
	// attack they were sent for.
	for (;;)
	{
		size_t next = campaigns_.size();
		for (size_t i = 0; i < campaigns_.size(); ++i)
		{
			const Campaign& c = campaigns_[i];
			if (c.order.defending || c.arriveDay > day_) continue;
			if (next == campaigns_.size() || c.arriveDay < campaigns_[next].arriveDay) next = i;
		}
		if (next == campaigns_.size()) break;
		resolve(next);
	}
}

void FakeWorld::pollFinishedCampaigns(std::vector<fw::CampaignResult>& out)
{
	out.insert(out.end(), finished_.begin(), finished_.end());
	finished_.clear();
}
