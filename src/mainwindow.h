#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTabWidget>
#include <QComboBox>
#include "controller_types.h"
#include "communication/connection_manager.h"
#include "protocol/gamepad_packet.h"
#include "gamepaddisplaywidget.h"

class GamepadManager;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // Sistema de atualização de estado
    void onGamepadStateUpdate(int playerIndex, const GamepadPacket& packet);
    void onPlayerConnected(int playerIndex, const QString& type);
    void onPlayerDisconnected(int playerIndex);
    void onLogMessage(const QString& message);
    void onDisconnectPlayerClicked(int playerIndex);

    // Sistema de status Cemuhook DSU
    void onDsuClientConnected(const QString& address, quint16 port);
    void onDsuClientDisconnected();

private:
    // Inicialização da interface gráfica
    void setupUI();
    QWidget* createConnectionsTab();
    QWidget* createTestTab();
    void updateConnectionStatus();

    // Componentes principais do sistema
    GamepadManager* m_gamepadManager;
    ConnectionManager* m_connectionManager;

    // Sistema de status e feedback
    QLabel* m_networkStatusLabel;
    QLabel* m_cemuhookStatusLabel;

    // Sistema de exibição dos jogadores
    QTabWidget* m_playerTabs;
    GamepadDisplayWidget* m_gamepadDisplays[MAX_PLAYERS];
    QLabel* m_gyroLabels[MAX_PLAYERS];
    QLabel* m_accelLabels[MAX_PLAYERS];
    QWidget* m_sensorWidgetWrappers[MAX_PLAYERS];
    QComboBox* m_controllerTypeSelectors[MAX_PLAYERS];

    // Estado interno da aplicação
    QString m_playerConnectionTypes[MAX_PLAYERS];
    bool m_warningShown = false;
};

#endif