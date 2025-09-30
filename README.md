# OBS WebSocket Relay

A plugin for OBS Studio that creates a transparent WebSocket relay between the local OBS WebSocket server and a remote WebSocket server.

This allows you to forward OBS WebSocket messages to a remote server while maintaining transparent authentication and message handling.

## Prerequisites

OBS 31.1.1 or later

## Installation

Windows users: Use installer from [Releases](https://github.com/BlueGlassBlock/obs-ws-relay/releases).

For other platforms, please help yourself or fire an issue.

## Usage

Open `Tools > OBS WebSocket Relay Settings` to configure the plugin.

After successfully connecting to the remote server,
the plugin will try to establish a connection to the local OBS WebSocket server and start relaying messages.

## License

GPL-2.0