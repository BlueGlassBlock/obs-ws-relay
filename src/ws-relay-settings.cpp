/*
OBS WebSocket Relay - Settings UI
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

#include "ws-relay-settings.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/config-file.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>

WSRelaySettingsDialog::WSRelaySettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("WebSocket Relay Settings");
    setModal(true);
    resize(500, 350);

    ws_relay_config_init(&current_config);
    SetupUI();
    LoadSettings();
}

WSRelaySettingsDialog::~WSRelaySettingsDialog()
{
    ws_relay_config_free(&current_config);
}

void WSRelaySettingsDialog::SetupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Connection settings group
    QGroupBox *connectionGroup = new QGroupBox("Connection Settings");
    QFormLayout *connectionLayout = new QFormLayout(connectionGroup);

    localAddressEdit = new QLineEdit();
    localAddressEdit->setPlaceholderText("ws://localhost:4455");
    connectionLayout->addRow("Local OBS Address:", localAddressEdit);

    remoteAddressEdit = new QLineEdit();
    remoteAddressEdit->setPlaceholderText("wss://example.com:8080/ws");
    connectionLayout->addRow("Remote WebSocket Address:", remoteAddressEdit);

    reconnectIntervalSpin = new QSpinBox();
    reconnectIntervalSpin->setRange(1, 300);
    reconnectIntervalSpin->setSuffix(" seconds");
    connectionLayout->addRow("Reconnect Interval:", reconnectIntervalSpin);

    enableLoggingCheck = new QCheckBox("Enable verbose logging");
    connectionLayout->addRow(enableLoggingCheck);

    mainLayout->addWidget(connectionGroup);

    // Status group
    QGroupBox *statusGroup = new QGroupBox("Status");
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);

    statusLabel = new QLabel("Disconnected");
    statusLayout->addWidget(statusLabel);

    testConnectionBtn = new QPushButton("Test Connection");
    statusLayout->addWidget(testConnectionBtn);

    mainLayout->addWidget(statusGroup);

    // Button box
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);

    mainLayout->addWidget(buttonBox);

    // Connect signals
    connect(buttonBox, &QDialogButtonBox::accepted, this, &WSRelaySettingsDialog::OnAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &WSRelaySettingsDialog::OnRejected);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &WSRelaySettingsDialog::SaveSettings);

    connect(testConnectionBtn, &QPushButton::clicked, this, &WSRelaySettingsDialog::OnTestConnection);

    connect(localAddressEdit, &QLineEdit::textChanged, this, &WSRelaySettingsDialog::OnSettingsChanged);
    connect(remoteAddressEdit, &QLineEdit::textChanged, this, &WSRelaySettingsDialog::OnSettingsChanged);
    connect(reconnectIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &WSRelaySettingsDialog::OnSettingsChanged);
    connect(enableLoggingCheck, &QCheckBox::toggled, this, &WSRelaySettingsDialog::OnSettingsChanged);
}

void WSRelaySettingsDialog::LoadSettings()
{
    if (ws_relay_config_load(&current_config)) {
        localAddressEdit->setText(current_config.local_obs_address);
        remoteAddressEdit->setText(current_config.remote_ws_address);
        reconnectIntervalSpin->setValue(current_config.reconnect_interval);
        enableLoggingCheck->setChecked(current_config.enable_logging);
    }

    UpdateConnectionStatus();
}

void WSRelaySettingsDialog::SaveSettings()
{
    // Update config from UI
    bfree(current_config.local_obs_address);
    bfree(current_config.remote_ws_address);

    current_config.local_obs_address = bstrdup(localAddressEdit->text().toUtf8().constData());
    current_config.remote_ws_address = bstrdup(remoteAddressEdit->text().toUtf8().constData());
    current_config.reconnect_interval = reconnectIntervalSpin->value();
    current_config.enable_logging = enableLoggingCheck->isChecked();

    if (ws_relay_config_save(&current_config)) {
        QMessageBox::information(this, "WebSocket Relay Settings", "Settings saved successfully!");
        obs_log(LOG_INFO, "WebSocket relay settings updated from UI");

        // Emit signal to notify that settings have changed
        emit SettingsChanged();
    } else {
        QMessageBox::warning(this, "WebSocket Relay Settings", "Failed to save settings!");
    }
}

void WSRelaySettingsDialog::OnTestConnection()
{
    testConnectionBtn->setEnabled(false);
    testConnectionBtn->setText("Testing...");

    QString remote_addr = remoteAddressEdit->text().trimmed();
    if (remote_addr.isEmpty()) {
        QMessageBox::warning(this, "Test Connection", "Please enter a remote WebSocket address first.");
    } else {
        // For now, just show a message. In a real implementation, you would
        // create a temporary connection to test
        QMessageBox::information(this, "Test Connection",
            QString("Connection test for: %1\n\nNote: Full connection testing will be implemented in a future version.").arg(remote_addr));
    }

    testConnectionBtn->setEnabled(true);
    testConnectionBtn->setText("Test Connection");
}

void WSRelaySettingsDialog::OnAccepted()
{
    SaveSettings();
    accept();
}

void WSRelaySettingsDialog::OnRejected()
{
    reject();
}

void WSRelaySettingsDialog::OnSettingsChanged()
{
    UpdateConnectionStatus();
}

void WSRelaySettingsDialog::UpdateConnectionStatus()
{
    if (remoteAddressEdit->text().trimmed().isEmpty()) {
        statusLabel->setText("Status: Not configured");
        statusLabel->setStyleSheet("color: orange;");
    } else {
        statusLabel->setText("Status: Configured");
        statusLabel->setStyleSheet("color: green;");
    }
}

// Global settings dialog instance
static WSRelaySettingsDialog *settings_dialog = nullptr;

extern "C" {

void ws_relay_show_settings()
{
    if (!settings_dialog) {
        QWidget *main_window = static_cast<QWidget*>(obs_frontend_get_main_window());
        settings_dialog = new WSRelaySettingsDialog(main_window);
    }

    settings_dialog->LoadSettings();
    settings_dialog->show();
    settings_dialog->raise();
    settings_dialog->activateWindow();
}

void ws_relay_hide_settings()
{
    if (settings_dialog) {
        settings_dialog->hide();
    }
}

void ws_relay_cleanup_settings()
{
    if (settings_dialog) {
        delete settings_dialog;
        settings_dialog = nullptr;
    }
}

}

