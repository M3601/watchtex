#ifndef TEX_HPP
#define TEX_HPP

#pragma once

#include <filesystem>
#include <map>
#include <set>

typedef std::map<std::filesystem::path, std::set<std::filesystem::path>> graph_t;

namespace tex {
void analyze(std::filesystem::path path, graph_t &deps, graph_t &roots);
void build(const std::filesystem::path &path, const graph_t &deps, const graph_t &roots);
} // namespace tex

#endif