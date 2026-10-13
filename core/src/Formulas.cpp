#include "fw/Formulas.h"
#include "fw/Log.h"

namespace fw {

namespace {

struct Def
{
	const char* name;
	const char* inputs;   // comma separated
	const char* text;
};

// Defaults. Placeholder numbers for tuning; data/FactionWars/formulas.json
// mirrors these so players can see and edit them.
const Def kDefs[F_COUNT] = {
	{ "targetScore", "relation,opportunistic,wealthGap,importance,distance,dormancy",
		"w_rel*(-relation) + w_opp*opportunistic*wealthGap + w_imp*importance - w_dist*distance + w_dorm*dormancy" },
	{ "battleChance", "score,temperature,tickDays",
		"clamp(score / 100, 0, 1) * battleRate * temperature * tickDays" },
	{ "offenseBudget", "wealth,offenseGreed,importance",
		"wealth * offenseGreed" },
	{ "defenseBudget", "wealth,defenceGreed,shortfall,importance",
		"min(wealth * defenceGreed, shortfall * defenseMargin * (1 + importance / 10))" },
	{ "joinHelpChance", "friendlyJoin,relationWithSide,distanceFalloff",
		"friendlyJoin * (relationWithSide / 100) * distanceFalloff" },
	{ "joinAttackChance", "hostileJoin,relationWithTarget,distanceFalloff,opportunistic,dormancy",
		"hostileJoin * (-relationWithTarget / 100) * distanceFalloff * (1 + opportunistic / 200) * (1 + dormancy / 200)" },
	{ "estimateError", "distanceToTown",
		"0.05 + 0.4 * min(distanceToTown / 5000, 1)" },
	{ "wealthIncome", "wealth,maxWealth,wealthGrowth,townImportance,townCount,tickDays",
		"wealthGrowth * (1 + 0.25 * townImportance) * max(0, 1 - wealth / maxWealth) * tickDays" },
	{ "relationDrift", "hostility,likelyAlly,likelyEnemy,sharedEnemies,atWar,allied,noise,tickDays",
		"(noise * driftScale - hostility / 100 * 0.3 + likelyAlly * 0.3 - likelyEnemy * 0.3 + sharedEnemies * 0.2 - atWar * 0.2 + allied * 0.2) * tickDays" },
	{ "declareWarChance", "relation,hostility,dormancy,sworn,temperature,tickDays",
		"if(relation < declareThreshold || sworn, warDeclareRate * (1 + hostility / 100) * (1 + dormancy / 200), 0) * temperature * tickDays" },
	{ "peaceChance", "relation,weariness,warDays,sworn,tickDays",
		"if(sworn, 0.2, 1) * peaceRate * (weariness / 5 + warDays / 180 + if(relation > peaceThreshold, 1, 0)) * tickDays" },
	{ "raidLoot", "defenderWealth,spent,importance",
		"min(defenderWealth * 0.1 * (1 + importance / 4), spent * 2)" },
	{ "squadCostEstimate", "size",
		"max(1, size * 2)" },
	{ "townImportanceGuess", "outpost,hasHQ,population",
		"clamp(if(outpost, 0.5, 2) + hasHQ * 1.5 + min(population, 100) / 100, 0, 4.9)" },
	{ "garrisonStrength", "population,importance,outpost",
		"population * residentStrength" },
};

std::vector<std::string> split(const char* list)
{
	std::vector<std::string> out;
	std::string cur;
	for (const char* p = list; ; ++p)
	{
		if (*p == ',' || *p == 0)
		{
			if (!cur.empty()) out.push_back(cur);
			cur.clear();
			if (*p == 0) break;
		}
		else cur += *p;
	}
	return out;
}

} // namespace

std::map<std::string, double> Formulas::defaultWeights()
{
	std::map<std::string, double> w;
	w["w_rel"] = 1.0;
	w["w_opp"] = 0.5;
	w["w_imp"] = 10;
	w["w_dist"] = 0.002;
	w["w_dorm"] = 0.3;
	w["battleRate"] = 0.05;
	w["defenseMargin"] = 1.2;
	w["residentStrength"] = 0.5;
	w["driftScale"] = 1.0;
	w["declareThreshold"] = -50;
	w["peaceThreshold"] = -20;
	w["warDeclareRate"] = 0.02;
	w["peaceRate"] = 0.01;
	w["relationHitPerBattle"] = 5;
	w["lootShare"] = 0.5;
	return w;
}

Formulas::Formulas()
{
	Log ignored;
	build(std::map<std::string, std::string>(), defaultWeights(), ignored, "defaults");
}

const char* Formulas::name(FormulaId id) { return kDefs[id].name; }
const char* Formulas::defaultText(FormulaId id) { return kDefs[id].text; }
std::vector<std::string> Formulas::inputNames(FormulaId id) { return split(kDefs[id].inputs); }

bool Formulas::findByName(const std::string& n, FormulaId& out)
{
	for (int i = 0; i < F_COUNT; ++i)
	{
		if (n == kDefs[i].name) { out = (FormulaId)i; return true; }
	}
	return false;
}

void Formulas::build(const std::map<std::string, std::string>& texts,
	const std::map<std::string, double>& weights, Log& log, const std::string& origin)
{
	// Defaults first, so a formula file that only changes one weight keeps the rest.
	weightValues_ = defaultWeights();
	for (std::map<std::string, double>::const_iterator it = weights.begin(); it != weights.end(); ++it)
		weightValues_[it->first] = it->second;

	weightNames_.clear();
	weightArray_.clear();
	for (std::map<std::string, double>::const_iterator it = weightValues_.begin(); it != weightValues_.end(); ++it)
	{
		weightNames_.push_back(it->first);
		weightArray_.push_back(it->second);
	}

	for (std::map<std::string, std::string>::const_iterator it = texts.begin(); it != texts.end(); ++it)
	{
		FormulaId id;
		if (!findByName(it->first, id))
			log.warn(origin, "unknown formula '" + it->first + "' ignored");
	}

	size_t maxInputs = 0;
	for (int i = 0; i < F_COUNT; ++i)
	{
		FormulaId id = (FormulaId)i;
		std::vector<std::string> vars = inputNames(id);
		inputCount_[i] = vars.size();
		if (vars.size() > maxInputs) maxInputs = vars.size();
		for (size_t w = 0; w < weightNames_.size(); ++w)
		{
			// An input shadows a weight of the same name; never list a name twice.
			bool clash = false;
			for (size_t k = 0; k < vars.size() && !clash; ++k) clash = vars[k] == weightNames_[w];
			vars.push_back(clash ? std::string("~") + weightNames_[w] : weightNames_[w]);
		}

		std::string text = kDefs[i].text;
		bool custom = false;
		std::map<std::string, std::string>::const_iterator t = texts.find(kDefs[i].name);
		if (t != texts.end()) { text = t->second; custom = true; }

		std::string error;
		if (!exprs_[i].compile(text, vars, error))
		{
			if (custom)
				log.error(origin, std::string("formula '") + kDefs[i].name + "': " + error + "; using the built-in default");
			else
				log.error(origin, std::string("built-in formula '") + kDefs[i].name + "' needs a weight that is missing: " + error);
			std::string ignored;
			exprs_[i].compile(kDefs[i].text, vars, ignored);
		}
	}
	scratch_.assign(maxInputs + weightArray_.size(), 0.0);
}

double Formulas::eval(FormulaId id, const double* inputs) const
{
	size_t n = inputCount_[id];
	for (size_t i = 0; i < n; ++i) scratch_[i] = inputs[i];
	for (size_t w = 0; w < weightArray_.size(); ++w) scratch_[n + w] = weightArray_[w];
	return exprs_[id].eval(&scratch_[0]);
}

double Formulas::weight(const std::string& n, double fallback) const
{
	std::map<std::string, double>::const_iterator it = weightValues_.find(n);
	return it == weightValues_.end() ? fallback : it->second;
}

} // namespace fw
