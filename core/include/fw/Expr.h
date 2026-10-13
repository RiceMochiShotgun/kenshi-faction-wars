// Small expression language for formulas.json.
//
//   numbers, named variables, + - * / % ^, unary - and !,
//   < <= > >= == != && ||, parentheses, and functions:
//   min max clamp abs sqrt pow floor ceil round exp log if lerp
//
// An expression is compiled once against a fixed list of variable names and
// then evaluated many times with an array of values in that order.
#ifndef FW_EXPR_H
#define FW_EXPR_H

#include <string>
#include <vector>

namespace fw {

class Expression
{
public:
	Expression() : maxDepth_(0) {}

	// Returns false and fills `error` if the text doesn't parse or uses a
	// name that isn't in `variables`. On failure the expression is left empty.
	bool compile(const std::string& text, const std::vector<std::string>& variables, std::string& error);

	// `values` must hold one entry per variable name given to compile().
	// An empty (failed) expression evaluates to 0.
	double eval(const double* values) const;

	bool valid() const { return !ops_.empty(); }
	const std::string& text() const { return text_; }

	enum OpCode
	{
		OP_CONST, OP_VAR,
		OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW, OP_NEG, OP_NOT,
		OP_LT, OP_LE, OP_GT, OP_GE, OP_EQ, OP_NE, OP_AND, OP_OR,
		OP_MIN, OP_MAX, OP_CLAMP, OP_ABS, OP_SQRT, OP_POWF, OP_FLOOR, OP_CEIL,
		OP_ROUND, OP_EXP, OP_LOG, OP_IF, OP_LERP
	};

	struct Op
	{
		OpCode code;
		double value;   // OP_CONST
		int arg;        // OP_VAR index, or argument count for min/max
	};

private:
	std::vector<Op> ops_;
	std::string text_;
	int maxDepth_;
};

} // namespace fw

#endif
