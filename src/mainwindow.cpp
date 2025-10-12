#include "mainwindow.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QCloseEvent>
#include <QDebug>
#include <QNetworkInterface>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerConnectionTypes[i] = "Nenhum";
    }

    m_gamepadManager = new GamepadManager(this);
    if (!m_gamepadManager->initialize()) {
        qCritical() << "ERRO CRÍTICO: Falha ao inicializar o Gamepad Manager (ViGEm).";
    }

    m_connectionManager = new ConnectionManager(m_gamepadManager, this);

    // Conectar sinais do GamepadManager para a MainWindow
    connect(m_gamepadManager, &GamepadManager::gamepadStateUpdated, this, &MainWindow::onGamepadStateUpdate);
    connect(m_gamepadManager, &GamepadManager::playerConnectedSignal, this, &MainWindow::onPlayerConnected);
    connect(m_gamepadManager, &GamepadManager::playerDisconnectedSignal, this, &MainWindow::onPlayerDisconnected);
    connect(m_connectionManager, &ConnectionManager::logMessage, this, &MainWindow::onLogMessage);

    setupUI();
    m_connectionManager->startServices();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI()
{
    setWindowTitle("Servidor GamePadVirtual");
    setMinimumSize(800, 600);
    QTabWidget* tabWidget = new QTabWidget();
    QWidget* connectionsTab = createConnectionsTab();
    QWidget* testTab = createTestTab();
    tabWidget->addTab(connectionsTab, "Conexões");
    tabWidget->addTab(testTab, "Teste de Controles");
    setCentralWidget(tabWidget);
}

QWidget* MainWindow::createConnectionsTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(tab);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // Grupo de Rede Unificado (TCP)
    QGroupBox* networkGroup = new QGroupBox("Conexão de Rede (Wi-Fi / Ancoragem USB)");
    QVBoxLayout* networkLayout = new QVBoxLayout(networkGroup);

    QLabel* wifiInstruction = new QLabel("<b>Para Wi-Fi:</b> Garanta que o PC e o celular estejam na mesma rede e use a Descoberta Automática no app.");
    wifiInstruction->setWordWrap(true);
    QLabel* usbInstruction = new QLabel("<b>Para USB:</b> Conecte o cabo, ative a 'Ancoragem USB' (USB Tethering) no celular e use a Descoberta Automática no app.");
    usbInstruction->setWordWrap(true);

    m_networkStatusLabel = new QLabel("Status: Aguardando Conexões...");

    networkLayout->addWidget(wifiInstruction);
    networkLayout->addWidget(usbInstruction);
    networkLayout->addWidget(m_networkStatusLabel);

    // Grupo Bluetooth
    QGroupBox* btGroup = new QGroupBox("Bluetooth");
    QVBoxLayout* btLayout = new QVBoxLayout(btGroup);
    m_btStatusLabel = new QLabel("Status: Aguardando Conexões...");
    btLayout->addWidget(m_btStatusLabel);

    mainLayout->addWidget(networkGroup);
    mainLayout->addWidget(btGroup);
    mainLayout->addStretch();

    return tab;
}

