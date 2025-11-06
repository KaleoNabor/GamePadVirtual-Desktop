#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTabWidget>
#include "communication/connection_manager.h"
#include "protocol/gamepad_packet.h"
#include "gamepaddisplaywidget.h"

class GamepadManager;

#define MAX_PLAYERS 8

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onGamepadStateUpdate(int playerIndex, const GamepadPacket& packet);
    void onPlayerConnected(int playerIndex, const QString& type);
    void onPlayerDisconnected(int playerIndex);
    void onLogMessage(const QString& message);
    void onDisconnectPlayerClicked(int playerIndex);

private:
    void setupUI();
    QWidget* createConnectionsTab();
    QWidget* createTestTab();
    void updateConnectionStatus();

    GamepadManager* m_gamepadManager;
    ConnectionManager* m_connectionManager;

    QLabel* m_networkStatusLabel;
    QLabel* m_btStatusLabel;
    QTabWidget* m_playerTabs;
    GamepadDisplayWidget* m_gamepadDisplays[MAX_PLAYERS];
    QLabel* m_gyroLabels[MAX_PLAYERS];
    QLabel* m_accelLabels[MAX_PLAYERS];
    QWidget* m_sensorWidgetWrappers[MAX_PLAYERS];

    QString m_playerConnectionTypes[MAX_PLAYERS];
    bool m_warningShown = false;
};

#endif