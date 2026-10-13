// RapidJSON, included the same way everywhere in the core.
#ifndef FW_JSON_H
#define FW_JSON_H

// RapidJSON's default spells "MemoryPoolAllocator<::...", which C++03 reads
// as the digraph "<:". Same type, written with a space.
#define RAPIDJSON_DEFAULT_ALLOCATOR ::rapidjson::MemoryPoolAllocator< ::rapidjson::CrtAllocator >

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#endif
