#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTabWidget>
#include "communication/connection_manager.h"
#include "virtual_gamepad/gamepad_manager.h"
#include "protocol/gamepad_packet.h"
#include "gamepaddisplaywidget.h"

#define MAX_PLAYERS 4

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

private:
    void setupUI();
    QWidget* createConnectionsTab();
    QWidget* createTestTab();
    void updateConnectionStatus();

    // Gerenciador de gamepads virtuais
    GamepadManager* m_gamepadManager;
    // Gerenciador de conexões
    ConnectionManager* m_connectionManager;

    // Elementos da interface
    QLabel* m_networkStatusLabel;
    QLabel* m_btStatusLabel;
    QTabWidget* m_playerTabs;
    GamepadDisplayWidget* m_gamepadDisplays[MAX_PLAYERS];
    QLabel* m_gyroLabels[MAX_PLAYERS];
    QLabel* m_accelLabels[MAX_PLAYERS];

    // Armazenamento dos tipos de conexão
    QString m_playerConnectionTypes[MAX_PLAYERS];
};

#endif