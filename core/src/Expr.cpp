#include "fw/Expr.h"

#include <cmath>
#include <cstdlib>
#include <cctype>

namespace fw {

namespace {

struct FunctionInfo
{
	const char* name;
	Expression::OpCode code;
	int minArgs;
	int maxArgs;   // -1 = any number >= minArgs
};

const FunctionInfo kFunctions[] = {
	{ "min", Expression::OP_MIN, 1, -1 },
	{ "max", Expression::OP_MAX, 1, -1 },
	{ "clamp", Expression::OP_CLAMP, 3, 3 },
	{ "abs", Expression::OP_ABS, 1, 1 },
	{ "sqrt", Expression::OP_SQRT, 1, 1 },
	{ "pow", Expression::OP_POWF, 2, 2 },
	{ "floor", Expression::OP_FLOOR, 1, 1 },
	{ "ceil", Expression::OP_CEIL, 1, 1 },
	{ "round", Expression::OP_ROUND, 1, 1 },
	{ "exp", Expression::OP_EXP, 1, 1 },
	{ "log", Expression::OP_LOG, 1, 1 },
	{ "if", Expression::OP_IF, 3, 3 },
	{ "lerp", Expression::OP_LERP, 3, 3 },
};

class Parser
{
public:
	Parser(const std::string& text, const std::vector<std::string>& vars, std::vector<Expression::Op>& out)
		: s_(text), vars_(vars), out_(out), pos_(0), depth_(0), maxDepth_(0) {}

	bool parse(std::string& error)
	{
		if (!expr()) { error = error_; return false; }
		skipSpace();
		if (pos_ < s_.size()) { error = at("unexpected '" + std::string(1, s_[pos_]) + "'"); return false; }
		return true;
	}

	int maxDepth() const { return maxDepth_; }

private:
	const std::string& s_;
	const std::vector<std::string>& vars_;
	std::vector<Expression::Op>& out_;
	size_t pos_;
	int depth_;
	int maxDepth_;
	std::string error_;

	std::string at(const std::string& msg) const
	{
		std::string p;
		size_t n = pos_ + 1;
		do { p.insert(p.begin(), char('0' + n % 10)); n /= 10; } while (n);
		return msg + " at character " + p;
	}

	bool fail(const std::string& msg) { if (error_.empty()) error_ = at(msg); return false; }

	void emit(Expression::OpCode code, int pops, int pushes, double value = 0, int arg = 0)
	{
		Expression::Op op;
		op.code = code;
		op.value = value;
		op.arg = arg;
		out_.push_back(op);
		depth_ += pushes - pops;
		if (depth_ > maxDepth_) maxDepth_ = depth_;
	}

	void skipSpace() { while (pos_ < s_.size() && std::isspace((unsigned char)s_[pos_])) ++pos_; }

	bool peek(const char* tok)
	{
		skipSpace();
		size_t n = 0;
		while (tok[n]) ++n;
		return s_.compare(pos_, n, tok) == 0;
	}

	bool accept(const char* tok)
	{
		if (!peek(tok)) return false;
		size_t n = 0;
		while (tok[n]) ++n;
		pos_ += n;
		return true;
	}

	bool expr() { return orExpr(); }

	bool orExpr()
	{
		if (!andExpr()) return false;
		while (accept("||")) { if (!andExpr()) return false; emit(Expression::OP_OR, 2, 1); }
		return true;
	}

	bool andExpr()
	{
		if (!cmpExpr()) return false;
		while (accept("&&")) { if (!cmpExpr()) return false; emit(Expression::OP_AND, 2, 1); }
		return true;
	}

	bool cmpExpr()
	{
		if (!addExpr()) return false;
		Expression::OpCode code;
		if (accept("<=")) code = Expression::OP_LE;
		else if (accept(">=")) code = Expression::OP_GE;
		else if (accept("==")) code = Expression::OP_EQ;
		else if (accept("!=")) code = Expression::OP_NE;
		else if (accept("<")) code = Expression::OP_LT;
		else if (accept(">")) code = Expression::OP_GT;
		else return true;
		if (!addExpr()) return false;
		emit(code, 2, 1);
		return true;
	}

