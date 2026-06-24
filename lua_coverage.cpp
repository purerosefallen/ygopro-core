#include "lua_coverage.h"

#include <algorithm>
#include <cstring>
#include <vector>
#include <lauxlib.h>
#include "buffer.h"
#include "duel.h"
#include "interpreter.h"

namespace {

bool starts_with(const std::string& text, const char* prefix) {
	const size_t len = std::strlen(prefix);
	return text.size() >= len && text.compare(0, len, prefix) == 0;
}

std::vector<std::pair<int32_t, uint32_t>> sorted_hits(const lua_coverage_file& file) {
	std::vector<std::pair<int32_t, uint32_t>> records;
	records.reserve(file.line_hits.size());
	for (const auto& hit : file.line_hits)
		records.emplace_back(hit.first, hit.second);
	std::sort(records.begin(), records.end(), [](const auto& left, const auto& right) {
		return left.first < right.first;
	});
	return records;
}

} // namespace

std::string lua_coverage_state::normalize_name(const char* source) {
	std::string name = source ? source : "";
	if (!name.empty() && name[0] == '@')
		name.erase(0, 1);
	std::replace(name.begin(), name.end(), '\\', '/');
	if (starts_with(name, "./script/"))
		name.erase(0, std::strlen("./script/"));
	else if (starts_with(name, "script/"))
		name.erase(0, std::strlen("script/"));
	else if (starts_with(name, "./"))
		name.erase(0, std::strlen("./"));
	return name;
}

void lua_coverage_state::line_hook(lua_State* L, lua_Debug* ar) {
	if (ar->event != LUA_HOOKLINE || ar->currentline <= 0)
		return;
	duel* pd = interpreter::get_duel_info(L);
	if (!pd || !pd->lua || !pd->lua->lua_coverage.enabled)
		return;
	lua_getinfo(L, "S", ar);
	pd->lua->lua_coverage.record(ar->source, ar->currentline);
}

void lua_coverage_state::enable(lua_State* L) {
	enabled = true;
	install_hook(L);
}

void lua_coverage_state::install_hook(lua_State* L) const {
	if (enabled)
		lua_sethook(L, lua_coverage_state::line_hook, LUA_MASKLINE, 0);
}

void lua_coverage_state::record(const char* source, int32_t line) {
	if (!enabled || line <= 0)
		return;
	auto name = normalize_name(source);
	if (name.empty())
		return;
	auto& file = files[name];
	if (file.name.empty())
		file.name = name;
	auto& count = file.line_hits[line];
	if (count != UINT32_MAX)
		++count;
}

void lua_coverage_state::clear(const char* name) {
	files.erase(normalize_name(name));
}

void lua_coverage_state::clear_all() {
	files.clear();
}

int32_t lua_coverage_state::get_dump_size(const char* name) const {
	if (!enabled)
		return 0;
	const auto it = files.find(normalize_name(name));
	if (it == files.end())
		return 0;
	return static_cast<int32_t>(it->second.line_hits.size() * sizeof(uint32_t) * 2);
}

int32_t lua_coverage_state::dump(const char* name, byte* out_buf, int32_t out_len) const {
	const auto required = get_dump_size(name);
	if (required <= 0)
		return 0;
	if (out_len < required)
		return required;
	const auto it = files.find(normalize_name(name));
	if (it == files.end())
		return 0;
	byte* p = out_buf;
	for (const auto& hit : sorted_hits(it->second)) {
		buffer_write<uint32_t>(p, static_cast<uint32_t>(hit.first));
		buffer_write<uint32_t>(p, hit.second);
	}
	return required;
}

int32_t lua_coverage_state::get_all_dump_size() const {
	if (!enabled || files.empty())
		return 0;
	size_t size = sizeof(uint32_t);
	for (const auto& entry : files) {
		size += sizeof(uint16_t);
		size += entry.first.size();
		size += sizeof(uint32_t);
		size += entry.second.line_hits.size() * sizeof(uint32_t) * 2;
	}
	return static_cast<int32_t>(size);
}

int32_t lua_coverage_state::dump_all(byte* out_buf, int32_t out_len) const {
	const auto required = get_all_dump_size();
	if (required <= 0)
		return 0;
	if (out_len < required)
		return required;

	std::vector<const lua_coverage_file*> ordered;
	ordered.reserve(files.size());
	for (const auto& entry : files)
		ordered.push_back(&entry.second);
	std::sort(ordered.begin(), ordered.end(), [](const auto* left, const auto* right) {
		return left->name < right->name;
	});

	byte* p = out_buf;
	buffer_write<uint32_t>(p, static_cast<uint32_t>(ordered.size()));
	for (const auto* file : ordered) {
		const auto name_length = static_cast<uint16_t>(file->name.size());
		buffer_write<uint16_t>(p, name_length);
		buffer_write_block(p, file->name.data(), name_length);
		const auto hits = sorted_hits(*file);
		buffer_write<uint32_t>(p, static_cast<uint32_t>(hits.size()));
		for (const auto& hit : hits) {
			buffer_write<uint32_t>(p, static_cast<uint32_t>(hit.first));
			buffer_write<uint32_t>(p, hit.second);
		}
	}
	return required;
}
