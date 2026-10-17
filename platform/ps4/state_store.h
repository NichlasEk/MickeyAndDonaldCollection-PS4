#pragma once
#include <string>
#include <vector>
#include <cstdint>
// One checksummed slot per game. Temporary write + sync + rename preserves
// the previous slot if writing fails. Parent directory must already exist.
void ps4_write_state(const std::string& path, const std::vector<uint8_t>& state);
std::vector<uint8_t> ps4_read_state(const std::string& path);
bool ps4_state_exists(const std::string& path);