	bool addExpr()
	{
		if (!mulExpr()) return false;
		for (;;)
		{
			if (accept("+")) { if (!mulExpr()) return false; emit(Expression::OP_ADD, 2, 1); }
			else if (accept("-")) { if (!mulExpr()) return false; emit(Expression::OP_SUB, 2, 1); }
			else return true;
		}
	}

	bool mulExpr()
	{
		if (!unary()) return false;
		for (;;)
		{
			if (accept("*")) { if (!unary()) return false; emit(Expression::OP_MUL, 2, 1); }
			else if (accept("/")) { if (!unary()) return false; emit(Expression::OP_DIV, 2, 1); }
			else if (accept("%")) { if (!unary()) return false; emit(Expression::OP_MOD, 2, 1); }
			else return true;
		}
	}

	bool unary()
	{
		if (accept("-")) { if (!unary()) return false; emit(Expression::OP_NEG, 1, 1); return true; }
		if (accept("+")) return unary();
		if (peek("!") && !peek("!=")) { ++pos_; if (!unary()) return false; emit(Expression::OP_NOT, 1, 1); return true; }
		return power();
	}

	// '^' binds tighter than unary minus on its left (-2^2 = -4) and is
	// right-associative.
	bool power()
	{
		if (!primary()) return false;
		if (accept("^")) { if (!unary()) return false; emit(Expression::OP_POW, 2, 1); }
		return true;
	}

	bool primary()
	{
		skipSpace();
		if (pos_ >= s_.size()) return fail("expression ends early");
		char c = s_[pos_];

		if (std::isdigit((unsigned char)c) || c == '.')
		{
			const char* begin = s_.c_str() + pos_;
			char* end = 0;
			double v = std::strtod(begin, &end);
			if (end == begin) return fail("bad number");
			pos_ += end - begin;
			emit(Expression::OP_CONST, 0, 1, v);
			return true;
		}

		if (std::isalpha((unsigned char)c) || c == '_')
		{
			size_t start = pos_;
			while (pos_ < s_.size() && (std::isalnum((unsigned char)s_[pos_]) || s_[pos_] == '_')) ++pos_;
			std::string name = s_.substr(start, pos_ - start);
			if (accept("(")) return call(name, start);
			for (size_t i = 0; i < vars_.size(); ++i)
			{
				if (vars_[i] == name) { emit(Expression::OP_VAR, 0, 1, 0, (int)i); return true; }
			}
			pos_ = start;
			return fail("unknown name '" + name + "'");
		}

		if (accept("("))
		{
			if (!expr()) return false;
			if (!accept(")")) return fail("missing ')'");
			return true;
		}

		return fail("unexpected '" + std::string(1, c) + "'");
	}

