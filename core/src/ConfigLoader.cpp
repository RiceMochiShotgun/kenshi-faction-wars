#include "fw/Config.h"
#include "fw/Log.h"
#include "fw/World.h"

#include "Json.h"

#include <map>
#include <set>

namespace fw {

Settings::Settings()
	: temperature(1.0), tickDays(1.0), factionMeetRequirement(true), showUnmetFactions(false),
	  maxWarSquads(20), defenseReserve(0.25), surgeChance(0.1), surgeMultiplier(1.5),
	  notifications(NOTIFY_NEARBY), debug(false), defaultWarSlots(2), peaceTreatyDays(30),
	  queueExpiryDays(10), seed(0)
{
}

FactionProfile::FactionProfile()
	: enabled(true), ignoreMeetRequirement(false), wealth(100), wealthGrowth(2), maxWealth(1000),
	  offenseGreed(0.2), defenceGreed(0.4), friendlyJoin(0.3), hostileJoin(0.2), warSlots(2),
	  maxSquads(0), surgeChance(-1), surgeMultiplier(-1)
{
}

const FactionProfile* Config::faction(const FactionId& id) const
{
	std::map<FactionId, FactionProfile>::const_iterator it = factions.find(id);
	return it == factions.end() ? 0 : &it->second;
}

double Config::townImportance(const TownId& id) const
{
	std::map<TownId, TownProfile>::const_iterator it = towns.find(id);
	return it == towns.end() ? 0 : it->second.importance;
}

void ConfigLoader::addFile(LayerKind kind, const std::string& origin, const std::string& relPath, const std::string& text)
{
	File f;
	f.kind = kind;
	f.origin = origin;
	f.relPath = relPath;
	f.text = text;
	files_.push_back(f);
}

namespace {

typedef rapidjson::Document Doc;
typedef rapidjson::Value Value;
typedef rapidjson::Document::AllocatorType Alloc;

const int kParseFlags = rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag;

std::string str(const Value& v) { return std::string(v.GetString(), v.GetStringLength()); }

bool startsWith(const std::string& s, const char* prefix)
{
	return s.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

// Resolves FCS string IDs or display names to IDs.
class NameIndex
{
public:
	void add(const std::string& id, const std::string& name, const std::string& group = std::string())
	{
		ids_[id] = group;
		names_.insert(std::make_pair(name, id));
	}

	// `preferGroup` breaks ties between entries with the same display name
	// (e.g. a squad name used by two factions).
	bool resolve(const std::string& key, std::string& out, bool& ambiguous, const std::string& preferGroup = std::string()) const
	{
		ambiguous = false;
		if (ids_.count(key)) { out = key; return true; }
		std::pair<Names::const_iterator, Names::const_iterator> r = names_.equal_range(key);
		std::vector<std::string> hits;
		for (Names::const_iterator it = r.first; it != r.second; ++it) hits.push_back(it->second);
		if (hits.size() > 1 && !preferGroup.empty())
		{
			std::vector<std::string> narrowed;
			for (size_t i = 0; i < hits.size(); ++i)
				if (ids_.find(hits[i])->second == preferGroup) narrowed.push_back(hits[i]);
			if (!narrowed.empty()) hits.swap(narrowed);
		}
		if (hits.size() == 1) { out = hits[0]; return true; }
		ambiguous = hits.size() > 1;
		return false;
	}

private:
	typedef std::multimap<std::string, std::string> Names;
	std::map<std::string, std::string> ids_;
	Names names_;
};

struct Origin
{
	LayerKind kind;
	std::string file;
};

class Builder
{
public:
	Builder(const IWorld& world, Log& log) : log_(log)
	{
		root_.SetObject();
		Alloc& a = root_.GetAllocator();
		const char* sections[] = { "settings", "formulas", "factions", "towns", "squads", "events" };
		for (size_t i = 0; i < sizeof(sections) / sizeof(sections[0]); ++i)
		{
			Value k(sections[i], a);
			Value v(rapidjson::kObjectType);
			root_.AddMember(k, v, a);
		}

		world.listFactions(factions_);
		world.listTowns(towns_);
		world.listSquads(squads_);
		for (size_t i = 0; i < factions_.size(); ++i) factionIndex_.add(factions_[i].id, factions_[i].name);
		for (size_t i = 0; i < towns_.size(); ++i) townIndex_.add(towns_[i].id, towns_[i].name);
		for (size_t i = 0; i < squads_.size(); ++i) squadIndex_.add(squads_[i].id, squads_[i].name, squads_[i].faction);
	}

	void addFile(LayerKind kind, const std::string& origin, const std::string& relPath, const std::string& text)
	{
		Doc doc;
		doc.Parse<kParseFlags>(text.c_str(), text.size());
		if (doc.HasParseError())
		{
			size_t off = doc.GetErrorOffset();
			int line = 1, col = 1;
			for (size_t i = 0; i < off && i < text.size(); ++i)
			{
				if (text[i] == '\n') { ++line; col = 1; }
				else ++col;
			}
			log_.error(origin, "line " + toString(line) + ", column " + toString(col) + ": " +
				rapidjson::GetParseError_En(doc.GetParseError()) + " File skipped.");
			return;
		}

		File f;
		f.kind = kind;
		f.origin = origin;

		if (relPath == "settings.json" || relPath == "formulas.json")
		{
			if (kind == LAYER_ADDON)
			{
				log_.warn(origin, relPath + " is only read from the main mod and the user folder; ignored");
				return;
			}
			if (!doc.IsObject()) { log_.error(origin, "expected an object; file skipped"); return; }
			const char* name = relPath == "settings.json" ? "settings" : "formulas";
			merge(section(name), doc, name, f);
		}
		else if (startsWith(relPath, "factions/")) forEachEntry(doc, f, &Builder::addFaction);
		else if (startsWith(relPath, "towns/")) forEachEntry(doc, f, &Builder::addTowns);
		else if (startsWith(relPath, "squads/")) forEachEntry(doc, f, &Builder::addSquads);
		else if (startsWith(relPath, "events/")) forEachEntry(doc, f, &Builder::addEvent);
		else log_.warn(origin, "unknown file '" + relPath + "'; expected settings.json, formulas.json or a file in factions/, towns/, squads/ or events/");
	}

	void freeze(Config& out)
	{
		out = Config();
		freezeSettings(out.settings);
		freezeFormulas(out.formulas);
		freezeFactions(out);
		freezeTowns(out);
		freezeSquads(out);
		freezeEvents(out);
	}

private:
	struct File
	{
		LayerKind kind;
		std::string origin;
	};

	Log& log_;
	Doc root_;
	std::map<std::string, Origin> origins_;   // "factions/<id>/hostility" -> file that set it
	std::vector<WorldFaction> factions_;
	std::vector<WorldTown> towns_;
	std::vector<WorldSquad> squads_;
	NameIndex factionIndex_, townIndex_, squadIndex_;

	Alloc& alloc() { return root_.GetAllocator(); }

	// Value::operator[] on a missing key needs thread-local storage that
	// clang rejects in C++03; FindMember avoids it. Sections always exist.
	Value& section(const char* name) { return root_.FindMember(name)->value; }

	// ---- merging --------------------------------------------------------

	typedef void (Builder::*EntryFn)(Value& entry, const File& f);

	void forEachEntry(Value& doc, const File& f, EntryFn fn)
	{
		if (doc.IsArray())
		{
			for (Value::ValueIterator it = doc.Begin(); it != doc.End(); ++it)
			{
				if (it->IsObject()) (this->*fn)(*it, f);
				else log_.error(f.origin, "list entries must be objects; entry skipped");
			}
		}
		else if (doc.IsObject()) (this->*fn)(doc, f);
		else log_.error(f.origin, "expected an object or a list of objects; file skipped");
	}

	static bool isListEdit(const Value& v)
	{
		if (!v.IsObject() || v.MemberCount() == 0) return false;
		for (Value::ConstMemberIterator m = v.MemberBegin(); m != v.MemberEnd(); ++m)
		{
			std::string n = str(m->name);
			if ((n != "add" && n != "remove") || !m->value.IsArray()) return false;
		}
		return true;
	}

	void noteOrigin(const std::string& path, const File& f)
	{
		std::map<std::string, Origin>::iterator it = origins_.find(path);
		if (it != origins_.end() && it->second.kind == LAYER_ADDON && f.kind == LAYER_ADDON && it->second.file != f.origin)
			log_.info(f.origin, "overrides '" + path + "' also set by " + it->second.file);
		Origin o;
		o.kind = f.kind;
		o.file = f.origin;
		origins_[path] = o;
	}

	std::string originOf(const std::string& path) const
	{
		// Walk up the path so a whole-object override is found too.
		std::string p = path;
		for (;;)
		{
			std::map<std::string, Origin>::const_iterator it = origins_.find(p);
			if (it != origins_.end()) return it->second.file;
			std::string::size_type slash = p.rfind('/');
			if (slash == std::string::npos) return "defaults";
			p = p.substr(0, slash);
		}
	}

	void applyListEdit(Value& arr, const Value& edit)
	{
		Value::ConstMemberIterator rem = edit.FindMember("remove");
		if (rem != edit.MemberEnd())
		{
			for (Value::ConstValueIterator r = rem->value.Begin(); r != rem->value.End(); ++r)
			{
				for (Value::ValueIterator it = arr.Begin(); it != arr.End(); )
				{
					if (*it == *r) it = arr.Erase(it);
					else ++it;
				}
			}
		}
		Value::ConstMemberIterator add = edit.FindMember("add");
		if (add != edit.MemberEnd())
		{
			for (Value::ConstValueIterator a = add->value.Begin(); a != add->value.End(); ++a)
			{
				bool present = false;
				for (Value::ValueIterator it = arr.Begin(); it != arr.End() && !present; ++it) present = *it == *a;
				if (!present) { Value copy(*a, alloc()); arr.PushBack(copy, alloc()); }
			}
		}
	}

	// Field-by-field merge. Objects recurse, arrays and scalars replace, and
	// {add, remove} objects edit an existing list.
	void merge(Value& dst, const Value& src, const std::string& path, const File& f, const char* skipKey = 0)
	{
		for (Value::ConstMemberIterator m = src.MemberBegin(); m != src.MemberEnd(); ++m)
		{
			std::string name = str(m->name);
			if (skipKey && name == skipKey) continue;
			std::string p = path + "/" + name;
			noteOrigin(p, f);

			Value::MemberIterator d = dst.FindMember(m->name);
			if (isListEdit(m->value))
			{
				if (d == dst.MemberEnd())
				{
					Value k(m->name, alloc());
					Value arr(rapidjson::kArrayType);
					dst.AddMember(k, arr, alloc());
					d = dst.FindMember(m->name);
				}
				else if (!d->value.IsArray())
				{
					d->value.SetArray();
				}
				applyListEdit(d->value, m->value);
			}
			else if (m->value.IsObject() && d != dst.MemberEnd() && d->value.IsObject())
			{
				merge(d->value, m->value, p, f);
			}
			else
			{
				Value copy(m->value, alloc());
				if (d != dst.MemberEnd()) d->value = copy;
				else { Value k(m->name, alloc()); dst.AddMember(k, copy, alloc()); }
			}
		}
	}

	Value& child(Value& parent, const std::string& key)
	{
		Value::MemberIterator it = parent.FindMember(key.c_str());
		if (it != parent.MemberEnd()) return it->value;
		Value k(key.c_str(), (rapidjson::SizeType)key.size(), alloc());
		Value v(rapidjson::kObjectType);
		parent.AddMember(k, v, alloc());
		return parent.FindMember(key.c_str())->value;
	}

	bool resolveFaction(const std::string& key, std::string& id, const std::string& origin)
	{
		bool ambiguous;
		if (factionIndex_.resolve(key, id, ambiguous)) return true;
		if (ambiguous) log_.warn(origin, "faction name '" + key + "' matches more than one faction; use its FCS string ID");
		else log_.info(origin, "faction '" + key + "' is not in this game (its mod may not be active); skipped");
		return false;
	}

	// Turn faction names inside list fields into IDs before merging, so a
	// name in one file and an ID in another refer to the same entry.
	void normalizeFactionList(Value& v, const std::string& origin)
	{
		if (v.IsArray())
		{
			for (Value::ValueIterator it = v.Begin(); it != v.End(); ++it)
			{
				if (!it->IsString()) continue;
				std::string id;
				bool ambiguous;
				if (factionIndex_.resolve(str(*it), id, ambiguous)) it->SetString(id.c_str(), (rapidjson::SizeType)id.size(), alloc());
			}
		}
		else if (isListEdit(v))
		{
			for (Value::MemberIterator m = v.MemberBegin(); m != v.MemberEnd(); ++m) normalizeFactionList(m->value, origin);
		}
	}

	void addFaction(Value& entry, const File& f)
	{
		Value::MemberIterator key = entry.FindMember("faction");
		if (key == entry.MemberEnd() || !key->value.IsString())
		{
			log_.error(f.origin, "faction profile without a \"faction\" name; entry skipped");
			return;
		}
		std::string id;
		if (!resolveFaction(str(key->value), id, f.origin)) return;

		const char* lists[] = { "likelyAllies", "likelyEnemies", "swornAllies", "swornEnemies" };
		for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); ++i)
		{
			Value::MemberIterator m = entry.FindMember(lists[i]);
			if (m != entry.MemberEnd()) normalizeFactionList(m->value, f.origin);
		}
		merge(child(section("factions"), id), entry, "factions/" + id, f, "faction");
	}

