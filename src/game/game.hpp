#pragma once

#include "assets/assets.hpp"

#include "defines.hpp"
#include "logger.hpp"

#include <renderer/shared_render_types.hpp>

uint32_t constexpr MAX_ENTITIES = 100u;
uint32_t constexpr MAX_MATERIALS = 100u;

struct Entity
{
    Transform pos;
};

struct Material
{
    AssetTypeID id;
    MaterialData data;
};

struct GameState
{
    uint32_t entity_count;
    Entity entities[MAX_ENTITIES];

    uint32_t material_count;
    Material materials[MAX_MATERIALS];
};

class Game
{
private:
    GameState* state;

public:
    Game() : state(new GameState{}) {}
    ~Game() { delete state; }

    bool init();
    void update();

private:
    Entity* create_entity(Transform position);

public:
    friend uint32_t create_material(GameState* state, AssetTypeID id, vec4 color);
    friend uint32_t get_material(GameState* state, AssetTypeID id, vec4 color);
    friend Material* get_material(GameState* state, uint32_t materialIdx);

public:
    inline GameState* GetGameState() const { return state; }
};

uint32_t create_material(GameState* state, AssetTypeID id, vec4 color);
uint32_t get_material(GameState* state, AssetTypeID id, vec4 color);
Material* get_material(GameState* state, uint32_t materialIdx);
