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
#include <util/dstr.h>
#include <libwebsockets.h>

// LWS protocols
const struct lws_protocols protocols[] = {
    {
        "obs-websocket",
        ws_callback_obs,
        0,
        4096,
    },
    {
        "websocket",
        ws_callback_remote,
        0,
        4096,
    },
    {NULL, NULL, 0, 0} /* terminator */
};

// Parse WebSocket URL
bool parse_ws_url(const char *url, char **host, uint16_t *port, char **path, bool *use_ssl) {
    if (!url || !host || !port || !path || !use_ssl) return false;

    *host = NULL;
    *path = NULL;
    *port = 80;
    *use_ssl = false;

    // Check protocol
    if (strncmp(url, "wss://", 6) == 0) {
        *use_ssl = true;
        *port = 443;
        url += 6;
    } else if (strncmp(url, "ws://", 5) == 0) {
        *use_ssl = false;
        *port = 80;
        url += 5;
    } else {
        obs_log(LOG_ERROR, "Invalid WebSocket URL protocol");
        return false;
    }

    // Parse host and port
    const char *colon = strchr(url, ':');
    const char *slash = strchr(url, '/');

    if (colon && (!slash || colon < slash)) {
        // Port specified
        size_t host_len = colon - url;
        *host = (char*) bzalloc(host_len + 1);
        strncpy(*host, url, host_len);

        char *endptr;
        long port_val = strtol(colon + 1, &endptr, 10);
        if (port_val > 0 && port_val <= 65535 && (endptr == slash || *endptr == '\0')) {
            *port = (uint16_t) port_val;
        }

        if (slash) {
            *path = bstrdup(slash);
        } else {
            *path = bstrdup("/");
        }
    } else if (slash) {
        // No port, path specified
        size_t host_len = slash - url;
        *host = (char *)bzalloc(host_len + 1);
        strncpy(*host, url, host_len);
        *path = bstrdup(slash);
    } else {
        // No port or path
        *host = bstrdup(url);
        *path = bstrdup("/");
    }

    return true;
}

// Initialize connection
void ws_connection_init(ws_connection_t *conn, bool is_remote, ws_relay_t *relay) {
    if (!conn) return;

    memset(conn, 0, sizeof(ws_connection_t));
    conn->state = WS_STATE_DISCONNECTED;
    conn->is_remote = is_remote;
    conn->relay = relay;
    conn->buffers = std::vector<std::vector<char>>();
}

// Free connection
void ws_connection_free(ws_connection_t *conn) {
    if (!conn) return;

    conn->buffers.clear();
    bfree(conn->address);
    bfree(conn->path);
    memset(conn, 0, sizeof(ws_connection_t));
}

// OBS WebSocket callback
int ws_callback_obs(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    ws_connection_t *conn = (ws_connection_t *) lws_get_opaque_user_data(wsi);
    if (!conn) return 0;

    ws_relay_t *relay = conn->relay;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            obs_log(LOG_INFO, "Connected to OBS WebSocket");
            pthread_mutex_lock(&relay->mutex);
            conn->state = WS_STATE_CONNECTED;
            pthread_mutex_unlock(&relay->mutex);
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (relay->config.enable_logging) {
                obs_log(LOG_INFO, "Received from OBS: %.*s", (int) len, (char *) in);
            }

            // Forward message to remote if connected
            pthread_mutex_lock(&relay->mutex);
            if (relay->remote_conn.state == WS_STATE_CONNECTED && relay->remote_conn.wsi) {
                std::vector<char> buf(LWS_PRE + len);
                std::memcpy(buf.data() + LWS_PRE, in, len);
                relay->remote_conn.buffers.push_back(std::move(buf));
                lws_callback_on_writable(relay->remote_conn.wsi);
            }
            pthread_mutex_unlock(&relay->mutex);
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            pthread_mutex_lock(&relay->mutex);
            if (!conn->buffers.empty()) {
                for (auto &buf: conn->buffers) {
                    if (relay->config.enable_logging) {
                        obs_log(LOG_INFO, "Write to OBS: %.*s", (int) (buf.size() - LWS_PRE),
                                buf.data() + LWS_PRE);
                    }
                    int n = lws_write(wsi, (unsigned char *) buf.data() + LWS_PRE,
                                      buf.size() - LWS_PRE, LWS_WRITE_TEXT);
                    if (n < 0) {
                        obs_log(LOG_ERROR, "Failed to write to OBS WebSocket");
                        conn->buffers.clear();
                        pthread_mutex_unlock(&relay->mutex);
                        return -1;
                    }
                }
                conn->buffers.clear();
            }
            pthread_mutex_unlock(&relay->mutex);
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            obs_log(LOG_ERROR, "OBS WebSocket connection error");
            pthread_mutex_lock(&relay->mutex);
            conn->state = WS_STATE_ERROR;
            pthread_mutex_unlock(&relay->mutex);
            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
        case LWS_CALLBACK_CLOSED:
            obs_log(LOG_INFO, "OBS WebSocket connection closed");
            pthread_mutex_lock(&relay->mutex);
            conn->state = WS_STATE_DISCONNECTED;
            conn->wsi = NULL;
            pthread_mutex_unlock(&relay->mutex);
            break;

        default:
            break;
    }

    return 0;
}

