#include "SampleWorld.h"

void buildSampleWorld(FakeWorld& w, bool staggerMeetings)
{
	w.addFaction("uc", "United Cities");
	w.addFaction("hn", "Holy Nation");
	w.addFaction("shek", "Shek Kingdom");
	w.addFaction("hive", "Western Hive");
	w.addFaction("tech", "Tech Hunters");
	w.addFaction("traders", "Traders Guild");
	w.addFaction("ninjas", "Flotsam Ninjas");
	w.addFaction("antislavers", "Anti-Slavers");
	w.addFaction("dust", "Dust Bandits", true);
	w.addFaction("skin", "Skin Bandits", true);

	w.addTown("heft", "Heft", "uc", 20000, 60000, 90, false, true);
	w.addTown("stoat", "Stoat", "uc", 26000, 52000, 60);
	w.addTown("shobattai", "Sho-Battai", "uc", 30000, 40000, 50);
	w.addTown("brink", "Brink", "uc", 44000, 56000, 40);
	w.addTown("okran", "Okran's Pride", "hn", 40000, 24000, 80, false, true);
	w.addTown("blister", "Blister Hill", "hn", 45000, 30000, 70);
	w.addTown("badteeth", "Bad Teeth", "hn", 35000, 30000, 45);
	w.addTown("stack", "Stack", "hn", 48000, 36000, 40);
	w.addTown("rebirth", "Rebirth", "hn", 38000, 14000, 25, true);
	w.addTown("admag", "Admag", "shek", 62000, 30000, 80, false, true);
	w.addTown("squin", "Squin", "shek", 56000, 40000, 50);
	w.addTown("laststand", "Last Stand", "shek", 58000, 24000, 20, true);
	w.addTown("hivevillage", "Hive Village", "hive", 50000, 46000, 40);
	w.addTown("worldsend", "World's End", "tech", 70000, 50000, 50, false, true);
	w.addTown("flotsamcamp", "Flotsam Village", "ninjas", 66000, 60000, 25, true);
	w.addTown("spring", "Spring", "antislavers", 15000, 20000, 20, true);
	w.addTown("dustcamp", "Dust Bandit Camp", "dust", 30000, 32000, 15, true);
	w.addTown("skinlair", "Skin Bandit Lair", "skin", 76000, 20000, 15, true);

	const char* ids[] = { "uc", "hn", "shek", "hive", "tech", "traders", "ninjas", "antislavers", "dust", "skin" };
	for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i)
	{
		std::string f = ids[i];
		w.addSquad(f + "_militia", f + " militia", f, 4);
		w.addSquad(f + "_patrol", f + " patrol", f, 7);
		w.addSquad(f + "_elite", f + " elite squad", f, 12);
	}

	w.setRelation("hn", "shek", -60);
	w.setRelation("hn", "ninjas", -70);
	w.setRelation("hn", "antislavers", -80);
	w.setRelation("hn", "tech", -40);
	w.setRelation("hn", "uc", 10);
	w.setRelation("uc", "traders", 60);
	w.setRelation("uc", "ninjas", -30);
	w.setRelation("uc", "antislavers", -40);
	w.setRelation("shek", "hive", 20);
	w.setRelation("tech", "uc", 10);
	for (size_t i = 0; i < 8; ++i)
	{
		w.setRelation("dust", ids[i], -60);
		w.setRelation("skin", ids[i], -80);
	}

	if (staggerMeetings)
	{
		w.meet("uc", 0);
		w.meet("hn", 0);
		w.meet("shek", 0);
		w.meet("dust", 5);
		w.meet("traders", 10);
		w.meet("hive", 40);
		w.meet("tech", 90);
		w.meet("ninjas", 150);
		w.meet("antislavers", 200);
		w.meet("skin", 300);
	}
	else
	{
		w.meetAll();
	}
}
