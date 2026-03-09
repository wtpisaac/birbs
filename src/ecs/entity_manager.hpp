#ifndef BIRBS_ECS_ENTITY_MANAGER_H
#define BIRBS_ECS_ENTITY_MANAGER_H 1

#include <cstdint>

#include "entity.hpp"

class EntityManager {
    public:
        Entity createEntity();

    private:
        uint64_t totalEntities {0};
};

#endif
