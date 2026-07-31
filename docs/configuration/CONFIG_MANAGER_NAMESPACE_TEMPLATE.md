# Config Manager Namespace Template

Use this template when adding a new namespace to `hex_config_manager`.

## Goal

Add one namespace with minimal touch points:
- New files in one folder under `components/hex_config_manager/namespaces/<namespace>/`
- One registration update in `components/hex_config_manager/config_namespace_catalog.c`
- One build wiring update in `components/hex_config_manager/CMakeLists.txt`

## Required Folder Shape

Create a folder:

- `components/hex_config_manager/namespaces/<namespace>/`

Recommended file set:

- `config_ns_<namespace>.h`
- `config_ns_<namespace>.c`
- `config_ns_<namespace>_api.h` (if exposing namespace types/APIs)
- `config_domain_<namespace>_access.h` (if typed/discovery mapping is needed)
- `config_domain_<namespace>_access.c`
- `config_domain_<namespace>_defaults.c`
- `config_domain_<namespace>_persistence_nvs.c`

## Copy-Ready Skeletons

### 1) Namespace API Header

```c
// namespaces/<namespace>/config_ns_<namespace>_api.h
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // TODO: namespace fields
} <namespace>_config_t;

const <namespace>_config_t* config_get_<namespace>(void);
esp_err_t config_set_<namespace>(const <namespace>_config_t* config);

void config_load_<namespace>_defaults(<namespace>_config_t* config);

#ifdef __cplusplus
}
#endif
```

### 2) Namespace Descriptor Header

```c
// namespaces/<namespace>/config_ns_<namespace>.h
#pragma once

#include <stdbool.h>

#include "config_manager_core_types.h"
#include "config_ns_<namespace>_api.h"
#include "config_namespace_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nvs_handle_t* nvs_handle;
    bool* namespace_dirty;
    bool* namespace_loaded;
    <namespace>_config_t* config;
} config_<namespace>_namespace_context_t;

extern const config_namespace_descriptor_t g_<namespace>_namespace_descriptor;

void config_<namespace>_namespace_bind(
    nvs_handle_t* nvs_handle,
    bool* namespace_dirty,
    bool* namespace_loaded
);

void* config_<namespace>_namespace_context(void);
<namespace>_config_t* config_<namespace>_namespace_config(void);

#ifdef __cplusplus
}
#endif
```

### 3) Descriptor Source Contract

Implement callbacks in `config_ns_<namespace>.c` for:
- `load_defaults`
- `load_from_nvs`
- `write_defaults_to_nvs`
- `save`
- `list_parameters`
- `get_parameter_info`
- typed get/set handlers that namespace supports (`bool`, `int32`, `uint32`, `float`, `string`)

The descriptor export should follow:

```c
const config_namespace_descriptor_t g_<namespace>_namespace_descriptor = {
    .ns_id = CONFIG_NS_<NAMESPACE_ENUM>,
    .ns_name = "<namespace>",
    // callback wiring...
};
```

## Required Global Edits

### 1) Add Namespace Enum

Update `components/hex_config_manager/config_manager_core_types.h`:
- Add `CONFIG_NS_<NAMESPACE_ENUM>` before `CONFIG_NS_COUNT`.

### 2) Register in Catalog

Update `components/hex_config_manager/config_namespace_catalog.c`:
- Include `config_ns_<namespace>.h`
- Bind namespace context in init flow
- Register descriptor/context in catalog registration order

### 3) Build Wiring

Update `components/hex_config_manager/CMakeLists.txt`:
- Add new source files with `namespaces/<namespace>/...` paths
- Add include folder if new public headers are consumed outside the folder

## Runtime Validation Checklist

After build and flash:
- `list namespaces` shows `<namespace>`
- `list <namespace>` returns expected parameters
- `get <namespace> <param>` works
- `set <namespace> <param> <value>` works with range checks
- `setpersist <namespace> <param> <value>` persists
- `save <namespace>` succeeds
- Reboot and verify persistence

## Integration Validation

For each subsystem expected to consume this namespace:
- Confirm runtime reads namespace values, not compile-time constants
- Confirm startup order ensures config manager is initialized first
- Confirm logs identify namespace-backed config path

## Done Definition

Namespace addition is complete only if:
- Functional logic exists only in one namespace folder
- No edits were needed in `config_manager.c` functional logic
- Registration changes were limited to catalog wiring
- Build and runtime validation passed