	void addTowns(Value& entry, const File& f)
	{
		Value::MemberIterator list = entry.FindMember("towns");
		if (list == entry.MemberEnd() || !list->value.IsObject())
		{
			log_.error(f.origin, "town file needs a \"towns\" object of name: importance; entry skipped");
			return;
		}
		for (Value::MemberIterator m = list->value.MemberBegin(); m != list->value.MemberEnd(); ++m)
		{
			std::string key = str(m->name), id;
			bool ambiguous;
			if (!townIndex_.resolve(key, id, ambiguous))
			{
				if (ambiguous) log_.warn(f.origin, "town name '" + key + "' matches more than one town; use its FCS string ID");
				else log_.info(f.origin, "town '" + key + "' is not in this game; skipped");
				continue;
			}
			Value patch(rapidjson::kObjectType);
			if (m->value.IsNumber())
			{
				Value v(m->value.GetDouble());
				patch.AddMember("importance", v, alloc());
			}
			else if (m->value.IsObject()) patch.CopyFrom(m->value, alloc());
			else { log_.error(f.origin, "town '" + key + "': expected a number or an object"); continue; }
			merge(child(section("towns"), id), patch, "towns/" + id, f);
		}
	}

	void addSquads(Value& entry, const File& f)
	{
		std::string factionId;
		Value::MemberIterator fac = entry.FindMember("faction");
		if (fac != entry.MemberEnd() && fac->value.IsString())
		{
			if (!resolveFaction(str(fac->value), factionId, f.origin)) return;
		}
		Value::MemberIterator list = entry.FindMember("squads");
		if (list == entry.MemberEnd() || !list->value.IsObject())
		{
			log_.error(f.origin, "squad file needs a \"squads\" object of name: cost; entry skipped");
			return;
		}
		for (Value::MemberIterator m = list->value.MemberBegin(); m != list->value.MemberEnd(); ++m)
		{
			std::string key = str(m->name), id;
			bool ambiguous;
			if (!squadIndex_.resolve(key, id, ambiguous, factionId))
			{
				if (ambiguous) log_.warn(f.origin, "squad name '" + key + "' matches more than one squad; add \"faction\" or use its FCS string ID");
				else log_.info(f.origin, "squad '" + key + "' is not in this game; skipped");
				continue;
			}
			Value patch(rapidjson::kObjectType);
			if (m->value.IsNumber())
			{
				Value v(m->value.GetDouble());
				patch.AddMember("cost", v, alloc());
			}
			else if (m->value.IsObject()) patch.CopyFrom(m->value, alloc());
			else { log_.error(f.origin, "squad '" + key + "': expected a number or an object"); continue; }
			merge(child(section("squads"), id), patch, "squads/" + id, f);
		}
	}

