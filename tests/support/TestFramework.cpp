#include "TestFramework.h"

#include <cstring>

namespace fwtest {

std::vector<TestCase>& registry()
{
	static std::vector<TestCase> r;
	return r;
}

int& failures()
{
	static int n = 0;
	return n;
}

void fail(const char* file, int line, const std::string& what)
{
	++failures();
	std::cout << "  FAILED " << file << ":" << line << ": " << what << "\n";
}

int runAll(const char* filter)
{
	int ran = 0, failed = 0;
	for (size_t i = 0; i < registry().size(); ++i)
	{
		const TestCase& t = registry()[i];
		if (filter && !std::strstr(t.name, filter)) continue;
		int before = failures();
		t.fn();
		++ran;
		if (failures() != before) { ++failed; std::cout << "FAIL " << t.name << "\n"; }
	}
	std::cout << ran - failed << "/" << ran << " tests passed\n";
	return failed ? 1 : 0;
}

} // namespace fwtest
