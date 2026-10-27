/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_check.h>
#include <cJSON.h>
#include "esp_mcp_mgr.h"
#include "esp_xiaozhi_transport.h"
#include "esp_xiaozhi_mcp.h"
#include "esp_xiaozhi_mcp_observer.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/idf_additions.h"
#include <stdatomic.h>

typedef struct {
    char *payload, *endpoint, *session;
    size_t length;
} product_mcp_request_t;
static esp_xiaozhi_mcp_observer_t s_observer;
void esp_xiaozhi_mcp_set_observer(esp_xiaozhi_mcp_observer_t observer) { s_observer = observer; }

/**
 * @brief MCP transport item structure; uses esp_xiaozhi_transport for MCP.
 */
typedef struct esp_xiaozhi_mcp_item_s {
    esp_xiaozhi_mcp_config_t *config;       /*!< XiaoZhi MCP transport configuration */
    esp_xiaozhi_transport_handle_t transport_handle; /*!< Transport handle (MQTT or WebSocket) */
    esp_mcp_mgr_handle_t handle;            /*!< MCP manager handle */
    QueueHandle_t requests;
    SemaphoreHandle_t worker_done;
    atomic_bool accepting, busy;
} esp_xiaozhi_mcp_item_t;

static const char *TAG = "ESP_XIAOZHI_MCP";

static esp_xiaozhi_mcp_item_t *s_xiaozhi_mcp = NULL;

static esp_xiaozhi_mcp_item_t *esp_xiaozhi_mcp_get_item_by_mgr_handle(esp_mcp_mgr_handle_t mgr_handle)
{
    esp_xiaozhi_mcp_item_t *mcp_item = (esp_xiaozhi_mcp_item_t *)s_xiaozhi_mcp;
    if (mcp_item && mcp_item->handle == mgr_handle) {
        return mcp_item;
    }

    return NULL;
}

