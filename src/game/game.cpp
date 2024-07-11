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

bool Game::init()
{
    for(uint32_t i = 0; i < 10; i++)
    {
        for (uint32_t j = 0; j < 10; j++)
        {
            Entity* e = create_entity({i * 120.f, j * 60.f, 70.f, 70.f});
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