// Remote WebSocket callback
int ws_callback_remote(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    ws_connection_t *conn = (ws_connection_t *) lws_get_opaque_user_data(wsi);
    if (!conn) return 0;

    ws_relay_t *relay = conn->relay;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            obs_log(LOG_INFO, "Connected to remote WebSocket");
            pthread_mutex_lock(&relay->mutex);
            conn->state = WS_STATE_CONNECTED;
            pthread_mutex_unlock(&relay->mutex);
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (relay->config.enable_logging) {
                obs_log(LOG_INFO, "Received from remote: %.*s", (int) len, (char *) in);
            }

            // Forward message to OBS if connected
            pthread_mutex_lock(&relay->mutex);
            if (relay->obs_conn.state == WS_STATE_CONNECTED && relay->obs_conn.wsi) {
                std::vector<char> buf(LWS_PRE + len);
                std::memcpy(buf.data() + LWS_PRE, in, len);
                relay->obs_conn.buffers.push_back(std::move(buf));
                lws_callback_on_writable(relay->obs_conn.wsi);
            }
            pthread_mutex_unlock(&relay->mutex);
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            pthread_mutex_lock(&relay->mutex);
            if (!conn->buffers.empty()) {
                for (auto &buf: conn->buffers) {
                    if (relay->config.enable_logging) {
                        obs_log(LOG_INFO, "Write to remote: %.*s", (int) (buf.size() - LWS_PRE),
                                buf.data() + LWS_PRE);
                    }
                    int n = lws_write(wsi, (unsigned char *) buf.data() + LWS_PRE,
                                      buf.size() - LWS_PRE, LWS_WRITE_TEXT);
                    if (n < 0) {
                        obs_log(LOG_ERROR, "Failed to write to remote WebSocket");
                        conn->buffers.clear();
                        pthread_mutex_unlock(&relay->mutex);
                        return -1;
                    }
                }
                conn->buffers.clear();
            }
            pthread_mutex_unlock(&relay->mutex);
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            obs_log(LOG_ERROR, "Remote WebSocket connection error");
            pthread_mutex_lock(&relay->mutex);
            conn->state = WS_STATE_ERROR;
            pthread_mutex_unlock(&relay->mutex);
            break;
        
        case LWS_CALLBACK_CLIENT_CLOSED:
        case LWS_CALLBACK_CLOSED:
            obs_log(LOG_INFO, "Remote WebSocket connection closed");
            pthread_mutex_lock(&relay->mutex);
            conn->state = WS_STATE_DISCONNECTED;
            conn->wsi = NULL;
            pthread_mutex_unlock(&relay->mutex);
            break;

        default:
		    break;
    }

    return 0;
}

// Connect to WebSocket
bool ws_connect(ws_connection_t *conn, const char *address) {
    if (!conn || !conn->relay || !address) return false;

    ws_relay_t *relay = conn->relay;

    // Parse URL
    if (!parse_ws_url(address, &conn->address, &conn->port, &conn->path, &conn->use_ssl)) {
        obs_log(LOG_ERROR, "Failed to parse WebSocket URL: %s", address);
        return false;
    }

    struct lws_client_connect_info info = {0};
    info.context = relay->context;
    info.address = conn->address;
    info.port = conn->port;
    info.path = conn->path;
    info.host = conn->address;
    info.origin = conn->address;
    info.protocol = conn->is_remote ? "websocket" : "obs-websocket";
    info.ietf_version_or_minus_one = -1;
    info.userdata = conn;

    if (conn->use_ssl) {
        info.ssl_connection = 1;
    }

    conn->state = WS_STATE_CONNECTING;
    conn->wsi = lws_client_connect_via_info(&info);

    if (!conn->wsi) {
        obs_log(LOG_ERROR, "Failed to create WebSocket connection to %s", address);
        conn->state = WS_STATE_ERROR;
        return false;
    }

    lws_set_opaque_user_data(conn->wsi, conn);

    obs_log(LOG_INFO, "Connecting to %s WebSocket: %s",
            conn->is_remote ? "remote" : "OBS", address);

    return true;
}

// Main event loop thread
void *ws_relay_thread(void *data) {
    ws_relay_t *relay = (ws_relay_t *) data;

    obs_log(LOG_INFO, "WebSocket relay thread started");

    while (relay->running) {
        lws_service(relay->context, 0);

        time_t now = time(NULL);

        // Check for reconnection
        pthread_mutex_lock(&relay->mutex);

        // First priority: Connect to remote server if needed
        if (relay->remote_conn.state != WS_STATE_CONNECTED &&
            relay->remote_conn.state != WS_STATE_CONNECTING &&
            now - relay->last_reconnect_attempt >= relay->config.reconnect_interval) {
            if (strlen(relay->config.remote_ws_address) > 0) {
                obs_log(LOG_INFO, "Attempting to connect to remote server first");
                ws_connect(&relay->remote_conn, relay->config.remote_ws_address);
                relay->last_reconnect_attempt = now;
            }
        }

        // Second priority: Connect to OBS only if remote is connected
        if (relay->remote_conn.state == WS_STATE_CONNECTED &&
            relay->obs_conn.state != WS_STATE_CONNECTED &&
            relay->obs_conn.state != WS_STATE_CONNECTING &&
            now - relay->last_reconnect_attempt >= relay->config.reconnect_interval) {
            if (strlen(relay->config.local_obs_address) > 0) {
                obs_log(LOG_INFO, "Remote server connected, now connecting to OBS");
                ws_connect(&relay->obs_conn, relay->config.local_obs_address);
                relay->last_reconnect_attempt = now;
            }
        }

        // If remote disconnects, disconnect OBS as well
        if (relay->remote_conn.state != WS_STATE_CONNECTED &&
            relay->obs_conn.state == WS_STATE_CONNECTED &&
            relay->obs_conn.wsi) {
            obs_log(LOG_INFO, "Remote server disconnected, closing OBS connection");
            lws_close_reason(relay->obs_conn.wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
            relay->obs_conn.wsi = NULL;
            relay->obs_conn.state = WS_STATE_DISCONNECTED;
        }

        pthread_mutex_unlock(&relay->mutex);
    }

    obs_log(LOG_INFO, "WebSocket relay thread stopped");
    return NULL;
}
