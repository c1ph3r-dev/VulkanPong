#include "defines.hpp"
#include "logger.hpp"

#include <renderer/shared_render_types.hpp>

uint32_t constexpr MAX_ENTITIES = 100u;

struct Entity
{
    Transform pos;
};

struct GameState
{
    uint32_t entity_count;
    Entity entities[MAX_ENTITIES];
};

internal Entity* create_entity(GameState* state, Transform position)
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

bool init_game(GameState* state)
{
    for(uint32_t i = 0; i < 10; i++)
    {
        for (uint32_t j = 0; j < 10; j++)
        {
            Entity* e = create_entity(state, {i * 120.f, j * 60.f, 70.f, 70.f});
        }
    }

    return true;
}

void update_game(GameState* state)
{
    // Does nothing
    for (uint32_t i = 0; i < state->entity_count; i++)
    {
        Entity* e = &state->entities[i];

        // This is frame rate dependent
        e->pos.x += 0.01f;
    }
    
}
