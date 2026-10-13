// Minimal test framework: TEST(name) { CHECK(...); }
#ifndef FW_TEST_FRAMEWORK_H
#define FW_TEST_FRAMEWORK_H

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fwtest {

typedef void (*TestFn)();

struct TestCase
{
	const char* name;
	TestFn fn;
};

std::vector<TestCase>& registry();
int& failures();
void fail(const char* file, int line, const std::string& what);

struct Registrar
{
	Registrar(const char* name, TestFn fn)
	{
		TestCase t;
		t.name = name;
		t.fn = fn;
		registry().push_back(t);
	}
};

int runAll(const char* filter);

} // namespace fwtest

#define TEST(name) \
	static void name(); \
	static fwtest::Registrar name##_registrar(#name, &name); \
	static void name()

#define CHECK(cond) \
	do { if (!(cond)) fwtest::fail(__FILE__, __LINE__, "CHECK(" #cond ")"); } while (0)

#define CHECK_EQ(a, b) \
	do { if (!((a) == (b))) { std::ostringstream fw_ss_; fw_ss_ << "CHECK_EQ(" #a ", " #b "): " << (a) << " != " << (b); fwtest::fail(__FILE__, __LINE__, fw_ss_.str()); } } while (0)

#define CHECK_NEAR(a, b, eps) \
	do { double fw_a_ = (a), fw_b_ = (b); if (!(std::fabs(fw_a_ - fw_b_) <= (eps))) { std::ostringstream fw_ss_; fw_ss_ << "CHECK_NEAR(" #a ", " #b "): " << fw_a_ << " vs " << fw_b_; fwtest::fail(__FILE__, __LINE__, fw_ss_.str()); } } while (0)

#endif
