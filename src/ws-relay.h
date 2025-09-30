/*
OBS WebSocket Relay
Copyright (C) 2025 BlueGlassBlock

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

#ifdef __cplusplus
extern "C" {


#endif

#include <stdbool.h>
#include <stdint.h>

// WebSocket connection states
typedef enum {
    WS_STATE_DISCONNECTED,
    WS_STATE_CONNECTING,
    WS_STATE_CONNECTED,
    WS_STATE_ERROR
} ws_connection_state_t;

// WebSocket relay structure
typedef struct ws_relay ws_relay_t;

extern ws_relay_t *global_relay;

// Configuration structure
typedef struct {
    char *local_obs_address; // Local OBS WebSocket address (e.g., "ws://localhost:4455")
    char *remote_ws_address; // Remote WebSocket address (supports wss://)
    int reconnect_interval; // Reconnect interval in seconds
    bool enable_logging; // Enable verbose logging
} ws_relay_config_t;

// Callback function types
typedef void (*ws_message_callback_t)(const char *message, size_t length, void *user_data);

typedef void (*ws_state_callback_t)(ws_connection_state_t state, void *user_data);

// Main relay functions
ws_relay_t *ws_relay_create(const ws_relay_config_t *config);

void ws_relay_destroy(ws_relay_t *relay);

bool ws_relay_start(ws_relay_t *relay);

void ws_relay_stop(ws_relay_t *relay);

bool ws_relay_is_connected(ws_relay_t *relay);

ws_connection_state_t ws_relay_get_obs_state(ws_relay_t *relay);

ws_connection_state_t ws_relay_get_remote_state(ws_relay_t *relay);

// Configuration management
void ws_relay_config_init(ws_relay_config_t *config);

void ws_relay_config_free(ws_relay_config_t *config);

bool ws_relay_config_load(ws_relay_config_t *config);

bool ws_relay_config_save(const ws_relay_config_t *config);

// Frontend settings UI functions
void ws_relay_show_settings(void);

void ws_relay_hide_settings(void);

void ws_relay_cleanup_settings(void);


#ifdef __cplusplus
}
#endif
