#include "DataDir.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace {

void listDir(const std::string& dir, std::vector<std::string>& files, std::vector<std::string>& dirs)
{
#ifdef _WIN32
	_finddata_t fd;
	intptr_t h = _findfirst((dir + "/*").c_str(), &fd);
	if (h == -1) return;
	do
	{
		std::string n = fd.name;
		if (n == "." || n == "..") continue;
		if (fd.attrib & _A_SUBDIR) dirs.push_back(n);
		else files.push_back(n);
	} while (_findnext(h, &fd) == 0);
	_findclose(h);
#else
	DIR* d = opendir(dir.c_str());
	if (!d) return;
	while (dirent* e = readdir(d))
	{
		std::string n = e->d_name;
		if (n == "." || n == "..") continue;
		struct stat st;
		if (stat((dir + "/" + n).c_str(), &st) != 0) continue;
		if (S_ISDIR(st.st_mode)) dirs.push_back(n);
		else files.push_back(n);
	}
	closedir(d);
#endif
	std::sort(files.begin(), files.end());
	std::sort(dirs.begin(), dirs.end());
}

bool endsWith(const std::string& s, const std::string& suffix)
{
	return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int walk(fw::ConfigLoader& loader, fw::LayerKind kind, const std::string& root, const std::string& rel, const std::string& prefix)
{
	std::vector<std::string> files, dirs;
	listDir(rel.empty() ? root : root + "/" + rel, files, dirs);
	int n = 0;
	for (size_t i = 0; i < files.size(); ++i)
	{
		if (!endsWith(files[i], ".json")) continue;
		std::string relPath = rel.empty() ? files[i] : rel + "/" + files[i];
		std::string text;
		if (!readTextFile(root + "/" + relPath, text)) continue;
		loader.addFile(kind, prefix + relPath, relPath, text);
		++n;
	}
	for (size_t i = 0; i < dirs.size(); ++i)
		n += walk(loader, kind, root, rel.empty() ? dirs[i] : rel + "/" + dirs[i], prefix);
	return n;
}

} // namespace

bool readTextFile(const std::string& path, std::string& out)
{
	std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
	if (!in) return false;
	std::ostringstream ss;
	ss << in.rdbuf();
	out = ss.str();
	return true;
}

int addDataDir(fw::ConfigLoader& loader, fw::LayerKind kind, const std::string& root, const std::string& originPrefix)
{
	return walk(loader, kind, root, std::string(), originPrefix);
}
