#ifndef LUA_COVERAGE_H_
#define LUA_COVERAGE_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <lua.h>
#include "common.h"

struct lua_coverage_file {
	std::string name;
	std::unordered_map<int32_t, uint32_t> line_hits;
};

class lua_coverage_state {
public:
	bool enabled{};
	std::unordered_map<std::string, lua_coverage_file> files;

	static std::string normalize_name(const char* source);
	static void line_hook(lua_State* L, lua_Debug* ar);

	void enable(lua_State* L);
	void install_hook(lua_State* L) const;
	void record(const char* source, int32_t line);
	void clear(const char* name);
	void clear_all();

	int32_t get_dump_size(const char* name) const;
	int32_t dump(const char* name, byte* out_buf, int32_t out_len) const;
	int32_t get_all_dump_size() const;
	int32_t dump_all(byte* out_buf, int32_t out_len) const;
};

#endif /* LUA_COVERAGE_H_ */
