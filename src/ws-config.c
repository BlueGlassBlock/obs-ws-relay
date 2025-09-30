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

#include "ws-relay.h"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/config-file.h>
#include <util/dstr.h>
#include <string.h>

#define CONFIG_SECTION "ws_relay"

// Default configuration values
#define DEFAULT_LOCAL_OBS_ADDRESS "ws://localhost:4455"
#define DEFAULT_REMOTE_WS_ADDRESS ""
#define DEFAULT_RECONNECT_INTERVAL 5
#define DEFAULT_ENABLE_LOGGING false

void ws_relay_config_init(ws_relay_config_t *config) {
    if (!config)
        return;

    memset(config, 0, sizeof(ws_relay_config_t));

    config->local_obs_address = bstrdup(DEFAULT_LOCAL_OBS_ADDRESS);
    config->remote_ws_address = bstrdup(DEFAULT_REMOTE_WS_ADDRESS);
    config->reconnect_interval = DEFAULT_RECONNECT_INTERVAL;
    config->enable_logging = DEFAULT_ENABLE_LOGGING;
}

void ws_relay_config_free(ws_relay_config_t *config) {
    if (!config)
        return;

    bfree(config->local_obs_address);
    bfree(config->remote_ws_address);

    memset(config, 0, sizeof(ws_relay_config_t));
}

bool ws_relay_config_load(ws_relay_config_t *config) {
    if (!config)
        return false;

    config_t *obs_config = obs_frontend_get_app_config();
    if (!obs_config) {
        obs_log(LOG_WARNING, "Failed to get OBS global config");
        return false;
    }

    // Load configuration values
    const char *local_address = config_get_string(obs_config, CONFIG_SECTION, "local_obs_address");
    const char *remote_address = config_get_string(obs_config, CONFIG_SECTION, "remote_ws_address");

    if (local_address && strlen(local_address) > 0) {
        bfree(config->local_obs_address);
        config->local_obs_address = bstrdup(local_address);
    }

    if (remote_address && strlen(remote_address) > 0) {
        bfree(config->remote_ws_address);
        config->remote_ws_address = bstrdup(remote_address);
    }

    config->reconnect_interval = (int) config_get_int(obs_config, CONFIG_SECTION, "reconnect_interval");
    if (config->reconnect_interval <= 0) {
        config->reconnect_interval = DEFAULT_RECONNECT_INTERVAL;
    }

    config->enable_logging = config_get_bool(obs_config, CONFIG_SECTION, "enable_logging");

    obs_log(LOG_INFO, "Configuration loaded - Local: %s, Remote: %s, Reconnect: %ds, Logging: %s",
            config->local_obs_address, config->remote_ws_address, config->reconnect_interval,
            config->enable_logging ? "enabled" : "disabled");

    return true;
}

bool ws_relay_config_save(const ws_relay_config_t *config) {
    if (!config)
        return false;

    config_t *obs_config = obs_frontend_get_app_config();
    if (!obs_config) {
        obs_log(LOG_WARNING, "Failed to get OBS global config");
        return false;
    }

    // Save configuration values
    config_set_string(obs_config, CONFIG_SECTION, "local_obs_address", config->local_obs_address);
    config_set_string(obs_config, CONFIG_SECTION, "remote_ws_address", config->remote_ws_address);
    config_set_int(obs_config, CONFIG_SECTION, "reconnect_interval", config->reconnect_interval);
    config_set_bool(obs_config, CONFIG_SECTION, "enable_logging", config->enable_logging);

    config_save(obs_config);

    obs_log(LOG_INFO, "Configuration saved");

    // Stop and restart the relay if running
    if (global_relay) {
        ws_relay_stop(global_relay);
        ws_relay_destroy(global_relay);
        global_relay = ws_relay_create(config);

        if (config->remote_ws_address && strlen(config->remote_ws_address) > 0) {
            if (ws_relay_start(global_relay)) {
                obs_log(LOG_INFO, "WebSocket relay started successfully");
            } else {
                obs_log(LOG_ERROR, "Failed to start WebSocket relay");
            }
        }
    }

    return true;
}
