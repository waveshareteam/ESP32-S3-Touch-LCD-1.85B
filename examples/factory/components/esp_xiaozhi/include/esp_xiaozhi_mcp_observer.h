/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Product hook observes copied MCP requests on the dedicated MCP worker.
 * Configure before starting chat; the callback must not retain payload. */
typedef void (*esp_xiaozhi_mcp_observer_t)(const char *payload, size_t length);
void esp_xiaozhi_mcp_set_observer(esp_xiaozhi_mcp_observer_t observer);
#ifdef __cplusplus
}
#endif
