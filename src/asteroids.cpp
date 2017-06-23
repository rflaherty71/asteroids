const float WORLD_HEIGHT = 15.0f;
const float WORLD_WIDTH  = WORLD_HEIGHT * ((float) WINDOW_WIDTH / (float) WINDOW_HEIGHT);

const float WORLD_LEFT   = -WORLD_WIDTH  / 2.0f;
const float WORLD_RIGHT  =  WORLD_WIDTH  / 2.0f;
const float WORLD_TOP    =  WORLD_HEIGHT / 2.0f;
const float WORLD_BOTTOM = -WORLD_HEIGHT / 2.0f;

Matrix4 world_projection;
Matrix4 gui_projection;

struct Entity;
struct Player;
struct Laser;

enum struct Entity_Type {
    NONE,
    PLAYER,
    LASER
};

char* to_string(Entity_Type entity_type) {
    switch (entity_type) {
        case Entity_Type::NONE:   return "NONE";
        case Entity_Type::PLAYER: return "PLAYER";
        case Entity_Type::LASER:  return "LASER";
    }

    return "INVALID";
}

typedef void Entity_Create(Entity* entity);
typedef void Entity_Destroy(Entity* entity);
typedef void Entity_Update(Entity* entity);

struct Entity {
    int id = -1;
    Entity_Type type = Entity_Type::NONE;
    
    Entity* parent  = NULL;
    Entity* child   = NULL;
    Entity* sibling = NULL;

    Matrix4 transform;

    Vector2 position;
    float   orientation = 0.0f;
    float   scale       = 1.0f;

    Sprite* sprite      = NULL;
    float   sprite_size = 1.0f;
    bool    is_visible  = true;

    Entity_Create*  create  = NULL;
    Entity_Destroy* destroy = NULL;
    Entity_Update*  update  = NULL;

    union {
        void*   derived = NULL;
        Player* player;
        Laser*  laser;
    };
};

Entity root_entity;

Entity* create_entity(Entity_Type type, Entity* parent = &root_entity);
void    destroy_entity(Entity* entity);
Entity* find_entity(int id);

#include "laser.cpp"
#include "player.cpp"

#define entity_storage(Type, type, count)   \
    Type type##_buffer[count];              \
    bool type##_buffer_mask[count]

entity_storage(Entity, entity, 512);
entity_storage(Player, player, 1);
entity_storage(Laser, laser, 32);

#undef entity_storage

int next_entity_id  = 0;
int active_entities = 0;

