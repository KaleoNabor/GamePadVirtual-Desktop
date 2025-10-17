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

    // --- Membros privados ---
    GamepadManager* m_gamepadManager;          // Gerenciador de gamepads virtuais
    ConnectionManager* m_connectionManager;    // Gerenciador de conexões

    QLabel* m_networkStatusLabel;              // Status das conexões de rede
    QLabel* m_btStatusLabel;                   // Status das conexões Bluetooth

    QTabWidget* m_playerTabs;                  // Abas dos jogadores
    GamepadDisplayWidget* m_gamepadDisplays[MAX_PLAYERS];  // Displays visuais dos gamepads
    QLabel* m_gyroLabels[MAX_PLAYERS];         // Labels do giroscópio
    QLabel* m_accelLabels[MAX_PLAYERS];        // Labels do acelerômetro

    QString m_playerConnectionTypes[MAX_PLAYERS];  // Tipos de conexão dos jogadores
};

#endif // MAINWINDOW_H