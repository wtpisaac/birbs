#include "entity_manager.hpp"

Entity
EntityManager::createEntity() {
    return Entity(
        // increment the total entities, and return the number correspondent to the total
        // for now I don't want there to be an entity with ID of 0, in case no entity is
        // useful to represent
        this->totalEntities++
    );
}
