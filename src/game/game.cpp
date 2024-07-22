#include "game.hpp"

Entity* Game::create_entity(Transform position)
{
    Entity* e = nullptr;
    if(state->entity_count < MAX_ENTITIES)
    {
        e = &state->entities[state->entity_count++];
        e->pos = position;
    }
    else
    {
        JONO_ASSERT(0, "Reached Maximum amount of entities");
    }
    return e;
}

uint32_t create_material(GameState* state, AssetTypeID id, vec4 color = {1.0f, 1.0f, 1.0f, 1.0f})
{
    uint32_t materialIdx = INVALID_IDX;

    if(state->material_count < MAX_MATERIALS)
    {
        materialIdx = state->material_count;
        Material* m = &state->materials[state->material_count++];

        m->id = id;
        m->data.color = color;
    }
    else
    {
        JONO_ASSERT(0, "Reached Maximum amount of materials");
    }

    return materialIdx;
}

uint32_t get_material(GameState* state, AssetTypeID id, vec4 color = {1.0f, 1.0f, 1.0f, 1.0f})
{
    uint32_t materialIdx = INVALID_IDX;

    for (uint32_t i = 0; i < state->material_count; i++)
    {
        Material* m = &state->materials[i];

        if(m->id == id && m->data.color == color)
        {
            materialIdx = i;
            break;
        }
    }

    if(materialIdx == INVALID_IDX)
    {
        materialIdx = create_material(state, id, color);
    }
    
    return materialIdx;
}

Material* get_material(GameState* state, uint32_t materialIdx)
{
    Material* m = nullptr;

    if(materialIdx <= MAX_MATERIALS)
    {
        m = &state->materials[materialIdx];
    }
    return m;
}

bool Game::init()
{
    uint32_t counter = 0;
    for(uint32_t i = 0; i < 10; i++)
    {
        for (uint32_t j = 0; j < 10; j++)
        {
            float r = counter / 100.0f;
            float g = 1.0f - r;
            float b = r;
            float a = 1.0f - r;

            Entity* e = create_entity({i * 120.f, j * 60.f, 70.f, 70.f});
            e->pos.materialIdx = create_material(state, ASSET_SPRITE_BALL, {r, g, b, a});

            counter++;
        }
    }

    return true;
}

void Game::update()
{
    // Does nothing
    for (uint32_t i = 0; i < state->entity_count; i++)
    {
        Entity* e = &state->entities[i];

        // This is frame rate dependent
        e->pos.x += 0.01f;
    }
}
