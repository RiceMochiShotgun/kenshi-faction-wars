#include "TestFramework.h"

int main(int argc, char** argv)
{
	return fwtest::runAll(argc > 1 ? argv[1] : 0);
}
