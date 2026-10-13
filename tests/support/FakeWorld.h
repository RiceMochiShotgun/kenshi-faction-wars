// In-memory IWorld for tests and the headless runner. Campaigns march in a
// straight line at a fixed speed. An attack is resolved when it arrives by
// comparing its strength with the town's garrison plus the counter-forces
// that got there first, with some randomness.
#ifndef FW_FAKE_WORLD_H
#define FW_FAKE_WORLD_H

#include "fw/Random.h"
#include "fw/World.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class FakeWorld : public fw::IWorld
{
public:
	FakeWorld();

	// ---- building the world ----
	void addFaction(const std::string& id, const std::string& name, bool enemyByDefault = false);
	void addTown(const std::string& id, const std::string& name, const std::string& owner, double x, double y,
		int population, bool outpost = false, bool hasHQ = false);
	void addSquad(const std::string& id, const std::string& name, const std::string& faction, int size);
	void meet(const std::string& faction, double onDay = 0);   // player meets it on that day
	void meetAll();

	// ---- driving it ----
	void advance(double days);   // moves time and resolves campaigns that arrived
	void setDay(double d) { day_ = d; }
	void setSeed(uint64_t s) { rng_.setState(s); }
	// Every campaign launched from now on fails to start (handle 0).
	void refuseCampaigns(bool refuse) { refuse_ = refuse; }
	// Resolve this campaign at once with the given outcome. Finishing an
	// attack also ends its counter-forces, with the same outcome.
	void finishCampaign(int handle, fw::CampaignOutcome outcome, double survivors);

	double travelSpeed;          // world units per day
	double garrisonPerResident;  // strength per living resident

	// ---- inspecting it ----
	struct Campaign
	{
		int handle;
		fw::AttackOrder order;
		double departDay, arriveDay;
		double fromX, fromY, toX, toY;
	};
	const std::vector<Campaign>& campaigns() const { return campaigns_; }
	const std::vector<std::string>& messages() const { return messages_; }
	int launched() const { return launched_; }

	// ---- IWorld ----
	virtual double currentDay() const { return day_; }
	virtual void listFactions(std::vector<fw::WorldFaction>& out) const { out = factions_; }
	virtual void listTowns(std::vector<fw::WorldTown>& out) const { out = towns_; }
	virtual void listSquads(std::vector<fw::WorldSquad>& out) const { out = squads_; }
	virtual double relation(const fw::FactionId& a, const fw::FactionId& b) const;
	virtual void setRelation(const fw::FactionId& a, const fw::FactionId& b, double value);
	virtual bool atWar(const fw::FactionId& a, const fw::FactionId& b) const { return war_.count(fw::pairKey(a, b)) != 0; }
	virtual void setAtWar(const fw::FactionId& a, const fw::FactionId& b, bool w);
	virtual bool allied(const fw::FactionId& a, const fw::FactionId& b) const { return allied_.count(fw::pairKey(a, b)) != 0; }
	virtual void setAllied(const fw::FactionId& a, const fw::FactionId& b, bool al);
	virtual void setPeaceTreaty(const fw::FactionId& a, const fw::FactionId& b, bool t);
	virtual bool playerHasMet(const fw::FactionId& f) const;
	virtual int launchAttack(const fw::AttackOrder& order);
	virtual void pollFinishedCampaigns(std::vector<fw::CampaignResult>& out);
	virtual bool campaignPosition(int handle, double& x, double& y) const;
	virtual void notify(const std::string& text) { messages_.push_back(text); }

	bool treaty(const fw::FactionId& a, const fw::FactionId& b) const { return treaty_.count(fw::pairKey(a, b)) != 0; }

private:
	double day_;
	fw::Rng rng_;
	bool refuse_;
	int nextHandle_;
	int launched_;
	std::vector<fw::WorldFaction> factions_;
	std::vector<fw::WorldTown> towns_;
	std::vector<fw::WorldSquad> squads_;
	std::map<std::string, double> relations_;
	std::set<std::string> war_, allied_, treaty_;
	std::map<std::string, double> meetDay_;
	std::vector<Campaign> campaigns_;
	std::vector<fw::CampaignResult> finished_;
	std::vector<std::string> messages_;

	const fw::WorldTown* town(const std::string& id) const;
	void resolve(size_t index);
	void finish(int handle, fw::CampaignOutcome outcome, double survivors);
	void endCounterForces(int attackHandle, fw::CampaignOutcome outcome, double survivors);
};

#endif
