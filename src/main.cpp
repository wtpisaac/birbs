#include <quill/LogMacros.h>

#include "core/logger.hpp"
#include "ecs/entity.hpp"
#include "ecs/entity_manager.hpp"

int main()
{
    QUILL_LOG_INFO(
        BIRBS_LOGGER,
        "Hello, World!"
    );
    EntityManager manager = EntityManager();
    Entity entity = manager.createEntity();
    Entity secondEntity = manager.createEntity();
    QUILL_LOG_INFO(
        BIRBS_LOGGER,
        "I've got {} and {}!",
        entity,
        secondEntity
    );
    return 0;
}
