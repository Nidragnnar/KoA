#pragma once
// Fase 17: o que as partes do World (World.cpp e World<Domínio>.cpp) dividem entre si
#include <algorithm>
#include <format>
#include <sstream>

#include <glm/geometric.hpp>
#include <nlohmann/json.hpp>

#include "Engine/Platform/FileSystem.h"
#include "Engine/World/Pathfinding.h"
#include "Game/Exploration/Components.h"
#include "Game/World/World.h"

namespace koa::interno {

inline constexpr int LADO = 16;

// Ponto da base de um objeto no tile (onde ele toca o chão)
inline glm::vec2 baseTile(int x, int y) { return {x * LADO + LADO / 2.0f, y * LADO + LADO - 2.0f}; }

}
