// Shared helpers for the core. C++03 plus what Visual C++ 2010 supports.
#ifndef FW_COMMON_H
#define FW_COMMON_H

#include <string>
#include <vector>
#include <sstream>

namespace fw {

typedef std::string FactionId;
typedef std::string TownId;
typedef std::string SquadId;

// VS2010 has no std::isfinite, so use the IEEE property that NaN != NaN
// and inf - inf is NaN.
inline bool isFinite(double x) { return x == x && (x - x) == (x - x); }

inline double clampd(double x, double lo, double hi)
{
	if (!(x == x)) return lo;
	return x < lo ? lo : (x > hi ? hi : x);
}

inline double finiteOr(double x, double fallback) { return isFinite(x) ? x : fallback; }

template <typename T>
std::string toString(const T& v)
{
	std::ostringstream ss;
	ss << v;
	return ss.str();
}

// Key for an unordered faction pair, so (a, b) and (b, a) share state.
inline std::string pairKey(const FactionId& a, const FactionId& b)
{
	return a < b ? a + "|" + b : b + "|" + a;
}

inline void splitPairKey(const std::string& key, FactionId& a, FactionId& b)
{
	std::string::size_type p = key.find('|');
	a = key.substr(0, p);
	b = p == std::string::npos ? std::string() : key.substr(p + 1);
}

template <typename T>
bool contains(const std::vector<T>& v, const T& x)
{
	for (size_t i = 0; i < v.size(); ++i)
		if (v[i] == x) return true;
	return false;
}

} // namespace fw

#endif