QWidget* MainWindow::createTestTab()
{
    QWidget* tab = new QWidget();
    QHBoxLayout* mainLayout = new QHBoxLayout(tab);
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerGroupBoxes[i] = new QGroupBox(QString("Jogador %1 (Desconectado)").arg(i + 1));
        QGridLayout* gridLayout = new QGridLayout(m_playerGroupBoxes[i]);
        m_buttonLabels[i][XUSB_GAMEPAD_A] = new QLabel("A");
        m_buttonLabels[i][XUSB_GAMEPAD_B] = new QLabel("B");
        m_buttonLabels[i][XUSB_GAMEPAD_X] = new QLabel("X");
        m_buttonLabels[i][XUSB_GAMEPAD_Y] = new QLabel("Y");
        m_gyroLabels[i] = new QLabel("Gyro: (0.00, 0.00, 0.00)");
        m_accelLabels[i] = new QLabel("Accel: (0.00, 0.00, 0.00)");

        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_Y], 0, 1, Qt::AlignCenter);
        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_X], 1, 0, Qt::AlignCenter);
        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_B], 1, 2, Qt::AlignCenter);
        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_A], 2, 1, Qt::AlignCenter);
        m_buttonLabels[i][XUSB_GAMEPAD_DPAD_UP] = new QLabel("▲");
        m_buttonLabels[i][XUSB_GAMEPAD_DPAD_DOWN] = new QLabel("▼");
        m_buttonLabels[i][XUSB_GAMEPAD_DPAD_LEFT] = new QLabel("◄");
        m_buttonLabels[i][XUSB_GAMEPAD_DPAD_RIGHT] = new QLabel("►");
        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_DPAD_UP], 0, 4, Qt::AlignCenter);
        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_DPAD_DOWN], 2, 4, Qt::AlignCenter);
        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_DPAD_LEFT], 1, 3, Qt::AlignCenter);
        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_DPAD_RIGHT], 1, 5, Qt::AlignCenter);
        m_buttonLabels[i][XUSB_GAMEPAD_LEFT_SHOULDER] = new QLabel("L1");
        m_buttonLabels[i][XUSB_GAMEPAD_RIGHT_SHOULDER] = new QLabel("R1");
        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_LEFT_SHOULDER], 3, 0, 1, 2, Qt::AlignCenter);
        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_RIGHT_SHOULDER], 3, 4, 1, 2, Qt::AlignCenter);
        m_leftTriggerBars[i] = new QProgressBar();
        m_leftTriggerBars[i]->setRange(0, 255);
        m_leftTriggerBars[i]->setFormat("L2");
        m_rightTriggerBars[i] = new QProgressBar();
        m_rightTriggerBars[i]->setRange(0, 255);
        m_rightTriggerBars[i]->setFormat("R2");
        gridLayout->addWidget(m_leftTriggerBars[i], 4, 0, 1, 2);
        gridLayout->addWidget(m_rightTriggerBars[i], 4, 4, 1, 2);
        m_buttonLabels[i][XUSB_GAMEPAD_BACK] = new QLabel("Select");
        m_buttonLabels[i][XUSB_GAMEPAD_START] = new QLabel("Start");
        m_buttonLabels[i][XUSB_GAMEPAD_LEFT_THUMB] = new QLabel("L3");
        m_buttonLabels[i][XUSB_GAMEPAD_RIGHT_THUMB] = new QLabel("R3");
        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_BACK], 5, 1, Qt::AlignCenter);
        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_START], 5, 4, Qt::AlignCenter);
        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_LEFT_THUMB], 7, 1, Qt::AlignCenter);
        gridLayout->addWidget(m_buttonLabels[i][XUSB_GAMEPAD_RIGHT_THUMB], 7, 4, Qt::AlignCenter);
        m_leftStickLabels[i] = new QLabel("LS: (0, 0)");
        m_rightStickLabels[i] = new QLabel("RS: (0, 0)");
        gridLayout->addWidget(m_leftStickLabels[i], 6, 0, 1, 2, Qt::AlignCenter);
        gridLayout->addWidget(m_rightStickLabels[i], 6, 4, 1, 2, Qt::AlignCenter);
        m_gyroLabels[i] = new QLabel("Gyro: (0.00, 0.00, 0.00)");
        m_accelLabels[i] = new QLabel("Accel: (0.00, 0.00, 0.00)");

        // Adiciona os novos labels ao layout da grid, na linha 8
        gridLayout->addWidget(m_gyroLabels[i], 8, 0, 1, 3, Qt::AlignCenter);
        gridLayout->addWidget(m_accelLabels[i], 8, 3, 1, 3, Qt::AlignCenter);
        // =========================================================================

        // Adiciona um espaçador para empurrar o conteúdo para cima
        gridLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding), 9, 0);

        mainLayout->addWidget(m_playerGroupBoxes[i]);
        for (QLabel* label : m_buttonLabels[i].values()) {
            label->setStyleSheet(m_styleButtonReleased);
            label->setMinimumSize(40, 25);
            label->setAlignment(Qt::AlignCenter);
        }
        mainLayout->addWidget(m_playerGroupBoxes[i]);
    }
    return tab;
}

