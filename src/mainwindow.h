// mainwindow.h

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
    // Atualiza a interface com dados recebidos do gamepad
    void onGamepadStateUpdate(int playerIndex, const GamepadPacket& packet);

    // Gerencia conexão de novos jogadores
    void onPlayerConnected(int playerIndex, const QString& type);

    // Gerencia desconexão de jogadores
    void onPlayerDisconnected(int playerIndex);

    // Exibe mensagens de log na barra de status
    void onLogMessage(const QString& message);

    // Processa clique no botão de desconectar jogador
    void onDisconnectPlayerClicked(int playerIndex);

private:
    // Configura a interface gráfica principal
    void setupUI();

    // Cria a aba de configurações de conexão
    QWidget* createConnectionsTab();

    // Cria a aba de teste e configuração de controles
    QWidget* createTestTab();

    // Atualiza os status de conexão na interface
    void updateConnectionStatus();

    // Gerenciador de controles virtuais
    GamepadManager* m_gamepadManager;

    // Gerenciador de conexões de rede/bluetooth
    ConnectionManager* m_connectionManager;

    // Labels para status de conexão de rede
    QLabel* m_networkStatusLabel;
    QLabel* m_btStatusLabel;

    // Widget de abas para cada jogador
    QTabWidget* m_playerTabs;

    // Displays visuais dos controles
    GamepadDisplayWidget* m_gamepadDisplays[MAX_PLAYERS];

    // Labels para exibir dados dos sensores
    QLabel* m_gyroLabels[MAX_PLAYERS];
    QLabel* m_accelLabels[MAX_PLAYERS];

    // Containers para widgets de sensores
    QWidget* m_sensorWidgetWrappers[MAX_PLAYERS];

    // Tipos de conexão de cada jogador
    QString m_playerConnectionTypes[MAX_PLAYERS];

    // Flag para controle de exibição de avisos
    bool m_warningShown = false;
};

#endif