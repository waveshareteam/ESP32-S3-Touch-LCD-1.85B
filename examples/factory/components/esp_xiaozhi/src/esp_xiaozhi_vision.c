/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <esp_check.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "esp_xiaozhi_board.h"
#include "esp_xiaozhi_vision.h"

#define ESP_XIAOZHI_VISION_BOUNDARY "----ESP32_CAMERA_BOUNDARY"
#define VISION_IO_TIMEOUT_MS 3000
#define VISION_TOTAL_TIMEOUT_US (45LL * 1000000)

static esp_err_t check_request(const esp_xiaozhi_vision_t *vision, int64_t deadline)
{
    if (vision->cancelled && vision->cancelled(vision->cancel_context)) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_timer_get_time() >= deadline ? ESP_ERR_TIMEOUT : ESP_OK;
}

// esp_http_client_write() does not frame chunks, even after open(client, -1).
// Emit HTTP chunk framing explicitly, as the official Xiaozhi HTTP wrapper does.
static esp_err_t write_bytes(esp_http_client_handle_t client,
                             const esp_xiaozhi_vision_t *vision, int64_t deadline,
                             const void *data, size_t size)
{
    esp_err_t ret = check_request(vision, deadline);
    if (ret != ESP_OK) return ret;
    const int written = esp_http_client_write(client, (const char *)data, (int)size);
    // A partial write is fatal: replaying it would corrupt the multipart body.
    return written == (int)size ? check_request(vision, deadline) : ESP_ERR_HTTP_WRITE_DATA;
}

static esp_err_t write_chunks(esp_http_client_handle_t client,
                              const esp_xiaozhi_vision_t *vision, int64_t deadline,
                              const void *data, size_t size)
{
    const uint8_t *cursor = data;
    while (size) {
        const size_t len = size > 4096 ? 4096 : size;
        char header[16];
        const int header_len = snprintf(header, sizeof(header), "%x\r\n", (unsigned)len);
        esp_err_t ret = write_bytes(client, vision, deadline, header, header_len);
        if (ret == ESP_OK) ret = write_bytes(client, vision, deadline, cursor, len);
        if (ret == ESP_OK) ret = write_bytes(client, vision, deadline, "\r\n", 2);
        if (ret != ESP_OK) return ret;
        cursor += len;
        size -= len;
    }
    return ESP_OK;
}

esp_err_t esp_xiaozhi_vision_init(esp_xiaozhi_vision_t *vision,
                                  const char *url,
                                  const char *token)
{
    ESP_RETURN_ON_FALSE(vision != NULL, ESP_ERR_INVALID_ARG, "ESP_XIAOZHI_VISION", "Invalid vision");

    memset(vision, 0, sizeof(*vision));
    return esp_xiaozhi_vision_set_endpoint(vision, url, token);
}

esp_err_t esp_xiaozhi_vision_set_endpoint(esp_xiaozhi_vision_t *vision,
                                          const char *url,
                                          const char *token)
{
    ESP_RETURN_ON_FALSE(vision != NULL, ESP_ERR_INVALID_ARG, "ESP_XIAOZHI_VISION", "Invalid vision");

    if (url != NULL) {
        if (strlen(url) >= sizeof(vision->explain_url)) return ESP_ERR_INVALID_SIZE;
        strlcpy(vision->explain_url, url, sizeof(vision->explain_url));
    } else {
        vision->explain_url[0] = '\0';
    }

    if (token != NULL) {
        if (strlen(token) >= sizeof(vision->explain_token)) return ESP_ERR_INVALID_SIZE;
        strlcpy(vision->explain_token, token, sizeof(vision->explain_token));
    } else {
        vision->explain_token[0] = '\0';
    }

    return ESP_OK;
}

