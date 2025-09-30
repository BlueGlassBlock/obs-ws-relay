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

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include "ws-relay.h"

ws_relay_t *global_relay = NULL;

// Menu action for settings
static void on_settings_menu_triggered(void *data)
{
    UNUSED_PARAMETER(data);
    ws_relay_show_settings();
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static void on_obs_frontend_event(enum obs_frontend_event event, void *data)
{
    UNUSED_PARAMETER(data);
    
    switch (event) {
    case OBS_FRONTEND_EVENT_STREAMING_STARTED:
        obs_log(LOG_INFO, "Streaming started - relay should be active");
        break;
    case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
        obs_log(LOG_INFO, "Streaming stopped");
        break;
    case OBS_FRONTEND_EVENT_EXIT:
        obs_log(LOG_INFO, "OBS exiting - stopping relay");
        if (global_relay) {
            ws_relay_stop(global_relay);
        }
        break;
    default:
        break;
    }
}

bool obs_module_load(void)
{
    obs_log(LOG_INFO, "OBS WebSocket Relay plugin loaded successfully (version %s)", PLUGIN_VERSION);
    
    // Load configuration
    ws_relay_config_t config;
    ws_relay_config_init(&config);
    
    if (!ws_relay_config_load(&config)) {
        obs_log(LOG_WARNING, "Failed to load configuration, using defaults");
    }

    // Save configuration if it was initialized with defaults
    ws_relay_config_save(&config);

    // Create and start the relay
    global_relay = ws_relay_create(&config);
    if (!global_relay) {
        obs_log(LOG_ERROR, "Failed to create WebSocket relay");
        ws_relay_config_free(&config);
        return false;
    }

    // Start the relay if addresses are configured
    if (config.remote_ws_address && strlen(config.remote_ws_address) > 0) {
        if (ws_relay_start(global_relay)) {
            obs_log(LOG_INFO, "WebSocket relay started successfully");
        } else {
            obs_log(LOG_ERROR, "Failed to start WebSocket relay");
        }
    } else {
        obs_log(LOG_INFO, "Remote WebSocket address not configured - relay not started");
        obs_log(LOG_INFO, "Please configure the remote address in OBS settings");
    }
    
    // Register frontend event callback
    obs_frontend_add_event_callback(on_obs_frontend_event, NULL);
    
    // Add settings menu item to Tools menu
    obs_frontend_add_tools_menu_item("WebSocket Relay Settings", on_settings_menu_triggered, NULL);

    ws_relay_config_free(&config);
    return true;
}

void obs_module_unload(void)
{
    obs_log(LOG_INFO, "Unloading WebSocket relay plugin");
    
    // Clean up settings UI
    ws_relay_cleanup_settings();

    // Remove frontend event callback
    obs_frontend_remove_event_callback(on_obs_frontend_event, NULL);
    
    // Stop and destroy the relay
    if (global_relay) {
        ws_relay_stop(global_relay);
        ws_relay_destroy(global_relay);
        global_relay = NULL;
    }
    
    obs_log(LOG_INFO, "WebSocket relay plugin unloaded");
}
