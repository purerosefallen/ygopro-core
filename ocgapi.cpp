/*
 * interface.cpp
 *
 *  Created on: 2010-5-2
 *	  Author: Argon
 */
#include <cstdio>
#include <cstring>
#include <set>
#include "ocgapi.h"
#include "duel.h"
#include "card.h"
#include "group.h"
#include "effect.h"
#include "field.h"
#include "interpreter.h"
#include "buffer.h"

static uint32_t default_card_reader(uint32_t code, card_data* data) {
	return 0;
}
static uint32_t default_message_handler(intptr_t pduel, uint32_t message_type) {
	return 0;
}
static script_reader sreader = default_script_reader;
static card_reader creader = default_card_reader;
static message_handler mhandler = default_message_handler;
static byte buffer[0x100000];
static std::set<duel*> duel_set;

OCGCORE_API void set_script_reader(script_reader f) {
	sreader = f;
}
OCGCORE_API void set_card_reader(card_reader f) {
	creader = f;
}
OCGCORE_API void set_message_handler(message_handler f) {
	mhandler = f;
}
byte* read_script(const char* script_name, int* len) {
	return sreader(script_name, len);
}
uint32_t read_card(uint32_t code, card_data* data) {
	if (code == TEMP_CARD_ID) {
		data->clear();
		return 0;
	}
	auto res = creader(code, data);
	if(!(data->type & TYPE_TOKEN)) {
		if(data->attack > 0) {
			data->attack *= 10;
		}
		if(data->defense > 0) {
			data->defense *= 10;
		}
	}
	return res;
}
uint32_t handle_message(void* pduel, uint32_t message_type) {
	return mhandler((intptr_t)pduel, message_type);
}
OCGCORE_API byte* default_script_reader(const char* script_name, int* slen) {
	FILE *fp;
	fp = std::fopen(script_name, "rb");
	if (!fp)
		return nullptr;
	size_t len = std::fread(buffer, 1, sizeof buffer, fp);
	std::fclose(fp);
	if (len >= sizeof buffer)
		return nullptr;
	*slen = (int)len;
	return buffer;
}
OCGCORE_API intptr_t create_duel(uint_fast32_t seed) {
	duel* pduel = new duel();
	duel_set.insert(pduel);
	pduel->random.seed(seed);
	pduel->rng_version = 1;
	pduel->lua->preloaded = FALSE;
	return (intptr_t)pduel;
}
OCGCORE_API intptr_t create_duel_v2(uint32_t seed_sequence[]) {
	duel* pduel = new duel();
	duel_set.insert(pduel);
	pduel->random.seed(seed_sequence, SEED_COUNT);
	pduel->rng_version = 2;
	pduel->lua->preloaded = FALSE;
	return (intptr_t)pduel;
}
OCGCORE_API void start_duel(intptr_t pduel, uint32_t options) {
	duel* pd = (duel*)pduel;
	if(!pd->lua->preloaded) {
		pd->lua->preloaded = TRUE;
		pd->lua->call_code_function(0, (char*) "PreloadUds", 0, 0);
	}
	uint16_t duel_rule = options >> 16;
	uint16_t duel_options = options & 0xffff;
	pd->game_field->core.duel_options |= duel_options;
	if (duel_rule >= 1 && duel_rule <= CURRENT_RULE)
		pd->game_field->core.duel_rule = duel_rule;
	else if(options & DUEL_OBSOLETE_RULING)		//provide backward compatibility with replay
		pd->game_field->core.duel_rule = 1;
	if (pd->game_field->core.duel_rule < 1 || pd->game_field->core.duel_rule > CURRENT_RULE)
		pd->game_field->core.duel_rule = CURRENT_RULE;
	if (pd->game_field->core.duel_rule == MASTER_RULE3) {
		pd->game_field->player[0].szone_size = 8;
		pd->game_field->player[1].szone_size = 8;
	}
	pd->game_field->core.shuffle_hand_check[0] = FALSE;
	pd->game_field->core.shuffle_hand_check[1] = FALSE;
	pd->game_field->core.shuffle_deck_check[0] = FALSE;
	pd->game_field->core.shuffle_deck_check[1] = FALSE;
	if(pd->game_field->player[0].start_count > 0)
		pd->game_field->draw(0, REASON_RULE, PLAYER_NONE, 0, pd->game_field->player[0].start_count);
	if(pd->game_field->player[1].start_count > 0)
		pd->game_field->draw(0, REASON_RULE, PLAYER_NONE, 1, pd->game_field->player[1].start_count);
	if(options & DUEL_TAG_MODE) {
		for(int i = 0; i < pd->game_field->player[0].start_count && pd->game_field->player[0].tag_list_main.size(); ++i) {
			card* pcard = pd->game_field->player[0].tag_list_main.back();
			pd->game_field->player[0].tag_list_main.pop_back();
			pd->game_field->player[0].tag_list_hand.push_back(pcard);
			pcard->current.controler = 0;
			pcard->current.location = LOCATION_HAND;
			pcard->current.sequence = (uint8_t)pd->game_field->player[0].tag_list_hand.size() - 1;
			pcard->current.position = POS_FACEDOWN;
		}
		for(int i = 0; i < pd->game_field->player[1].start_count && pd->game_field->player[1].tag_list_main.size(); ++i) {
			card* pcard = pd->game_field->player[1].tag_list_main.back();
			pd->game_field->player[1].tag_list_main.pop_back();
			pd->game_field->player[1].tag_list_hand.push_back(pcard);
			pcard->current.controler = 1;
			pcard->current.location = LOCATION_HAND;
			pcard->current.sequence = (uint8_t)pd->game_field->player[1].tag_list_hand.size() - 1;
			pcard->current.position = POS_FACEDOWN;
		}
	}
	pd->game_field->add_process(PROCESSOR_TURN, 0, 0, 0, 0, 0);
}
OCGCORE_API void end_duel(intptr_t pduel) {
	duel* pd = (duel*)pduel;
	if(duel_set.count(pd)) {
		duel_set.erase(pd);
		delete pd;
	}
}
OCGCORE_API void set_player_info(intptr_t pduel, int32_t playerid, int32_t lp, int32_t startcount, int32_t drawcount) {
	if (!check_playerid(playerid))
		return;
	duel* pd = (duel*)pduel;
	if(lp > 0)
		pd->game_field->player[playerid].lp = lp;
	if(startcount >= 0)
		pd->game_field->player[playerid].start_count = startcount;
	if(drawcount >= 0)
		pd->game_field->player[playerid].draw_count = drawcount;
}
OCGCORE_API void get_log_message(intptr_t pduel, char* buf) {
	duel* pd = (duel*)pduel;
	std::strncpy(buf, pd->strbuffer, sizeof pd->strbuffer - 1);
	buf[sizeof pd->strbuffer - 1] = 0;
}
OCGCORE_API int32_t get_message(intptr_t pduel, byte* buf) {
	int32_t len = ((duel*)pduel)->read_buffer(buf);
	((duel*)pduel)->clear_buffer();
	return len;
}
OCGCORE_API uint32_t process(intptr_t pduel) {
	duel* pd = (duel*)pduel;
	uint32_t result = 0; 
	do {
		result = pd->game_field->process();
	} while ((result & PROCESSOR_BUFFER_LEN) == 0 && (result & PROCESSOR_FLAG) == 0);
	return result;
}
OCGCORE_API void new_card(intptr_t pduel, uint32_t code, uint8_t owner, uint8_t playerid, uint8_t location, uint8_t sequence, uint8_t position) {
	if (!check_playerid(owner) || !check_playerid(playerid))
		return;
	duel* ptduel = (duel*)pduel;
	if(!ptduel->lua->preloaded) {
		ptduel->lua->preloaded = TRUE;
		ptduel->lua->call_code_function(0, (char*) "PreloadUds", 0, 0);
	}
	if(ptduel->game_field->is_location_useable(playerid, location, sequence)) {
		card* pcard = ptduel->new_card(code);
		pcard->owner = owner;
		ptduel->game_field->add_card(playerid, pcard, location, sequence);
		pcard->current.position = position;
		if(!(location & LOCATION_ONFIELD) || (position & POS_FACEUP)) {
			pcard->enable_field_effect(true);
			ptduel->game_field->adjust_instant();
		} if(location & LOCATION_ONFIELD) {
			if(location == LOCATION_MZONE)
				pcard->set_status(STATUS_PROC_COMPLETE, TRUE);
		}
	}
}
OCGCORE_API void new_tag_card(intptr_t pduel, uint32_t code, uint8_t owner, uint8_t location) {
	duel* ptduel = (duel*)pduel;
	if(owner > 1 || !(location & (LOCATION_DECK | LOCATION_EXTRA)))
		return;
	card* pcard = ptduel->new_card(code);
	switch(location) {
	case LOCATION_DECK:
		ptduel->game_field->player[owner].tag_list_main.push_back(pcard);
		pcard->owner = owner;
		pcard->current.controler = owner;
		pcard->current.location = LOCATION_DECK;
		pcard->current.sequence = (uint8_t)ptduel->game_field->player[owner].tag_list_main.size() - 1;
		pcard->current.position = POS_FACEDOWN_DEFENSE;
		break;
	case LOCATION_EXTRA:
		ptduel->game_field->player[owner].tag_list_extra.push_back(pcard);
		pcard->owner = owner;
		pcard->current.controler = owner;
		pcard->current.location = LOCATION_EXTRA;
		pcard->current.sequence = (uint8_t)ptduel->game_field->player[owner].tag_list_extra.size() - 1;
		pcard->current.position = POS_FACEDOWN_DEFENSE;
		break;
	}
}
/**
* @brief Get card information.
* @param buf int32_t array
* @return buffer length in bytes
*/
OCGCORE_API int32_t query_card(intptr_t pduel, uint8_t playerid, uint8_t location, uint8_t sequence, int32_t query_flag, byte* buf, int32_t use_cache) {
	if (!check_playerid(playerid))
		return LEN_FAIL;
	duel* ptduel = (duel*)pduel;
	card* pcard = nullptr;
	location &= 0x7f;
	if (location == LOCATION_MZONE || location == LOCATION_SZONE)
		pcard = ptduel->game_field->get_field_card(playerid, location, sequence);
	else {
		card_vector* lst = nullptr;
		if (location == LOCATION_HAND)
			lst = &ptduel->game_field->player[playerid].list_hand;
		else if (location == LOCATION_GRAVE)
			lst = &ptduel->game_field->player[playerid].list_grave;
		else if (location == LOCATION_REMOVED)
			lst = &ptduel->game_field->player[playerid].list_remove;
		else if (location == LOCATION_EXTRA)
			lst = &ptduel->game_field->player[playerid].list_extra;
		else if (location == LOCATION_DECK)
			lst = &ptduel->game_field->player[playerid].list_main;
		else
			return LEN_FAIL;
		if (sequence >= (int32_t)lst->size())
			return LEN_FAIL;
		pcard = (*lst)[sequence];
	}
	if (pcard) {
		return pcard->get_infos(buf, query_flag, use_cache);
	}
	else {
		buffer_write<int32_t>(buf, LEN_EMPTY);
		return LEN_EMPTY;
	}
}
OCGCORE_API int32_t query_field_count(intptr_t pduel, uint8_t playerid, uint8_t location) {
	duel* ptduel = (duel*)pduel;
	if (!check_playerid(playerid))
		return 0;
	auto& player = ptduel->game_field->player[playerid];
	if(location == LOCATION_HAND)
		return (int32_t)player.list_hand.size();
	if(location == LOCATION_GRAVE)
		return (int32_t)player.list_grave.size();
	if(location == LOCATION_REMOVED)
		return (int32_t)player.list_remove.size();
	if(location == LOCATION_EXTRA)
		return (int32_t)player.list_extra.size();
	if(location == LOCATION_DECK)
		return (int32_t)player.list_main.size();
	if(location == LOCATION_MZONE) {
		int32_t count = 0;
		for(auto& pcard : player.list_mzone)
			if(pcard)
				++count;
		return count;
	}
	if(location == LOCATION_SZONE) {
		int32_t count = 0;
		for(auto& pcard : player.list_szone)
			if(pcard)
				++count;
		return count;
	}
	return 0;
}
OCGCORE_API int32_t query_field_card(intptr_t pduel, uint8_t playerid, uint8_t location, uint32_t query_flag, byte* buf, int32_t use_cache) {
	if (!check_playerid(playerid))
		return LEN_FAIL;
	duel* ptduel = (duel*)pduel;
	auto& player = ptduel->game_field->player[playerid];
	byte* p = buf;
	if(location == LOCATION_MZONE) {
		for(auto& pcard : player.list_mzone) {
			if(pcard) {
				int32_t clen = pcard->get_infos(p, query_flag, use_cache);
				p += clen;
			} else {
				buffer_write<int32_t>(p, LEN_EMPTY);
			}
		}
	}
	else if(location == LOCATION_SZONE) {
		for(auto& pcard : player.list_szone) {
			if(pcard) {
				int32_t clen = pcard->get_infos(p, query_flag, use_cache);
				p += clen;
			} else {
				buffer_write<int32_t>(p, LEN_EMPTY);
			}
		}
	}
	else {
		card_vector* lst = nullptr;
		if(location == LOCATION_HAND)
			lst = &player.list_hand;
		else if(location == LOCATION_GRAVE)
			lst = &player.list_grave;
		else if(location == LOCATION_REMOVED)
			lst = &player.list_remove;
		else if(location == LOCATION_EXTRA)
			lst = &player.list_extra;
		else if(location == LOCATION_DECK)
			lst = &player.list_main;
		else
			return LEN_FAIL;
		for(auto& pcard : *lst) {
			int32_t clen = pcard->get_infos(p, query_flag, use_cache);
			p += clen;
		}
	}
	return (int32_t)(p - buf);
}
OCGCORE_API int32_t query_field_info(intptr_t pduel, byte* buf) {
	duel* ptduel = (duel*)pduel;
	byte* p = buf;
	*p++ = MSG_RELOAD_FIELD;
	*p++ = (uint8_t)ptduel->game_field->core.duel_rule;
	for(int playerid = 0; playerid < 2; ++playerid) {
		auto& player = ptduel->game_field->player[playerid];
		buffer_write<int32_t>(p, player.lp);
		for(auto& pcard : player.list_mzone) {
			if(pcard) {
				*p++ = 1;
				*p++ = pcard->current.position;
				*p++ = (uint8_t)pcard->xyz_materials.size();
			} else {
				*p++ = 0;
			}
		}
		for(auto& pcard : player.list_szone) {
			if(pcard) {
				*p++ = 1;
				*p++ = pcard->current.position;
			} else {
				*p++ = 0;
			}
		}
		*p++ = (uint8_t)player.list_main.size();
		*p++ = (uint8_t)player.list_hand.size();
		*p++ = (uint8_t)player.list_grave.size();
		*p++ = (uint8_t)player.list_remove.size();
		*p++ = (uint8_t)player.list_extra.size();
		*p++ = (uint8_t)player.extra_p_count;
	}
	*p++ = (uint8_t)ptduel->game_field->core.current_chain.size();
	for(const auto& ch : ptduel->game_field->core.current_chain) {
		effect* peffect = ch.triggering_effect;
		buffer_write<uint32_t>(p, peffect->get_handler()->data.code);
		buffer_write<uint32_t>(p, peffect->get_handler()->get_info_location());
		*p++ = ch.triggering_controler;
		*p++ = (uint8_t)ch.triggering_location;
		*p++ = ch.triggering_sequence;
		buffer_write<uint32_t>(p, peffect->description);
	}
	return (int32_t)(p - buf);
}
OCGCORE_API void set_responsei(intptr_t pduel, int32_t value) {
	((duel*)pduel)->set_responsei(value);
}
OCGCORE_API void set_responseb(intptr_t pduel, byte* buf) {
	((duel*)pduel)->set_responseb(buf);
}
OCGCORE_API int32_t preload_script(intptr_t pduel, const char* script_name) {
	return ((duel*)pduel)->lua->load_script(script_name);
}
OCGCORE_API int32_t get_registry_value(intptr_t pduel, const char* key, byte* out_buf) {
	if (!pduel || !key || !out_buf) return -1;

	duel* d = (duel*)pduel;
	auto it = d->registry.find(key);
	if (it == d->registry.end())
		return -1;

	const std::string& val = it->second;
	std::memcpy(out_buf, val.c_str(), val.size());
	return static_cast<int32_t>(val.size());
}
OCGCORE_API void set_registry_value(intptr_t pduel, const char* key, const char* value) {
	if (!pduel || !key) return;
	duel* d = (duel*)pduel;
	if (value) {
		d->registry[key] = value;
	} else {
		d->registry.erase(key);
	}
}
OCGCORE_API int32_t get_registry_keys(intptr_t pduel, byte* out_buf) {
	if (!pduel || !out_buf) return -1;

	duel* d = (duel*)pduel;
	byte* ptr = out_buf;

	for (const auto& pair : d->registry) {
		const std::string& key = pair.first;
		uint16_t len = static_cast<uint16_t>(std::min<size_t>(key.size(), 0xFFFF));

		// 写入长度（2字节，小端序）
		buffer_write(ptr, len);

		std::memcpy(ptr, key.data(), len);
		ptr += len;
	}

	return ptr - out_buf;
}
OCGCORE_API void clear_registry(intptr_t pduel) {
	if (!pduel) return;
	duel* d = (duel*)pduel;
	d->registry.clear();
}

