// Deterministic RNG (xorshift64*). Its state is saved with the game, so a
// reloaded save continues the same sequence.
#ifndef FW_RANDOM_H
#define FW_RANDOM_H

#include <stdint.h>

namespace fw {

class Rng
{
public:
	explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ULL) { setState(seed); }

	void setState(uint64_t s) { state_ = s ? s : 0x9E3779B97F4A7C15ULL; }
	uint64_t state() const { return state_; }

	uint64_t next()
	{
		state_ ^= state_ >> 12;
		state_ ^= state_ << 25;
		state_ ^= state_ >> 27;
		return state_ * 0x2545F4914F6CDD1DULL;
	}

	// Uniform in [0, 1).
	double uniform() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
	double range(double lo, double hi) { return lo + (hi - lo) * uniform(); }
	bool chance(double p) { return uniform() < p; }
	// Uniform integer in [0, n).
	unsigned index(unsigned n) { return n ? (unsigned)(next() % n) : 0; }

private:
	uint64_t state_;
};

} // namespace fw

#endif