esp_err_t esp_xiaozhi_vision_explain_jpeg(esp_xiaozhi_vision_t *vision,
                                          const char *log_tag,
                                          const char *filename,
                                          const char *question,
                                          const uint8_t *jpeg_data,
                                          size_t jpeg_len,
                                          char *out_buf,
                                          size_t buf_size,
                                          size_t *out_len)
{
    ESP_RETURN_ON_FALSE(vision != NULL, ESP_ERR_INVALID_ARG, log_tag, "Invalid vision");
    vision->phase = "prepare";
    vision->http_status = 0;
    if (out_len) *out_len = 0;
    if (out_buf && buf_size) out_buf[0] = '\0';
    ESP_RETURN_ON_FALSE(filename && !strpbrk(filename, "\r\n\""), ESP_ERR_INVALID_ARG, log_tag, "Invalid filename");
    ESP_RETURN_ON_FALSE(question && question[0] && strlen(question) <= 1024,
                        ESP_ERR_INVALID_ARG, log_tag, "Invalid question");
    ESP_RETURN_ON_FALSE(jpeg_data && jpeg_len >= 4 && jpeg_data[0] == 0xff && jpeg_data[1] == 0xd8 &&
                        jpeg_data[jpeg_len - 2] == 0xff && jpeg_data[jpeg_len - 1] == 0xd9,
                        ESP_ERR_INVALID_ARG, log_tag, "Incomplete JPEG");
    ESP_RETURN_ON_FALSE(!out_buf || buf_size > 0, ESP_ERR_INVALID_ARG, log_tag, "Invalid response buffer");
    ESP_RETURN_ON_FALSE(vision->explain_url[0] != '\0', ESP_ERR_INVALID_STATE, log_tag, "Explain URL is not configured");

    const int64_t started = esp_timer_get_time();
    const int64_t deadline = started + VISION_TOTAL_TIMEOUT_US;
    esp_err_t ret = check_request(vision, deadline);
    ESP_RETURN_ON_ERROR(ret, log_tag, "Request cancelled");
    esp_xiaozhi_chat_board_info_t board_info = {0};
    ret = esp_xiaozhi_chat_get_board_info(&board_info);
    ESP_RETURN_ON_ERROR(ret, log_tag, "Failed to get board info");

    const char *header_format =
        "--%s\r\nContent-Disposition: form-data; name=\"question\"\r\n\r\n%s\r\n"
        "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n";
    const int header_len = snprintf(NULL, 0, header_format, ESP_XIAOZHI_VISION_BOUNDARY,
                                    question, ESP_XIAOZHI_VISION_BOUNDARY, filename);
    ESP_RETURN_ON_FALSE(header_len > 0, ESP_FAIL, log_tag, "Multipart header failed");
    char *header = heap_caps_malloc(header_len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(header != NULL, ESP_ERR_NO_MEM, log_tag, "Multipart header allocation failed");
    snprintf(header, header_len + 1, header_format, ESP_XIAOZHI_VISION_BOUNDARY,
             question, ESP_XIAOZHI_VISION_BOUNDARY, filename);
    const char footer[] = "\r\n--" ESP_XIAOZHI_VISION_BOUNDARY "--\r\n";

    // Connect gets 8 seconds; response reads use shorter waits so leaving the
    // APP cancels the request without waiting for the full inference deadline.
    esp_http_client_config_t config = {
        .url = vision->explain_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 8000,
        .disable_auto_redirect = true,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        heap_caps_free(header);
        return ESP_ERR_NO_MEM;
    }
    size_t response_len = 0;
    char content_type[96];
    char authorization[ESP_XIAOZHI_VISION_EXPLAIN_TOKEN_LEN + 8];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", ESP_XIAOZHI_VISION_BOUNDARY);
    ret = esp_http_client_set_header(client, "Content-Type", content_type);
    if (ret == ESP_OK) ret = esp_http_client_set_header(client, "Device-Id", board_info.mac_address);
    if (ret == ESP_OK) ret = esp_http_client_set_header(client, "Client-Id", board_info.uuid);
    if (ret == ESP_OK && vision->explain_token[0]) {
        snprintf(authorization, sizeof(authorization), "Bearer %s", vision->explain_token);
        ret = esp_http_client_set_header(client, "Authorization", authorization);
    }
    if (ret != ESP_OK) goto done;

    vision->phase = "connect";
    ESP_LOGI(log_tag, "Vision connect: JPEG=%u bytes, deadline=45 s", (unsigned)jpeg_len);
    ret = esp_http_client_open(client, -1); // Transfer-Encoding: chunked
    if (ret != ESP_OK) goto done;
    esp_http_client_set_timeout_ms(client, VISION_IO_TIMEOUT_MS);

    vision->phase = "upload";
    ret = write_chunks(client, vision, deadline, header, header_len);
    if (ret == ESP_OK) ret = write_chunks(client, vision, deadline, jpeg_data, jpeg_len);
    if (ret == ESP_OK) ret = write_chunks(client, vision, deadline, footer, sizeof(footer) - 1);
    if (ret == ESP_OK) ret = write_bytes(client, vision, deadline, "0\r\n\r\n", 5);
    if (ret != ESP_OK) goto done;
    ESP_LOGI(log_tag, "Vision upload complete: %u JPEG bytes", (unsigned)jpeg_len);

    vision->phase = "headers";
    for (;;) {
        ret = check_request(vision, deadline);
        if (ret != ESP_OK) goto done;
        // IDF 5.5 retains its HTTP parser across EAGAIN, including partial
        // headers; on_headers_complete sets status when the header is finished.
        const int64_t length = esp_http_client_fetch_headers(client);
        if (length == -ESP_ERR_HTTP_EAGAIN) continue;
        if (length < 0) {
            ret = ESP_ERR_HTTP_FETCH_HEADER;
            goto done;
        }
        break;
    }
    vision->http_status = esp_http_client_get_status_code(client);
    ESP_LOGI(log_tag, "Vision HTTP status=%d", vision->http_status);
    if (vision->http_status != 200) {
        // Do not forward bearer credentials to a redirect target.
        ret = ESP_FAIL;
        goto done;
    }

    vision->phase = "response";
    for (;;) {
        ret = check_request(vision, deadline);
        if (ret != ESP_OK) goto done;
        char chunk[512];
        const int len = esp_http_client_read(client, chunk, sizeof(chunk));
        if (len == -ESP_ERR_HTTP_EAGAIN) continue;
        if (len < 0) {
            ret = ESP_ERR_HTTP_CONNECTION_CLOSED;
            goto done;
        }
        if (len == 0) {
            ret = esp_http_client_is_complete_data_received(client) ? ESP_OK : ESP_ERR_HTTP_INCOMPLETE_DATA;
            break;
        }
        // Drain cached bytes too: wire reception may finish before read() has
        // delivered every byte cached while fetching the headers.
        if ((out_buf && (size_t)len >= buf_size - response_len) ||
                (!out_buf && response_len + (size_t)len > 64 * 1024)) {
            ret = ESP_ERR_INVALID_SIZE;
            goto done;
        }
        if (out_buf) {
            memcpy(out_buf + response_len, chunk, len);
            out_buf[response_len + len] = '\0';
        }
        response_len += len;
    }
    if (ret == ESP_OK && !response_len) ret = ESP_ERR_INVALID_RESPONSE;
    if (ret == ESP_OK) {
        if (out_len) *out_len = response_len;
        vision->phase = "complete";
    }

done:
    ESP_LOGI(log_tag, "Vision result: phase=%s, HTTP=%d, result=%s, response=%u bytes, %lld ms",
             vision->phase, vision->http_status, esp_err_to_name(ret), (unsigned)response_len,
             (esp_timer_get_time() - started) / 1000);
    esp_http_client_cleanup(client);
    heap_caps_free(header);
    return ret;
}