OCGCORE_API int32_t dump_registry(intptr_t pduel, byte* out_buf) {
	if (!pduel || !out_buf) return -1;

	duel* d = (duel*)pduel;
	byte* ptr = out_buf;

	for (const auto& pair : d->registry) {
		const std::string& key = pair.first;
		const std::string& value = pair.second;

		uint16_t key_len = static_cast<uint16_t>(std::min<size_t>(key.size(), 0xFFFF));
		uint16_t val_len = static_cast<uint16_t>(std::min<size_t>(value.size(), 0xFFFF));

		// 写入 key_len 和 val_len（每次调用后 ptr 自动推进）
		buffer_write(ptr, key_len);
		buffer_write(ptr, val_len);

		// 写入 key 内容
		std::memcpy(ptr, key.data(), key_len);
		ptr += key_len;

		// 写入 value 内容
		std::memcpy(ptr, value.data(), val_len);
		ptr += val_len;
	}

	return ptr - out_buf;  // 返回写入的总字节数
}
OCGCORE_API void load_registry(intptr_t pduel, const byte* in_buf, int32_t in_len) {
	if (!pduel || !in_buf || in_len <= 0) return;

	duel* d = (duel*)pduel;

	byte* ptr = const_cast<byte*>(in_buf);  // buffer_read 要求非 const
	byte* end = ptr + in_len;

	while (ptr + 4 <= end) {  // 至少要读取 key_len 和 val_len（2 + 2 字节）
		uint16_t key_len = buffer_read<uint16_t>(ptr);
		uint16_t val_len = buffer_read<uint16_t>(ptr);

		if (ptr + key_len + val_len > end) {
			break;  // 数据不完整，退出
		}

		std::string key(reinterpret_cast<const char*>(ptr), key_len);
		ptr += key_len;

		std::string value(reinterpret_cast<const char*>(ptr), val_len);
		ptr += val_len;

		d->registry[std::move(key)] = std::move(value);
	}
}
