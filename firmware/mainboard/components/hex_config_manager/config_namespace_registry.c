#include "config_namespace_registry.h"

#include <string.h>

#define CONFIG_NAMESPACE_REGISTRY_MAX 8

static config_namespace_registration_t g_registry[CONFIG_NAMESPACE_REGISTRY_MAX];
static size_t g_registry_count = 0;

void config_namespace_registry_reset(void) {
    g_registry_count = 0;
    memset(g_registry, 0, sizeof(g_registry));
}

esp_err_t config_namespace_registry_register(const config_namespace_descriptor_t* descriptor, void* context) {
    if (!descriptor || !descriptor->ns_name) {
        return ESP_ERR_INVALID_ARG;
    }

    if (descriptor->ns_id < 0 || descriptor->ns_id >= CONFIG_NS_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_registry_count >= CONFIG_NAMESPACE_REGISTRY_MAX) {
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < g_registry_count; i++) {
        if (g_registry[i].descriptor->ns_id == descriptor->ns_id ||
            strcmp(g_registry[i].descriptor->ns_name, descriptor->ns_name) == 0) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    g_registry[g_registry_count].descriptor = descriptor;
    g_registry[g_registry_count].context = context;
    g_registry_count++;

    return ESP_OK;
}

const config_namespace_registration_t* config_namespace_registry_find_by_id(config_namespace_t ns_id) {
    for (size_t i = 0; i < g_registry_count; i++) {
        if (g_registry[i].descriptor->ns_id == ns_id) {
            return &g_registry[i];
        }
    }

    return NULL;
}

const config_namespace_registration_t* config_namespace_registry_find_by_name(const char* ns_name) {
    if (!ns_name) {
        return NULL;
    }

    for (size_t i = 0; i < g_registry_count; i++) {
        if (strcmp(g_registry[i].descriptor->ns_name, ns_name) == 0) {
            return &g_registry[i];
        }
    }

    return NULL;
}

size_t config_namespace_registry_count(void) {
    return g_registry_count;
}

const config_namespace_registration_t* config_namespace_registry_get_at(size_t index) {
    if (index >= g_registry_count) {
        return NULL;
    }

    return &g_registry[index];
}
