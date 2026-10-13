// A small vanilla-like world: the major factions, a few minor ones and
// bandits, with towns placed roughly where they sit on Kenshi's map.
// Names match vanilla display names so data/FactionWars files apply to it.
// IDs and numbers are made up.
#ifndef FW_SAMPLE_WORLD_H
#define FW_SAMPLE_WORLD_H

#include "FakeWorld.h"

// `staggerMeetings`: the player meets the majors at once and the rest over
// the first months, to exercise the meet requirement.
void buildSampleWorld(FakeWorld& w, bool staggerMeetings);

#endif
