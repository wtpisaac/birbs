#include <quill/LogMacros.h>

#include "core/logger.hpp"

int main()
{
    QUILL_LOG_INFO(
        BIRBS_LOGGER,
        "Hello, World!"
    );
    return 0;
}
