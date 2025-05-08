#include "container.h"

std::string container_type_to_string(CONTAINER_TYPE type) {
    switch (type) {
        case HOT_CONTAINER: return "hot_container";
        case COLD_CONTAINER: return "cold_container";
        case CONTAINER:      return "container";
        default:
            throw std::invalid_argument("Unknown CONTAINER_TYPE value");
    }
}

CONTAINER_TYPE string_to_container_type(const std::string& str) {
    if (str == "hot_container") return HOT_CONTAINER;
    if (str == "cold_container") return COLD_CONTAINER;
    if (str == "container") return CONTAINER;
    throw std::invalid_argument("Unknown CONTAINER_TYPE string");
}