	void addEvent(Value& entry, const File& f)
	{
		Value::MemberIterator idv = entry.FindMember("id");
		if (idv == entry.MemberEnd() || !idv->value.IsString())
		{
			log_.error(f.origin, "world event without an \"id\"; entry skipped");
			return;
		}
		std::string id = str(idv->value);
		Value& events = section("events");
		Value::MemberIterator rem = entry.FindMember("remove");
		if (rem != entry.MemberEnd() && rem->value.IsBool() && rem->value.GetBool())
		{
			events.RemoveMember(id.c_str());
			noteOrigin("events/" + id, f);
			return;
		}
		merge(child(events, id), entry, "events/" + id, f, "remove");
	}

	// ---- typed values -----------------------------------------------------

	class Fields
	{
	public:
		Fields(Builder& b, const Value* obj, const std::string& path, const std::string& label)
			: b_(b), obj_(obj), path_(path), label_(label) {}

		void num(const char* name, double& out, double lo, double hi)
		{
			const Value* v = get(name);
			if (!v) return;
			if (!v->IsNumber()) { bad(name, "expected a number"); return; }
			double x = v->GetDouble();
			if (x < lo || x > hi)
			{
				b_.log_.warn(b_.originOf(path_ + "/" + name), label_ + ": " + name + " = " + toString(x) +
					" is outside " + toString(lo) + ".." + toString(hi) + "; clamped");
				x = clampd(x, lo, hi);
			}
			out = x;
		}

