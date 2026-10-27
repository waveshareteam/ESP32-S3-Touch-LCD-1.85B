#pragma once
#include <stddef.h>
#include <stdint.h>

/* A bounded, single-question captive portal DNS responder. Zero means ignore. */
size_t portal_dns_reply(const uint8_t *query, size_t length, uint8_t *reply,
                        size_t capacity, const uint8_t ipv4[4]);
