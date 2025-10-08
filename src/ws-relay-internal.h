/*
OBS WebSocket Relay - Internal Definitions
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

#include "ws-relay.h"
#include <libwebsockets.h>
#include <util/dstr.h>
#include <util/threading.h>
#include <time.h>
#include <vector>

// Forward declarations
typedef struct ws_connection ws_connection_t;
typedef struct ws_relay ws_relay_t;

// Connection data structure
struct ws_connection {
    struct lws *wsi;
    ws_connection_state_t state;
    std::vector<std::vector<char>> buffers;
    bool is_remote;
    ws_relay_t *relay;
    char *address;
    uint16_t port;
    char *path;
    bool use_ssl;
};

// Main relay structure
struct ws_relay {
    ws_relay_config_t config;
    
    ws_connection_t obs_conn;
    ws_connection_t remote_conn;
    
    struct lws_context *context;
    pthread_t thread;
    bool running;
    bool thread_started;
    
    pthread_mutex_t mutex;
    
    // Reconnection handling
    time_t last_reconnect_attempt;
};

// Internal function declarations
void *ws_relay_thread(void *data);
void ws_connection_init(ws_connection_t *conn, bool is_remote, ws_relay_t *relay);
void ws_connection_free(ws_connection_t *conn);
bool ws_connect(ws_connection_t *conn, const char *address);
bool parse_ws_url(const char *url, char **host, uint16_t *port, char **path, bool *use_ssl);

// LWS protocol callbacks
int ws_callback_obs(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
int ws_callback_remote(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

// LWS protocols array
extern const struct lws_protocols protocols[];