		void integer(const char* name, int& out, int lo, int hi)
		{
			double d = out;
			num(name, d, lo, hi);
			out = (int)(d < 0 ? d - 0.5 : d + 0.5);
		}

		void boolean(const char* name, bool& out)
		{
			const Value* v = get(name);
			if (!v) return;
			if (!v->IsBool()) { bad(name, "expected true or false"); return; }
			out = v->GetBool();
		}

		void string(const char* name, std::string& out)
		{
			const Value* v = get(name);
			if (!v) return;
			if (!v->IsString()) { bad(name, "expected text"); return; }
			out = str(*v);
		}

		void factionList(const char* name, std::vector<FactionId>& out)
		{
			const Value* v = get(name);
			if (!v) return;
			if (!v->IsArray()) { bad(name, "expected a list of faction names"); return; }
			out.clear();
			for (Value::ConstValueIterator it = v->Begin(); it != v->End(); ++it)
			{
				if (!it->IsString()) continue;
				std::string id;
				bool ambiguous;
				if (b_.factionIndex_.resolve(str(*it), id, ambiguous)) { if (!contains(out, id)) out.push_back(id); }
				else b_.log_.info(b_.originOf(path_ + "/" + name), label_ + ": " + name + " lists unknown faction '" + str(*it) + "'; ignored");
			}
		}