	bool call(const std::string& name, size_t namePos)
	{
		const FunctionInfo* fn = 0;
		for (size_t i = 0; i < sizeof(kFunctions) / sizeof(kFunctions[0]); ++i)
			if (name == kFunctions[i].name) fn = &kFunctions[i];
		if (!fn) { pos_ = namePos; return fail("unknown function '" + name + "'"); }

		int argc = 0;
		if (!accept(")"))
		{
			for (;;)
			{
				if (!expr()) return false;
				++argc;
				if (accept(")")) break;
				if (!accept(",")) return fail("expected ',' or ')'");
			}
		}
		if (argc < fn->minArgs || (fn->maxArgs >= 0 && argc > fn->maxArgs))
		{
			pos_ = namePos;
			return fail("wrong number of arguments to " + name);
		}
		emit(fn->code, argc, 1, 0, argc);
		return true;
	}
};

inline double truth(bool b) { return b ? 1.0 : 0.0; }

} // namespace

bool Expression::compile(const std::string& text, const std::vector<std::string>& variables, std::string& error)
{
	std::vector<Op> ops;
	Parser parser(text, variables, ops);
	if (!parser.parse(error))
	{
		ops_.clear();
		text_.clear();
		maxDepth_ = 0;
		return false;
	}
	ops_.swap(ops);
	text_ = text;
	maxDepth_ = parser.maxDepth();
	return true;
}

double Expression::eval(const double* values) const
{
	if (ops_.empty()) return 0.0;

	const int kStackSize = 64;
	double fixed[kStackSize];
	std::vector<double> heap;
	double* st = fixed;
	if (maxDepth_ > kStackSize) { heap.resize(maxDepth_); st = &heap[0]; }
	int sp = 0;

	for (size_t i = 0; i < ops_.size(); ++i)
	{
		const Op& op = ops_[i];
		switch (op.code)
		{
		case OP_CONST: st[sp++] = op.value; break;
		case OP_VAR: st[sp++] = values[op.arg]; break;
		case OP_ADD: --sp; st[sp - 1] += st[sp]; break;
		case OP_SUB: --sp; st[sp - 1] -= st[sp]; break;
		case OP_MUL: --sp; st[sp - 1] *= st[sp]; break;
		case OP_DIV: --sp; st[sp - 1] /= st[sp]; break;
		case OP_MOD: --sp; st[sp - 1] = std::fmod(st[sp - 1], st[sp]); break;
		case OP_POW:
		case OP_POWF: --sp; st[sp - 1] = std::pow(st[sp - 1], st[sp]); break;
		case OP_NEG: st[sp - 1] = -st[sp - 1]; break;
		case OP_NOT: st[sp - 1] = truth(st[sp - 1] == 0.0); break;
		case OP_LT: --sp; st[sp - 1] = truth(st[sp - 1] < st[sp]); break;
		case OP_LE: --sp; st[sp - 1] = truth(st[sp - 1] <= st[sp]); break;
		case OP_GT: --sp; st[sp - 1] = truth(st[sp - 1] > st[sp]); break;
		case OP_GE: --sp; st[sp - 1] = truth(st[sp - 1] >= st[sp]); break;
		case OP_EQ: --sp; st[sp - 1] = truth(st[sp - 1] == st[sp]); break;
		case OP_NE: --sp; st[sp - 1] = truth(st[sp - 1] != st[sp]); break;
		case OP_AND: --sp; st[sp - 1] = truth(st[sp - 1] != 0.0 && st[sp] != 0.0); break;
		case OP_OR: --sp; st[sp - 1] = truth(st[sp - 1] != 0.0 || st[sp] != 0.0); break;
		case OP_MIN:
		case OP_MAX:
		{
			int base = sp - op.arg;
			double r = st[base];
			for (int k = base + 1; k < sp; ++k)
			{
				if (op.code == OP_MIN ? st[k] < r : st[k] > r) r = st[k];
			}
			sp = base;
			st[sp++] = r;
			break;
		}
		case OP_CLAMP:
		{
			sp -= 2;
			double x = st[sp - 1], lo = st[sp], hi = st[sp + 1];
			st[sp - 1] = x < lo ? lo : (x > hi ? hi : x);
			break;
		}
		case OP_ABS: st[sp - 1] = std::fabs(st[sp - 1]); break;
		case OP_SQRT: st[sp - 1] = std::sqrt(st[sp - 1]); break;
		case OP_FLOOR: st[sp - 1] = std::floor(st[sp - 1]); break;
		case OP_CEIL: st[sp - 1] = std::ceil(st[sp - 1]); break;
		case OP_ROUND: st[sp - 1] = std::floor(st[sp - 1] + 0.5); break;
		case OP_EXP: st[sp - 1] = std::exp(st[sp - 1]); break;
		case OP_LOG: st[sp - 1] = std::log(st[sp - 1]); break;
		case OP_IF:
		{
			sp -= 2;
			st[sp - 1] = st[sp - 1] != 0.0 ? st[sp] : st[sp + 1];
			break;
		}
		case OP_LERP:
		{
			sp -= 2;
			double a = st[sp - 1], b = st[sp], t = st[sp + 1];
			st[sp - 1] = a + (b - a) * t;
			break;
		}
		}
	}
	return sp > 0 ? st[sp - 1] : 0.0;
}

} // namespace fw
