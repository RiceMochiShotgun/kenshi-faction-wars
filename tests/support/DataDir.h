// Loads every JSON file under a FactionWars/ folder into a ConfigLoader.
#ifndef FW_DATA_DIR_H
#define FW_DATA_DIR_H

#include "fw/Config.h"

#include <string>

// Returns the number of files added. Paths are reported relative to `root`.
int addDataDir(fw::ConfigLoader& loader, fw::LayerKind kind, const std::string& root, const std::string& originPrefix);
bool readTextFile(const std::string& path, std::string& out);

#endif