		const Value* object(const char* name)
		{
			const Value* v = get(name);
			if (v && !v->IsObject()) { bad(name, "expected an object"); return 0; }
			return v;
		}

		void ignore(const char* name) { seen_.insert(name); }

		void warnUnknown()
		{
			if (!obj_) return;
			for (Value::ConstMemberIterator m = obj_->MemberBegin(); m != obj_->MemberEnd(); ++m)
			{
				std::string n = str(m->name);
				if (!seen_.count(n))
					b_.log_.warn(b_.originOf(path_ + "/" + n), label_ + ": unknown field '" + n + "' ignored (typo?)");
			}
		}

	private:
		Builder& b_;
		const Value* obj_;
		std::string path_, label_;
		std::set<std::string> seen_;

		const Value* get(const char* name)
		{
			seen_.insert(name);
			if (!obj_) return 0;
			Value::ConstMemberIterator it = obj_->FindMember(name);
			return it == obj_->MemberEnd() ? 0 : &it->value;
		}

		void bad(const char* name, const char* what)
		{
			b_.log_.error(b_.originOf(path_ + "/" + name), label_ + ": " + name + ": " + what + "; using the default");
		}
	};

	void freezeSettings(Settings& s)
	{
		Fields f(*this, &section("settings"), "settings", "settings.json");
		f.num("temperature", s.temperature, 0, 10);
		f.num("tickDays", s.tickDays, 0.1, 30);
		f.boolean("factionMeetRequirement", s.factionMeetRequirement);
		f.boolean("showUnmetFactions", s.showUnmetFactions);
		f.integer("maxWarSquads", s.maxWarSquads, 0, 1000);
		f.num("defenseReserve", s.defenseReserve, 0, 1);
		f.num("surgeChance", s.surgeChance, 0, 1);
		f.num("surgeMultiplier", s.surgeMultiplier, 1, 10);
		f.boolean("debug", s.debug);
		f.integer("defaultWarSlots", s.defaultWarSlots, 0, 20);
		f.num("peaceTreatyDays", s.peaceTreatyDays, 0, 3650);
		f.num("queueExpiryDays", s.queueExpiryDays, 0, 3650);
		f.integer("seed", s.seed, 0, 2147483647);
		std::string mode;
		f.string("notifications", mode);
		if (mode == "off") s.notifications = NOTIFY_OFF;
		else if (mode == "nearby") s.notifications = NOTIFY_NEARBY;
		else if (mode == "known") s.notifications = NOTIFY_KNOWN;
		else if (mode == "all") s.notifications = NOTIFY_ALL;
		else if (!mode.empty()) log_.warn(originOf("settings/notifications"), "notifications must be off, nearby, known or all; using nearby");
		f.warnUnknown();
	}

