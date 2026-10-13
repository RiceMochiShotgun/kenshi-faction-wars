#include "fw/WarState.h"
#include "Json.h"

namespace fw {

namespace {

typedef rapidjson::Value Value;
typedef rapidjson::Writer<rapidjson::StringBuffer> Writer;

void key(Writer& w, const char* k) { w.Key(k); }
void putStr(Writer& w, const std::string& s) { w.String(s.c_str(), (rapidjson::SizeType)s.size()); }

double getNum(const Value& o, const char* k, double def)
{
	Value::ConstMemberIterator it = o.FindMember(k);
	return it != o.MemberEnd() && it->value.IsNumber() ? it->value.GetDouble() : def;
}

std::string getStr(const Value& o, const char* k)
{
	Value::ConstMemberIterator it = o.FindMember(k);
	return it != o.MemberEnd() && it->value.IsString() ? std::string(it->value.GetString(), it->value.GetStringLength()) : std::string();
}

bool getBool(const Value& o, const char* k, bool def)
{
	Value::ConstMemberIterator it = o.FindMember(k);
	return it != o.MemberEnd() && it->value.IsBool() ? it->value.GetBool() : def;
}

} // namespace

const PairState* WarState::findPair(const FactionId& a, const FactionId& b) const
{
	std::map<std::string, PairState>::const_iterator it = pairs.find(pairKey(a, b));
	return it == pairs.end() ? 0 : &it->second;
}

int WarState::activeSquads() const
{
	int n = 0;
	for (size_t i = 0; i < battles.size(); ++i)
		if (battles[i].status == BATTLE_ACTIVE) n += (int)battles[i].squads.size();
	return n;
}

int WarState::activeSquads(const FactionId& f) const
{
	int n = 0;
	for (size_t i = 0; i < battles.size(); ++i)
		if (battles[i].status == BATTLE_ACTIVE && battles[i].attacker == f) n += (int)battles[i].squads.size();
	return n;
}

std::string WarState::toJson() const
{
	rapidjson::StringBuffer buf;
	Writer w(buf);
	w.StartObject();
	key(w, "version"); w.Int(kVersion);
	key(w, "initialized"); w.Bool(initialized);
	key(w, "lastTickDay"); w.Double(lastTickDay);
	key(w, "rng"); w.Uint64(rngState);
	key(w, "nextBattleId"); w.Int(nextBattleId);

	key(w, "factions"); w.StartObject();
	for (std::map<FactionId, FactionState>::const_iterator it = factions.begin(); it != factions.end(); ++it)
	{
		w.Key(it->first.c_str(), (rapidjson::SizeType)it->first.size());
		w.StartObject();
		key(w, "wealth"); w.Double(it->second.wealth);
		key(w, "weariness"); w.Double(it->second.weariness);
		key(w, "met"); w.Bool(it->second.met);
		w.EndObject();
	}
	w.EndObject();

	key(w, "pairs"); w.StartObject();
	for (std::map<std::string, PairState>::const_iterator it = pairs.begin(); it != pairs.end(); ++it)
	{
		w.Key(it->first.c_str(), (rapidjson::SizeType)it->first.size());
		w.StartObject();
		key(w, "warStartDay"); w.Double(it->second.warStartDay);
		key(w, "peaceUntil"); w.Double(it->second.peaceUntil);
		w.EndObject();
	}
	w.EndObject();

	key(w, "battles"); w.StartArray();
	for (size_t i = 0; i < battles.size(); ++i)
	{
		const Battle& b = battles[i];
		w.StartObject();
		key(w, "id"); w.Int(b.id);
		key(w, "handle"); w.Int(b.handle);
		key(w, "status"); w.String(b.status == BATTLE_ACTIVE ? "active" : "queued");
		key(w, "attacker"); putStr(w, b.attacker);
		key(w, "defender"); putStr(w, b.defender);
		key(w, "homeTown"); putStr(w, b.homeTown);
		key(w, "targetTown"); putStr(w, b.targetTown);
		key(w, "squads"); w.StartArray();
		for (size_t s = 0; s < b.squads.size(); ++s) putStr(w, b.squads[s]);
		w.EndArray();
		key(w, "spent"); w.Double(b.spent);
		key(w, "createdDay"); w.Double(b.createdDay);
		key(w, "surge"); w.Bool(b.surge);
		w.EndObject();
	}
	w.EndArray();

	key(w, "stats"); w.StartObject();
	key(w, "warsDeclared"); w.Int(stats.warsDeclared);
	key(w, "peacesMade"); w.Int(stats.peacesMade);
	key(w, "battlesLaunched"); w.Int(stats.battlesLaunched);
	key(w, "attackerWins"); w.Int(stats.attackerWins);
	key(w, "defenderWins"); w.Int(stats.defenderWins);
	key(w, "battlesCancelled"); w.Int(stats.battlesCancelled);
	key(w, "battlesExpired"); w.Int(stats.battlesExpired);
	w.EndObject();

	w.EndObject();
	return buf.GetString();
}

bool WarState::fromJson(const std::string& json, std::string& error)
{
	rapidjson::Document d;
	d.Parse(json.c_str(), json.size());
	if (d.HasParseError() || !d.IsObject())
	{
		error = d.HasParseError() ? rapidjson::GetParseError_En(d.GetParseError()) : "not an object";
		return false;
	}
	int version = (int)getNum(d, "version", 0);
	if (version < 1 || version > kVersion)
	{
		error = "unsupported save data version " + toString(version);
		return false;
	}

	WarState s;
	s.initialized = getBool(d, "initialized", false);
	s.lastTickDay = getNum(d, "lastTickDay", 0);
	Value::ConstMemberIterator rng = d.FindMember("rng");
	if (rng != d.MemberEnd() && rng->value.IsUint64()) s.rngState = rng->value.GetUint64();
	s.nextBattleId = (int)getNum(d, "nextBattleId", 1);

	Value::ConstMemberIterator fs = d.FindMember("factions");
	if (fs != d.MemberEnd() && fs->value.IsObject())
	{
		for (Value::ConstMemberIterator m = fs->value.MemberBegin(); m != fs->value.MemberEnd(); ++m)
		{
			if (!m->value.IsObject()) continue;
			FactionState f;
			f.wealth = getNum(m->value, "wealth", 0);
			f.weariness = getNum(m->value, "weariness", 0);
			f.met = getBool(m->value, "met", false);
			s.factions[std::string(m->name.GetString(), m->name.GetStringLength())] = f;
		}
	}

	Value::ConstMemberIterator ps = d.FindMember("pairs");
	if (ps != d.MemberEnd() && ps->value.IsObject())
	{
		for (Value::ConstMemberIterator m = ps->value.MemberBegin(); m != ps->value.MemberEnd(); ++m)
		{
			if (!m->value.IsObject()) continue;
			PairState p;
			p.warStartDay = getNum(m->value, "warStartDay", -1);
			p.peaceUntil = getNum(m->value, "peaceUntil", -1);
			s.pairs[std::string(m->name.GetString(), m->name.GetStringLength())] = p;
		}
	}

	Value::ConstMemberIterator bs = d.FindMember("battles");
	if (bs != d.MemberEnd() && bs->value.IsArray())
	{
		for (Value::ConstValueIterator it = bs->value.Begin(); it != bs->value.End(); ++it)
		{
			if (!it->IsObject()) continue;
			Battle b;
			b.id = (int)getNum(*it, "id", 0);
			b.handle = (int)getNum(*it, "handle", 0);
			b.status = getStr(*it, "status") == "active" ? BATTLE_ACTIVE : BATTLE_QUEUED;
			b.attacker = getStr(*it, "attacker");
			b.defender = getStr(*it, "defender");
			b.homeTown = getStr(*it, "homeTown");
			b.targetTown = getStr(*it, "targetTown");
			Value::ConstMemberIterator sq = it->FindMember("squads");
			if (sq != it->MemberEnd() && sq->value.IsArray())
			{
				for (Value::ConstValueIterator q = sq->value.Begin(); q != sq->value.End(); ++q)
					if (q->IsString()) b.squads.push_back(std::string(q->GetString(), q->GetStringLength()));
			}
			b.spent = getNum(*it, "spent", 0);
			b.createdDay = getNum(*it, "createdDay", 0);
			b.surge = getBool(*it, "surge", false);
			s.battles.push_back(b);
		}
	}

	Value::ConstMemberIterator st = d.FindMember("stats");
	if (st != d.MemberEnd() && st->value.IsObject())
	{
		const Value& o = st->value;
		s.stats.warsDeclared = (int)getNum(o, "warsDeclared", 0);
		s.stats.peacesMade = (int)getNum(o, "peacesMade", 0);
		s.stats.battlesLaunched = (int)getNum(o, "battlesLaunched", 0);
		s.stats.attackerWins = (int)getNum(o, "attackerWins", 0);
		s.stats.defenderWins = (int)getNum(o, "defenderWins", 0);
		s.stats.battlesCancelled = (int)getNum(o, "battlesCancelled", 0);
		s.stats.battlesExpired = (int)getNum(o, "battlesExpired", 0);
	}

	*this = s;
	return true;
}

} // namespace fw
