// Named formulas from formulas.json, each compiled against a fixed list of
// inputs plus every weight. A formula that fails to compile keeps its
// built-in default.
#ifndef FW_FORMULAS_H
#define FW_FORMULAS_H

#include "fw/Expr.h"

#include <map>
#include <string>
#include <vector>

namespace fw {

class Log;

enum FormulaId
{
	F_TARGET_SCORE,
	F_BATTLE_CHANCE,
	F_OFFENSE_BUDGET,
	F_DEFENSE_BUDGET,
	F_JOIN_HELP_CHANCE,
	F_JOIN_ATTACK_CHANCE,
	F_ESTIMATE_ERROR,
	F_WEALTH_INCOME,
	F_RELATION_DRIFT,
	F_DECLARE_WAR_CHANCE,
	F_PEACE_CHANCE,
	F_RAID_LOOT,
	F_SQUAD_COST_ESTIMATE,
	F_TOWN_IMPORTANCE_GUESS,
	F_COUNT
};

class Formulas
{
public:
	Formulas();

	// Rebuild from config: weights first (they are variables in every
	// formula), then texts. Missing names use defaults; bad texts are logged
	// and replaced by their default.
	void build(const std::map<std::string, std::string>& texts,
		const std::map<std::string, double>& weights, Log& log, const std::string& origin);

	// `inputs` holds the formula's inputs in the order given by inputNames().
	double eval(FormulaId id, const double* inputs) const;

	double weight(const std::string& name, double fallback = 0) const;
	const std::map<std::string, double>& weights() const { return weightValues_; }

	static const char* name(FormulaId id);
	static const char* defaultText(FormulaId id);
	static std::vector<std::string> inputNames(FormulaId id);
	static bool findByName(const std::string& name, FormulaId& out);
	static std::map<std::string, double> defaultWeights();

	const std::string& text(FormulaId id) const { return exprs_[id].text(); }

private:
	Expression exprs_[F_COUNT];
	size_t inputCount_[F_COUNT];
	std::vector<std::string> weightNames_;
	std::vector<double> weightArray_;
	std::map<std::string, double> weightValues_;
	mutable std::vector<double> scratch_;
};

} // namespace fw

#endif
