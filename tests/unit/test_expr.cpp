#include "TestFramework.h"
#include "fw/Expr.h"
#include "fw/Formulas.h"
#include "fw/Log.h"

#include <map>

using namespace fw;

namespace {

double run(const std::string& text, double x = 0, double y = 0)
{
	std::vector<std::string> vars;
	vars.push_back("x");
	vars.push_back("y");
	Expression e;
	std::string err;
	if (!e.compile(text, vars, err)) { fwtest::fail(__FILE__, __LINE__, "compile failed: " + text + ": " + err); return -999; }
	double v[2] = { x, y };
	return e.eval(v);
}

bool compiles(const std::string& text, std::string& err)
{
	std::vector<std::string> vars(1, "x");
	Expression e;
	return e.compile(text, vars, err);
}

} // namespace

TEST(expr_arithmetic_and_precedence)
{
	CHECK_NEAR(run("1 + 2 * 3"), 7, 1e-12);
	CHECK_NEAR(run("(1 + 2) * 3"), 9, 1e-12);
	CHECK_NEAR(run("10 / 4"), 2.5, 1e-12);
	CHECK_NEAR(run("7 % 3"), 1, 1e-12);
	CHECK_NEAR(run("2 ^ 3 ^ 2"), 512, 1e-9);     // right-associative
	CHECK_NEAR(run("-2 ^ 2"), -4, 1e-12);        // ^ binds tighter than unary minus
	CHECK_NEAR(run("--3"), 3, 1e-12);
	CHECK_NEAR(run("1.5e2 + .5"), 150.5, 1e-12);
}

TEST(expr_variables)
{
	CHECK_NEAR(run("x * 2 + y", 3, 4), 10, 1e-12);
	CHECK_NEAR(run("x - -y", 3, 4), 7, 1e-12);
	CHECK_NEAR(run("  x\t*\n y ", 3, 4), 12, 1e-12);
}

TEST(expr_comparisons_and_logic)
{
	CHECK_NEAR(run("x < y", 1, 2), 1, 0);
	CHECK_NEAR(run("x >= y", 1, 2), 0, 0);
	CHECK_NEAR(run("x == 1 && y != 3", 1, 2), 1, 0);
	CHECK_NEAR(run("x > 5 || y > 5", 1, 2), 0, 0);
	CHECK_NEAR(run("!x", 0), 1, 0);
	CHECK_NEAR(run("!(x == 0)", 0), 0, 0);
}

TEST(expr_functions)
{
	CHECK_NEAR(run("min(3, 1, 2)"), 1, 0);
	CHECK_NEAR(run("max(3, 1, 2)"), 3, 0);
	CHECK_NEAR(run("clamp(x, 0, 1)", 5), 1, 0);
	CHECK_NEAR(run("clamp(x, 0, 1)", -5), 0, 0);
	CHECK_NEAR(run("abs(-4) + sqrt(9) + pow(2, 3)"), 15, 1e-12);
	CHECK_NEAR(run("floor(2.7) + ceil(2.1) + round(2.5)"), 8, 0);
	CHECK_NEAR(run("if(x > 0, 10, 20)", 1), 10, 0);
	CHECK_NEAR(run("if(x > 0, 10, 20)", -1), 20, 0);
	CHECK_NEAR(run("lerp(0, 10, 0.25)"), 2.5, 1e-12);
	CHECK_NEAR(run("log(exp(2))"), 2, 1e-12);
}

TEST(expr_errors_are_reported)
{
	std::string err;
	CHECK(!compiles("1 +", err));
	CHECK(err.find("ends early") != std::string::npos);
	CHECK(!compiles("x * unknownThing", err));
	CHECK(err.find("unknownThing") != std::string::npos);
	CHECK(!compiles("nosuchfn(1)", err));
	CHECK(!compiles("min()", err));
	CHECK(!compiles("clamp(1, 2)", err));
	CHECK(!compiles("(1 + 2", err));
	CHECK(!compiles("1 2", err));
	CHECK(!compiles("", err));
}

TEST(expr_failed_compile_evaluates_to_zero)
{
	Expression e;
	std::string err;
	std::vector<std::string> vars;
	CHECK(!e.compile("1 +", vars, err));
	CHECK(!e.valid());
	CHECK_NEAR(e.eval(0), 0, 0);
}

TEST(expr_deep_nesting)
{
	std::string text;
	for (int i = 0; i < 100; ++i) text += "(1 + ";
	text += "0";
	for (int i = 0; i < 100; ++i) text += ")";
	CHECK_NEAR(run(text), 100, 0);
}

TEST(formulas_defaults_compile)
{
	Formulas f;
	for (int i = 0; i < F_COUNT; ++i)
		CHECK_EQ(f.text((FormulaId)i), std::string(Formulas::defaultText((FormulaId)i)));
	double in[3] = { 200, 0.25, 2 };
	CHECK_NEAR(f.eval(F_OFFENSE_BUDGET, in), 50, 1e-12);
}

TEST(formulas_override_weight_and_text)
{
	Formulas f;
	Log log;
	std::map<std::string, std::string> texts;
	std::map<std::string, double> weights;
	texts["offenseBudget"] = "wealth * offenseGreed * myBoost";
	weights["myBoost"] = 2;
	f.build(texts, weights, log, "test");
	CHECK_EQ(log.count(LOG_WARNING), 0u);
	double in[3] = { 100, 0.5, 0 };
	CHECK_NEAR(f.eval(F_OFFENSE_BUDGET, in), 100, 1e-12);
	// Defaults are still present alongside new weights.
	CHECK_NEAR(f.weight("w_imp"), 10, 0);
}

TEST(formulas_bad_text_falls_back_to_default)
{
	Formulas f;
	Log log;
	std::map<std::string, std::string> texts;
	texts["offenseBudget"] = "wealth * typoGreed";
	texts["notAFormula"] = "1";
	f.build(texts, std::map<std::string, double>(), log, "formulas.json");
	CHECK_EQ(log.count(LOG_ERROR), 1u);
	CHECK_EQ(log.count(LOG_WARNING), 2u);   // the error, plus the unknown name warning
	CHECK_EQ(f.text(F_OFFENSE_BUDGET), std::string(Formulas::defaultText(F_OFFENSE_BUDGET)));
	double in[3] = { 100, 0.5, 0 };
	CHECK_NEAR(f.eval(F_OFFENSE_BUDGET, in), 50, 1e-12);
}
