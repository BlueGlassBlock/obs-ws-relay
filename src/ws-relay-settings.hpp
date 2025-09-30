/*
OBS WebSocket Relay - Settings UI Header
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

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include "ws-relay.h"

class WSRelaySettingsDialog : public QDialog
{
    Q_OBJECT

private:
    QLineEdit *localAddressEdit;
    QLineEdit *remoteAddressEdit;
    QSpinBox *reconnectIntervalSpin;
    QCheckBox *enableLoggingCheck;
    QLabel *statusLabel;
    QPushButton *testConnectionBtn;

    ws_relay_config_t current_config;

public:
    WSRelaySettingsDialog(QWidget *parent = nullptr);
    ~WSRelaySettingsDialog();

    void LoadSettings();
    void SaveSettings();

signals:
    void SettingsChanged();

private slots:
    void OnTestConnection();
    void OnAccepted();
    void OnRejected();
    void OnSettingsChanged();

private:
    void UpdateConnectionStatus();
    void SetupUI();
};

// C interface functions
extern "C" {
    void ws_relay_show_settings();
    void ws_relay_hide_settings();
    void ws_relay_cleanup_settings();
}