Entity* create_entity(Entity_Type type, Entity* parent) {
    Entity* entity = NULL;
    for (int i = 0; i < count_of(entity_buffer); i++) {
        if (entity_buffer_mask[i]) continue;
        entity_buffer_mask[i] = true;

        entity = new(&entity_buffer[i]) Entity;
        break;
    }

    assert(entity);

    entity->id   = next_entity_id;
    entity->type = type;

    if (parent != &root_entity) {
        int parent_index = (int) (parent - entity_buffer);

        assert(parent_index < count_of(entity_buffer));
        assert(entity_buffer_mask[parent_index]);
    }

    entity->parent = parent;

    if (entity->parent->child) {
        Entity* child = entity->parent->child;
        while (child->sibling) {
            child = child->sibling;
        }

        child->sibling = entity;
    }
    else {
        entity->parent->child = entity;
    }

    #define create_entity_case(TYPE, Type, type)                    \
        case Entity_Type::TYPE: {                                   \
            entity->create  = create_##type;                        \
            entity->destroy = destroy_##type;                       \
            entity->update  = update_##type;                        \
                                                                    \
            for (int i = 0; i < count_of(type##_buffer); i++) {     \
                if (type##_buffer_mask[i]) continue;                \
                type##_buffer_mask[i] = true;                       \
                                                                    \
                entity->type = new(&type##_buffer[i]) Type;         \
                break;                                              \
            }                                                       \
                                                                    \
            break;                                                  \
        }

    switch (entity->type) {
        case Entity_Type::NONE: {
            break;
        }

        create_entity_case(PLAYER, Player, player);
        create_entity_case(LASER, Laser, laser);

        default: {
            printf("Failed to create entity, unhandled entity type '%s' (%i) specified\n", to_string(entity->type), entity->type);
            assert(false);

            break;
        }
    }

    #undef create_entity_case

    printf("Created entity type '%s' (id: %i)\n", to_string(entity->type), entity->id);

    next_entity_id  += 1;
    active_entities += 1;

    if (entity->create) {
        entity->create(entity);
    }

    return entity;
}

void destroy_entity(Entity* entity) {
    int entity_index = (int) (entity - entity_buffer);

    assert(entity_index < count_of(entity_buffer));
    assert(entity_buffer_mask[entity_index]);

    if (entity->destroy) {
        entity->destroy(entity);
    }

    #define destroy_entity_case(TYPE, type)                             \
        case Entity_Type::TYPE: {                                       \
            int type##_index = (int) (entity->type - type##_buffer);    \
                                                                        \
            assert(type##_index < count_of(type##_buffer));             \
            assert(type##_buffer_mask[type##_index]);                   \
                                                                        \
            type##_buffer_mask[type##_index] = false;                   \
            break;                                                      \
        }

    switch (entity->type) {
        case Entity_Type::NONE: {
            break;
        }

        destroy_entity_case(PLAYER, player);
        destroy_entity_case(LASER, laser);

        default: {
            printf("Failed to destroy entity, unhandled entity type '%s' (%i) specified\n", to_string(entity->type), entity->type);
            break;
        }
    }

    #undef destroy_entity_case

    if (entity->parent->child == entity) {
        entity->parent->child = entity->sibling;
    }
    else {
        Entity* child = entity->parent->child;
        while (child->sibling != entity) {
            child = child->sibling;
        }

        child->sibling = entity->sibling;
    }

    printf("Destroyed entity type '%s', (id: %i)\n", to_string(entity->type), entity->id);

    entity_buffer_mask[entity_index] = false;
    active_entities -= 1;
}

Entity* find_entity(int id) {
    Entity* entity = NULL;
    for (int i = 0; i < count_of(entity_buffer); i++) {
        if (!entity_buffer_mask[i])    continue;
        if (entity_buffer[i].id != id) continue;
        
        entity = &entity_buffer[i];
        break;
    }

    assert(entity);
    return entity;
}

void init_asteroids() {
    world_projection = make_orthographic_matrix(WORLD_LEFT, WORLD_RIGHT, WORLD_TOP, WORLD_BOTTOM);
    gui_projection   = make_orthographic_matrix(0.0f, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f);

    // root_entity.transform = make_identity_matrix();

    create_entity(Entity_Type::PLAYER);
}

void draw_entity_at(Entity* entity, Vector2 position, bool show_collider = true) {
    Matrix4 transform = make_transform_matrix(position, entity->orientation, entity->scale);
    set_transform(&transform);

    draw_sprite(entity->sprite, entity->sprite_size * entity->sprite->aspect, entity->sprite_size);

    // if (show_collider) {
    //     draw_rectangle(entity->sprite->aspect, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, true, false);
    // }
}

void build_entity_hierarchy(Entity* entity) {
    Matrix4 local_transform = make_transform_matrix(entity->position, entity->orientation, entity->scale);
    if (entity->parent) {
        entity->transform = entity->parent->transform * local_transform;
    }
    else {
        entity->transform = local_transform;
    }

    Entity* child = entity->child;
    while (child) {
        build_entity_hierarchy(child);
        child = child->sibling;
    }
}

void draw_entity_hierarchy(Entity* entity, Vector2* layout) {
    Matrix4 transform = make_transform_matrix(*layout);
    set_transform(&transform);

    draw_text("%s", to_string(entity->type));
    layout->y -= font_vertical_advance;

    layout->x += 16.0f;

    Entity* child = entity->child;
    while (child) {
        draw_entity_hierarchy(child, layout);
        child = child->sibling;
    }

    layout->x -= 16.0f;
}

void update_asteroids() {
    for (int i = 0; i < count_of(entity_buffer); i++) {
        if (!entity_buffer_mask[i]) continue;

        Entity* entity = &entity_buffer[i];

        if (entity->update) {
            entity->update(entity);
        }

        if (entity->position.x < WORLD_LEFT)   entity->position.x = WORLD_RIGHT;
        if (entity->position.x > WORLD_RIGHT)  entity->position.x = WORLD_LEFT;
        if (entity->position.y < WORLD_BOTTOM) entity->position.y = WORLD_TOP;
        if (entity->position.y > WORLD_TOP)    entity->position.y = WORLD_BOTTOM;
    }

    build_entity_hierarchy(&root_entity);

    set_projection(&world_projection);

    for (int x = 0; x < 6; x++) {
        for (int y = 0; y < 3; y++) {
            Vector2 position = make_vector2(WORLD_LEFT, WORLD_BOTTOM);
            position += make_vector2(x, y) * 5.0f;

            Matrix4 transform = make_transform_matrix(position);
            set_transform(&transform);

            draw_sprite(&background_sprite, 5.0f, 5.0f, false);
        }
    }

    for (int i = 0; i < count_of(entity_buffer); i++) {
        if (!entity_buffer_mask[i]) continue;

        Entity* entity = &entity_buffer[i];
        if (entity->sprite && entity->is_visible) {
            set_transform(&entity->transform);
            draw_sprite(entity->sprite, entity->sprite_size * entity->sprite->aspect, entity->sprite_size);

            // draw_entity_at(entity, entity->position);

            // float bounds = entity->sprite->width > entity->sprite->height ? entity->sprite->width : entity->sprite->height;

            // if (entity->position.x - WORLD_LEFT <= bounds) {
            //     float x = WORLD_RIGHT + (entity->position.x - WORLD_LEFT);
            //     draw_entity_at(entity, make_vector2(x, entity->position.y));
            // }

            // if (WORLD_RIGHT - entity->position.x <= bounds) {
            //     float x = WORLD_LEFT - (WORLD_RIGHT - entity->position.x);
            //     draw_entity_at(entity, make_vector2(x, entity->position.y));
            // }

            // if (entity->position.y - WORLD_BOTTOM <= bounds) {
            //     float y = WORLD_TOP + (entity->position.y - WORLD_BOTTOM);
            //     draw_entity_at(entity, make_vector2(entity->position.x, y));
            // }

            // if (WORLD_TOP - entity->position.y <= bounds) {
            //     float y = WORLD_BOTTOM - (WORLD_TOP - entity->position.y);
            //     draw_entity_at(entity, make_vector2(entity->position.x, y));
            // }
        }
    }

    set_projection(&gui_projection);

    Vector2 layout = make_vector2(16.0f, WINDOW_HEIGHT - font_height - 16.0f);

    Matrix4 transform = make_transform_matrix(layout);
    set_transform(&transform);
    
    draw_text("%.2f, %.2f, %i", time.now, time.delta * 1000.0f, (int) (1.0f / time.delta));
    layout.y -= font_vertical_advance;

    transform = make_transform_matrix(layout);
    set_transform(&transform);

    draw_text("Active Entities: %i", active_entities);
    layout.y -= font_vertical_advance;
    
    transform = make_transform_matrix(layout);
    set_transform(&transform);

    draw_text("Entity Hierarchy:");
    layout.y -= font_vertical_advance;

    layout.x += 16.0f;

    draw_entity_hierarchy(&root_entity, &layout);

    layout.x -= 16.0f;
}