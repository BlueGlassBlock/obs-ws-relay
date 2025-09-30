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

#include "ws-relay-internal.h"
#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>
#include <util/threading.h>
#include <libwebsockets.h>

ws_relay_t *ws_relay_create(const ws_relay_config_t *config) {
    if (!config) {
        obs_log(LOG_ERROR, "Invalid configuration provided");
        return NULL;
    }

    ws_relay_t *relay = bzalloc(sizeof(ws_relay_t));
    if (!relay) {
        obs_log(LOG_ERROR, "Failed to allocate relay structure");
        return NULL;
    }

    // Copy configuration
    ws_relay_config_init(&relay->config);
    if (config->local_obs_address && strlen(config->local_obs_address) > 0) {
        bfree(relay->config.local_obs_address);
        relay->config.local_obs_address = bstrdup(config->local_obs_address);
    }
    if (config->remote_ws_address && strlen(config->remote_ws_address) > 0) {
        bfree(relay->config.remote_ws_address);
        relay->config.remote_ws_address = bstrdup(config->remote_ws_address);
    }
    relay->config.reconnect_interval = config->reconnect_interval;
    relay->config.enable_logging = config->enable_logging;

    // Initialize mutex
    if (pthread_mutex_init(&relay->mutex, NULL) != 0) {
        obs_log(LOG_ERROR, "Failed to initialize mutex");
        ws_relay_config_free(&relay->config);
        bfree(relay);
        return NULL;
    }

    // Initialize connections
    ws_connection_init(&relay->obs_conn, false, relay);
    ws_connection_init(&relay->remote_conn, true, relay);

    // Create libwebsockets context
    struct lws_context_creation_info info = {0};
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    relay->context = lws_create_context(&info);
    if (!relay->context) {
        obs_log(LOG_ERROR, "Failed to create libwebsockets context");
        ws_connection_free(&relay->obs_conn);
        ws_connection_free(&relay->remote_conn);
        pthread_mutex_destroy(&relay->mutex);
        ws_relay_config_free(&relay->config);
        bfree(relay);
        return NULL;
    }

    relay->running = false;
    relay->thread_started = false;
    relay->last_reconnect_attempt = 0;

    obs_log(LOG_INFO, "WebSocket relay created successfully");
    return relay;
}

void ws_relay_destroy(ws_relay_t *relay) {
    if (!relay) return;

    obs_log(LOG_INFO, "Destroying WebSocket relay");

    // Stop the relay if running
    ws_relay_stop(relay);

    // Clean up libwebsockets context
    if (relay->context) {
        lws_context_destroy(relay->context);
        relay->context = NULL;
    }

    // Clean up connections
    ws_connection_free(&relay->obs_conn);
    ws_connection_free(&relay->remote_conn);

    // Clean up mutex
    pthread_mutex_destroy(&relay->mutex);

    // Clean up configuration
    ws_relay_config_free(&relay->config);

    bfree(relay);
    obs_log(LOG_INFO, "WebSocket relay destroyed");
}

bool ws_relay_start(ws_relay_t *relay) {
    if (!relay) {
        obs_log(LOG_ERROR, "Invalid relay pointer");
        return false;
    }

    if (relay->running) {
        obs_log(LOG_WARNING, "Relay is already running");
        return true;
    }

    obs_log(LOG_INFO, "Starting WebSocket relay");

    // Validate configuration
    if (!relay->config.local_obs_address || strlen(relay->config.local_obs_address) == 0) {
        obs_log(LOG_ERROR, "Local OBS WebSocket address not configured");
        return false;
    }

    if (!relay->config.remote_ws_address || strlen(relay->config.remote_ws_address) == 0) {
        obs_log(LOG_ERROR, "Remote WebSocket address not configured");
        return false;
    }

    relay->running = true;
    relay->last_reconnect_attempt = 0;

    // Start the thread
    if (pthread_create(&relay->thread, NULL, ws_relay_thread, relay) != 0) {
        obs_log(LOG_ERROR, "Failed to create relay thread");
        relay->running = false;
        return false;
    }

    relay->thread_started = true;
    obs_log(LOG_INFO, "WebSocket relay started successfully");
    return true;
}

void ws_relay_stop(ws_relay_t *relay) {
    if (!relay) return;

    if (!relay->running) {
        obs_log(LOG_DEBUG, "Relay is not running");
        return;
    }

    obs_log(LOG_INFO, "Stopping WebSocket relay");

    relay->running = false;

    lws_cancel_service(relay->context);

    // Close connections
    pthread_mutex_lock(&relay->mutex);

    if (relay->obs_conn.wsi) {
        lws_close_reason(relay->obs_conn.wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
        relay->obs_conn.wsi = NULL;
    }
    relay->obs_conn.state = WS_STATE_DISCONNECTED;

    if (relay->remote_conn.wsi) {
        lws_close_reason(relay->remote_conn.wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
        relay->remote_conn.wsi = NULL;
    }
    relay->remote_conn.state = WS_STATE_DISCONNECTED;

    pthread_mutex_unlock(&relay->mutex);

    // Wait for thread to finish
    if (relay->thread_started) {
        pthread_join(relay->thread, NULL);
        relay->thread_started = false;
    }

    obs_log(LOG_INFO, "WebSocket relay stopped");
}

bool ws_relay_is_connected(ws_relay_t *relay) {
    if (!relay) return false;

    pthread_mutex_lock(&relay->mutex);
    bool connected = (relay->obs_conn.state == WS_STATE_CONNECTED &&
                      relay->remote_conn.state == WS_STATE_CONNECTED);
    pthread_mutex_unlock(&relay->mutex);

    return connected;
}

ws_connection_state_t ws_relay_get_obs_state(ws_relay_t *relay) {
    if (!relay) return WS_STATE_DISCONNECTED;

    pthread_mutex_lock(&relay->mutex);
    ws_connection_state_t state = relay->obs_conn.state;
    pthread_mutex_unlock(&relay->mutex);

    return state;
}

ws_connection_state_t ws_relay_get_remote_state(ws_relay_t *relay) {
    if (!relay) return WS_STATE_DISCONNECTED;

    pthread_mutex_lock(&relay->mutex);
    ws_connection_state_t state = relay->remote_conn.state;
    pthread_mutex_unlock(&relay->mutex);

    return state;
}
