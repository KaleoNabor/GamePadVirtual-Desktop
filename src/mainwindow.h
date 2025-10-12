#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QGroupBox>
#include <QProgressBar>
#include <QMap>
#include <QStatusBar>

#include "communication/connection_manager.h"
#include "virtual_gamepad/gamepad_manager.h"
#include "protocol/gamepad_packet.h" // Incluído para o tipo do slot

#include <Windows.h>
#include "ViGEm/Client.h"

using ButtonLabelMap = QMap<int, QLabel*>;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // ATUALIZADO: O tipo do pacote mudou para GamepadPacket
    void onGamepadStateUpdate(int playerIndex, const GamepadPacket& packet);
    void onPlayerConnected(int playerIndex, const QString& type);
    void onPlayerDisconnected(int playerIndex);
    void onLogMessage(const QString& message);

private:
    void setupUI();
    QWidget* createConnectionsTab();
    QWidget* createTestTab();
    void updateConnectionStatus();

    GamepadManager* m_gamepadManager;
    ConnectionManager* m_connectionManager;

    QLabel* m_networkStatusLabel;
    QLabel* m_btStatusLabel;

    QGroupBox* m_playerGroupBoxes[MAX_PLAYERS];
    ButtonLabelMap m_buttonLabels[MAX_PLAYERS];
    QProgressBar* m_leftTriggerBars[MAX_PLAYERS];
    QProgressBar* m_rightTriggerBars[MAX_PLAYERS];
    QLabel* m_leftStickLabels[MAX_PLAYERS];
    QLabel* m_rightStickLabels[MAX_PLAYERS];

    // ADICIONADO: Labels para os dados dos sensores
    QLabel* m_gyroLabels[MAX_PLAYERS];
    QLabel* m_accelLabels[MAX_PLAYERS];

    QString m_playerConnectionTypes[MAX_PLAYERS];

    const QString m_styleButtonPressed = "background-color: #4CAF50; color: white; border-radius: 5px; font-weight: bold;";
    const QString m_styleButtonReleased = "background-color: #424242; color: white; border-radius: 5px;";
};

#endif // MAINWINDOW_H