	void freezeFormulas(Formulas& formulas)
	{
		std::map<std::string, std::string> texts;
		std::map<std::string, double> weights;
		const Value& obj = section("formulas");
		for (Value::ConstMemberIterator m = obj.MemberBegin(); m != obj.MemberEnd(); ++m)
		{
			std::string n = str(m->name);
			if (n == "weights")
			{
				if (!m->value.IsObject()) { log_.error(originOf("formulas/weights"), "weights must be an object"); continue; }
				for (Value::ConstMemberIterator w = m->value.MemberBegin(); w != m->value.MemberEnd(); ++w)
				{
					if (w->value.IsNumber()) weights[str(w->name)] = w->value.GetDouble();
					else log_.error(originOf("formulas/weights/" + str(w->name)), "weight '" + str(w->name) + "' must be a number");
				}
			}
			else if (m->value.IsString()) texts[n] = str(m->value);
			else log_.error(originOf("formulas/" + n), "formula '" + n + "' must be text");
		}
		formulas.build(texts, weights, log_, originOf("formulas"));
	}

	void freezeFactions(Config& out)
	{
		Value& merged = section("factions");
		for (size_t i = 0; i < factions_.size(); ++i)
		{
			const WorldFaction& wf = factions_[i];
			FactionProfile p;
			p.id = wf.id;
			p.name = wf.name;
			p.warSlots = out.settings.defaultWarSlots;
			// Auto default: factions that are enemies of most others by default lean hostile.
			if (wf.enemyByDefault) { p.character.hostility = 50; p.character.opportunistic = 30; }

			Value::MemberIterator it = merged.FindMember(wf.id.c_str());
			if (it != merged.MemberEnd())
			{
				std::string path = "factions/" + wf.id;
				Fields f(*this, &it->value, path, "faction '" + wf.name + "'");
				f.boolean("enabled", p.enabled);
				f.boolean("ignoreMeetRequirement", p.ignoreMeetRequirement);
				f.num("wealth", p.wealth, 0, 1e9);
				f.num("wealthGrowth", p.wealthGrowth, 0, 1e6);
				f.num("maxWealth", p.maxWealth, 1, 1e9);
				f.num("offenseGreed", p.offenseGreed, 0, 1);
				f.num("defenceGreed", p.defenceGreed, 0, 1);
				f.num("friendlyJoin", p.friendlyJoin, 0, 1);
				f.num("hostileJoin", p.hostileJoin, 0, 1);
				f.integer("warSlots", p.warSlots, 0, 20);
				f.integer("maxWarSquads", p.maxSquads, 0, 1000);
				f.num("surgeChance", p.surgeChance, 0, 1);
				f.num("surgeMultiplier", p.surgeMultiplier, 1, 10);
				f.factionList("likelyAllies", p.likelyAllies);
				f.factionList("likelyEnemies", p.likelyEnemies);
				f.factionList("swornAllies", p.swornAllies);
				f.factionList("swornEnemies", p.swornEnemies);
				f.ignore("faction");
				if (const Value* c = f.object("character"))
				{
					Fields cf(*this, c, path + "/character", "faction '" + wf.name + "' character");
					cf.num("hostility", p.character.hostility, -100, 100);
					cf.num("opportunistic", p.character.opportunistic, -100, 100);
					cf.num("dormancy", p.character.dormancy, -100, 100);
					cf.warnUnknown();
				}
				f.warnUnknown();
			}
			if (p.wealth > p.maxWealth) p.wealth = p.maxWealth;

			// A faction can't be its own ally or enemy, and sworn lists win
			// over each other only by being consistent.
			const char* listNames[] = { "likelyAllies", "likelyEnemies", "swornAllies", "swornEnemies" };
			std::vector<FactionId>* lists[] = { &p.likelyAllies, &p.likelyEnemies, &p.swornAllies, &p.swornEnemies };
			for (size_t l = 0; l < 4; ++l)
			{
				std::vector<FactionId>& v = *lists[l];
				for (size_t k = 0; k < v.size(); )
				{
					if (v[k] == p.id) { log_.warn(originOf("factions/" + p.id + "/" + listNames[l]), "faction '" + p.name + "' lists itself in " + listNames[l] + "; ignored"); v.erase(v.begin() + k); }
					else ++k;
				}
			}
			for (size_t k = 0; k < p.swornAllies.size(); )
			{
				if (contains(p.swornEnemies, p.swornAllies[k]))
				{
					log_.warn(originOf("factions/" + p.id + "/swornAllies"), "faction '" + p.name + "' has '" + p.swornAllies[k] + "' as both sworn ally and sworn enemy; keeping sworn enemy");
					p.swornAllies.erase(p.swornAllies.begin() + k);
				}
				else ++k;
			}
			out.factions[p.id] = p;
		}

		// Sworn relationships are mutual: if either side swears, both are bound.
		for (std::map<FactionId, FactionProfile>::iterator a = out.factions.begin(); a != out.factions.end(); ++a)
		{
			for (size_t k = 0; k < a->second.swornAllies.size(); ++k)
			{
				std::map<FactionId, FactionProfile>::iterator b = out.factions.find(a->second.swornAllies[k]);
				if (b != out.factions.end() && !contains(b->second.swornAllies, a->first) && !contains(b->second.swornEnemies, a->first))
					b->second.swornAllies.push_back(a->first);
			}
			for (size_t k = 0; k < a->second.swornEnemies.size(); ++k)
			{
				std::map<FactionId, FactionProfile>::iterator b = out.factions.find(a->second.swornEnemies[k]);
				if (b != out.factions.end() && !contains(b->second.swornEnemies, a->first))
				{
					b->second.swornEnemies.push_back(a->first);
					std::vector<FactionId>& al = b->second.swornAllies;
					for (size_t j = 0; j < al.size(); ++j) if (al[j] == a->first) { al.erase(al.begin() + j); break; }
				}
			}
		}
	}