static esp_err_t product_mcp_process_message(esp_mcp_mgr_handle_t mgr_handle,
                                         const char *ep_name,
                                         const char *payload,
                                         size_t payload_len,
                                         const char *session_id)
{
    ESP_RETURN_ON_FALSE(ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid endpoint name");
    ESP_RETURN_ON_FALSE(payload, ESP_ERR_INVALID_ARG, TAG, "Invalid payload");

    esp_xiaozhi_mcp_item_t *mcp_item = esp_xiaozhi_mcp_get_item_by_mgr_handle(mgr_handle);
    ESP_RETURN_ON_FALSE(mcp_item, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP item or handle mismatch");

    if (s_observer) s_observer(payload, payload_len);
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    esp_err_t ret = esp_mcp_mgr_req_handle(mgr_handle, ep_name, (const uint8_t *)payload, payload_len, &outbuf, &outlen);
    if (ret == ESP_OK && outbuf && outlen > 0 && atomic_load(&mcp_item->accepting)) {
        cJSON *response_root = cJSON_CreateObject();
        if (session_id) {
            cJSON_AddStringToObject(response_root, "session_id", session_id);
        }
        cJSON_AddStringToObject(response_root, "type", "mcp");
        cJSON *response_payload = cJSON_Parse((char *)outbuf);
        if (response_payload) {
            cJSON_AddItemToObject(response_root, "payload", response_payload);
            char *json_str = cJSON_PrintUnformatted(response_root);
            if (json_str) {
                esp_xiaozhi_transport_send_text(mcp_item->transport_handle, json_str);
                cJSON_free(json_str);
            } else {
                ESP_LOGE(TAG, "Failed to print MCP response");
            }
        } else {
            ESP_LOGE(TAG, "Failed to parse MCP response payload");
        }
        cJSON_Delete(response_root);
    } else if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to process MCP message: %s", esp_err_to_name(ret));
    }

    if (outbuf) esp_mcp_mgr_req_destroy_response(mgr_handle, outbuf);
    return ESP_OK;
}

static void product_mcp_worker(void *arg)
{
    esp_xiaozhi_mcp_item_t *item = arg;
    ESP_LOGI(TAG, "MCP worker: stack=%s, size=12288 bytes",
             esp_ptr_internal(pxTaskGetStackStart(NULL)) ? "internal RAM" : "external RAM");
    product_mcp_request_t *request;
    while (xQueueReceive(item->requests, &request, portMAX_DELAY) == pdTRUE && request) {
        atomic_store(&item->busy, true);
        if (atomic_load(&item->accepting)) {
            product_mcp_process_message(item->handle, request->endpoint, request->payload,
                                        request->length, request->session);
        }
        free(request->payload); free(request->endpoint); free(request->session); free(request);
        atomic_store(&item->busy, false);
    }
    xSemaphoreGive(item->worker_done);
    vTaskDeleteWithCaps(NULL);
}

esp_err_t esp_xiaozhi_mcp_handle_message(esp_mcp_mgr_handle_t mgr_handle,
                                         const char *ep_name, const char *payload,
                                         size_t payload_len, const char *session_id)
{
    esp_xiaozhi_mcp_item_t *item = esp_xiaozhi_mcp_get_item_by_mgr_handle(mgr_handle);
    if (!item || !atomic_load(&item->accepting)) return ESP_ERR_INVALID_STATE;
    if (!ep_name || !payload || !payload_len || payload_len > 8192) return ESP_ERR_INVALID_ARG;
    product_mcp_request_t *request = calloc(1, sizeof(*request));
    if (!request) return ESP_ERR_NO_MEM;
    request->payload = malloc(payload_len + 1);
    if (request->payload) {
        memcpy(request->payload, payload, payload_len);
        request->payload[payload_len] = '\0';
    }
    request->endpoint = strdup(ep_name);
    request->session = session_id ? strdup(session_id) : NULL;
    request->length = payload_len;
    if (request->payload && request->endpoint && (!session_id || request->session) &&
        xQueueSend(item->requests, &request, 0) == pdTRUE) return ESP_OK;
    free(request->payload); free(request->endpoint); free(request->session); free(request);
    ESP_LOGW(TAG, "MCP request queue full or allocation failed");
    return ESP_ERR_NO_MEM;
}

esp_err_t esp_xiaozhi_mcp_get_handle(esp_mcp_mgr_handle_t mgr_handle, esp_xiaozhi_transport_handle_t *transport_handle)
{
    ESP_RETURN_ON_FALSE(transport_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid transport handle");

    esp_xiaozhi_mcp_item_t *mcp_item = esp_xiaozhi_mcp_get_item_by_mgr_handle(mgr_handle);
    ESP_RETURN_ON_FALSE(mcp_item, ESP_ERR_INVALID_ARG, TAG, "Invalid MCP item or handle mismatch");

    *transport_handle = mcp_item->transport_handle;

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_init(esp_mcp_mgr_handle_t handle, esp_mcp_transport_handle_t *transport_handle)
{
    esp_xiaozhi_mcp_item_t *mcp_item = calloc(1, sizeof(esp_xiaozhi_mcp_item_t));
    ESP_RETURN_ON_FALSE(mcp_item, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for MCP transport item");

    mcp_item->handle = handle;
    mcp_item->config = NULL;

    esp_err_t ret = esp_xiaozhi_transport_init(&mcp_item->transport_handle);
    if (ret != ESP_OK) {
        free(mcp_item);
        return ret;
    }

    mcp_item->requests = xQueueCreate(4, sizeof(product_mcp_request_t *));
    mcp_item->worker_done = xSemaphoreCreateBinary();
    atomic_init(&mcp_item->accepting, false);
    atomic_init(&mcp_item->busy, false);
    if (!mcp_item->requests || !mcp_item->worker_done ||
        xTaskCreateWithCaps(product_mcp_worker, "xiaozhi_mcp", 12288, mcp_item, 3, NULL,
                            // MCP tools read NVS/Flash (vision Device-Id/UUID).
                            // Their stack must remain accessible with cache off.
                            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) != pdPASS) {
        ESP_LOGE(TAG, "MCP worker allocation failed: internal free=%u, largest=%u, stack=12288",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        if (mcp_item->requests) vQueueDelete(mcp_item->requests);
        if (mcp_item->worker_done) vSemaphoreDelete(mcp_item->worker_done);
        esp_xiaozhi_transport_deinit(mcp_item->transport_handle);
        free(mcp_item);
        return ESP_ERR_NO_MEM;
    }

    *transport_handle = (esp_mcp_transport_handle_t)mcp_item;
    s_xiaozhi_mcp = mcp_item;

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_deinit(esp_mcp_transport_handle_t handle)
{
    esp_xiaozhi_mcp_item_t *mcp_item = (esp_xiaozhi_mcp_item_t *)handle;
    ESP_RETURN_ON_FALSE(mcp_item, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    atomic_store(&mcp_item->accepting, false);
    product_mcp_request_t *end = NULL;
    xQueueSend(mcp_item->requests, &end, portMAX_DELAY);
    xSemaphoreTake(mcp_item->worker_done, portMAX_DELAY);
    vQueueDelete(mcp_item->requests);
    vSemaphoreDelete(mcp_item->worker_done);
    esp_xiaozhi_transport_deinit(mcp_item->transport_handle);
    mcp_item->handle = 0;
    mcp_item->config = NULL;
    if (s_xiaozhi_mcp == mcp_item) {
        s_xiaozhi_mcp = NULL;
    }
    free(mcp_item);

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_create_config(const void *config, void **config_out)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");

    esp_xiaozhi_mcp_config_t *old_config = (esp_xiaozhi_mcp_config_t *)config;
    esp_xiaozhi_mcp_config_t *new_config = calloc(1, sizeof(esp_xiaozhi_mcp_config_t));
    ESP_RETURN_ON_FALSE(new_config, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for new configuration");

    new_config->ctx = old_config->ctx;
    new_config->use_mqtt = old_config->use_mqtt;
    new_config->callbacks = old_config->callbacks;

    if (old_config->session_id && old_config->session_id_len > 0) {
        new_config->session_id_len = old_config->session_id_len;
        new_config->session_id = calloc(1, new_config->session_id_len + 1);
        ESP_RETURN_ON_FALSE(new_config->session_id, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for session ID");
        memcpy(new_config->session_id, old_config->session_id, new_config->session_id_len);
    }

    *config_out = new_config;

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_delete_config(void *config)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");

    esp_xiaozhi_mcp_config_t *old_config = (esp_xiaozhi_mcp_config_t *)config;
    if (old_config->session_id) {
        free(old_config->session_id);
    }
    free(old_config);

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_start(esp_mcp_transport_handle_t handle, void *config)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");

    esp_xiaozhi_mcp_item_t *mcp_item = (esp_xiaozhi_mcp_item_t *)handle;
    esp_xiaozhi_mcp_config_t *new_config = (esp_xiaozhi_mcp_config_t *)config;

    mcp_item->config = new_config;
    atomic_store(&mcp_item->accepting, true);

    esp_err_t ret = esp_xiaozhi_transport_start(mcp_item->transport_handle, new_config->use_mqtt, &new_config->callbacks);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to start transport: %s", esp_err_to_name(ret));

    ESP_LOGD(TAG, "MCP transport started with %s", new_config->use_mqtt ? "MQTT" : "WebSocket");

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_stop(esp_mcp_transport_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    esp_xiaozhi_mcp_item_t *mcp_item = (esp_xiaozhi_mcp_item_t *)handle;

    atomic_store(&mcp_item->accepting, false);
    while (atomic_load(&mcp_item->busy)) vTaskDelay(pdMS_TO_TICKS(10));
    esp_xiaozhi_transport_stop(mcp_item->transport_handle);
    mcp_item->config = NULL;

    ESP_LOGD(TAG, "MCP transport stopped");

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_register_endpoint(esp_mcp_transport_handle_t handle, const char *ep_name, void *priv_data)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid endpoint name");

    ESP_LOGD(TAG, "MCP endpoint registered: %s", ep_name);

    return ESP_OK;
}

static esp_err_t esp_xiaozhi_mcp_unregister_endpoint(esp_mcp_transport_handle_t handle, const char *ep_name)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(ep_name, ESP_ERR_INVALID_ARG, TAG, "Invalid endpoint name");

    ESP_LOGD(TAG, "MCP endpoint unregistered: %s", ep_name);

    return ESP_OK;
}

const esp_mcp_transport_t esp_mcp_transport_xiaozhi = {
    .init                = esp_xiaozhi_mcp_init,
    .deinit              = esp_xiaozhi_mcp_deinit,
    .create_config       = esp_xiaozhi_mcp_create_config,
    .delete_config       = esp_xiaozhi_mcp_delete_config,
    .start               = esp_xiaozhi_mcp_start,
    .stop                = esp_xiaozhi_mcp_stop,
    .register_endpoint   = esp_xiaozhi_mcp_register_endpoint,
    .unregister_endpoint = esp_xiaozhi_mcp_unregister_endpoint,
};