void MainWindow::onGamepadStateUpdate(int playerIndex, const GamepadPacket& packet)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    // 1. Processa os botões
    for (int buttonFlag : m_buttonLabels[playerIndex].keys()) {
        QLabel* label = m_buttonLabels[playerIndex][buttonFlag];
        if (packet.buttons & buttonFlag) {
            label->setStyleSheet(m_styleButtonPressed);
        }
        else {
            label->setStyleSheet(m_styleButtonReleased);
        }
    }

    // 2. Processa os analógicos e gatilhos
    m_leftTriggerBars[playerIndex]->setValue(packet.leftTrigger);
    m_rightTriggerBars[playerIndex]->setValue(packet.rightTrigger);
    m_leftTriggerBars[playerIndex]->setFormat(QString("L2: %1").arg(packet.leftTrigger));
    m_rightTriggerBars[playerIndex]->setFormat(QString("R2: %1").arg(packet.rightTrigger));
    m_leftStickLabels[playerIndex]->setText(QString("LS: (%1, %2)").arg(packet.leftStickX).arg(packet.leftStickY));
    m_rightStickLabels[playerIndex]->setText(QString("RS: (%1, %2)").arg(packet.rightStickX).arg(packet.rightStickY));

    // 3. ATUALIZADO: Exibe os dados dos sensores
    // Os valores são divididos por 100.0 porque os enviámos multiplicados por 100 no Flutter
    m_gyroLabels[playerIndex]->setText(QString("Gyro: (%1, %2, %3)")
        .arg(packet.gyroX / 100.0, 0, 'f', 2)
        .arg(packet.gyroY / 100.0, 0, 'f', 2)
        .arg(packet.gyroZ / 100.0, 0, 'f', 2));

    m_accelLabels[playerIndex]->setText(QString("Accel: (%1, %2, %3)")
        .arg(packet.accelX / 100.0, 0, 'f', 2)
        .arg(packet.accelY / 100.0, 0, 'f', 2)
        .arg(packet.accelZ / 100.0, 0, 'f', 2));

}

void MainWindow::onPlayerConnected(int playerIndex, const QString& type)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;
    m_playerGroupBoxes[playerIndex]->setTitle(QString("Jogador %1 (Conectado - %2)").arg(playerIndex + 1).arg(type));
    m_playerConnectionTypes[playerIndex] = type;
    updateConnectionStatus();
}

void MainWindow::onPlayerDisconnected(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;
    m_playerGroupBoxes[playerIndex]->setTitle(QString("Jogador %1 (Desconectado)").arg(playerIndex + 1));
    m_playerConnectionTypes[playerIndex] = "Nenhum";

    // CORREÇÃO 3: Criar um GamepadPacket vazio em vez de XUSB_REPORT
    GamepadPacket emptyPacket = {}; // Isso inicializa todos os membros com zero

    // Agora a chamada da função tem o tipo correto de argumento
    onGamepadStateUpdate(playerIndex, emptyPacket);

    updateConnectionStatus();
}

void MainWindow::updateConnectionStatus()
{
    int networkCount = 0;
    int btCount = 0;
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_playerConnectionTypes[i] == "Wi-Fi" || m_playerConnectionTypes[i] == "Ancoragem USB") networkCount++;
        else if (m_playerConnectionTypes[i] == "Bluetooth") btCount++;
    }

    m_networkStatusLabel->setText(networkCount > 0 ? QString("Status: %1 jogador(es) conectado(s).").arg(networkCount) : "Status: Aguardando Conexões...");
    m_btStatusLabel->setText(btCount > 0 ? QString("Status: %1 jogador(es) conectado(s).").arg(btCount) : "Status: Aguardando Conexões...");
}

void MainWindow::onLogMessage(const QString& message)
{
    statusBar()->showMessage(message, 5000);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    qDebug() << "Fechando a aplicacao...";
    m_connectionManager->stopServices();
    event->accept();
}