	void freezeTowns(Config& out)
	{
		Value& merged = section("towns");
		for (size_t i = 0; i < towns_.size(); ++i)
		{
			const WorldTown& wt = towns_[i];
			TownProfile t;
			t.id = wt.id;
			t.name = wt.name;
			double in[3] = { wt.outpost ? 1.0 : 0.0, wt.hasHQ ? 1.0 : 0.0, (double)wt.population };
			t.importance = clampd(finiteOr(out.formulas.eval(F_TOWN_IMPORTANCE_GUESS, in), 1.0), 0, 4.999);

			Value::MemberIterator it = merged.FindMember(wt.id.c_str());
			if (it != merged.MemberEnd())
			{
				Fields f(*this, &it->value, "towns/" + wt.id, "town '" + wt.name + "'");
				f.num("importance", t.importance, 0, 4.999);
				f.warnUnknown();
			}
			out.towns[t.id] = t;
		}
	}

	void freezeSquads(Config& out)
	{
		Value& merged = section("squads");
		for (size_t i = 0; i < squads_.size(); ++i)
		{
			const WorldSquad& ws = squads_[i];
			SquadProfile s;
			s.id = ws.id;
			s.name = ws.name;
			s.faction = ws.faction;
			double in[1] = { (double)ws.size };
			s.cost = clampd(finiteOr(out.formulas.eval(F_SQUAD_COST_ESTIMATE, in), 1.0), 0.1, 1e5);

			Value::MemberIterator it = merged.FindMember(ws.id.c_str());
			if (it != merged.MemberEnd())
			{
				Fields f(*this, &it->value, "squads/" + ws.id, "squad '" + ws.name + "'");
				f.num("cost", s.cost, 0.1, 1e5);
				f.boolean("war", s.war);
				f.warnUnknown();
			}
			out.squads[s.id] = s;
		}
	}

	void freezeEvents(Config& out)
	{
		Value& merged = section("events");
		for (Value::MemberIterator m = merged.MemberBegin(); m != merged.MemberEnd(); ++m)
		{
			EventDef e;
			e.id = str(m->name);
			Value::MemberIterator w = m->value.FindMember("weight");
			if (w != m->value.MemberEnd() && w->value.IsNumber()) e.weight = w->value.GetDouble();
			rapidjson::StringBuffer buf;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
			m->value.Accept(writer);
			e.json = buf.GetString();
			out.events[e.id] = e;
		}
	}
};

} // namespace

size_t ConfigLoader::build(const IWorld& world, Config& out, Log& log) const
{
	size_t before = log.count(LOG_ERROR);
	Builder b(world, log);
	for (size_t i = 0; i < files_.size(); ++i)
		b.addFile(files_[i].kind, files_[i].origin, files_[i].relPath, files_[i].text);
	b.freeze(out);
	return log.count(LOG_ERROR) - before;
}

} // namespace fw
