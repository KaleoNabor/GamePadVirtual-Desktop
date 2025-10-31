#include "mainwindow.h"
#include "gamepaddisplaywidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QStatusBar>
#include <QCloseEvent>
#include <QDebug>
#include <QNetworkInterface>
#include <QPushButton>
#include <QSpacerItem>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // Inicialização dos tipos de conexão dos jogadores
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerConnectionTypes[i] = "Nenhum";
    }

    // Criação do gerenciador de gamepads virtuais
    m_gamepadManager = new GamepadManager(this);
    // Criação do gerenciador de conexões
    m_connectionManager = new ConnectionManager(m_gamepadManager, this);

    // Conexão dos sinais para atualização da interface
    connect(m_gamepadManager, &GamepadManager::gamepadStateUpdated, this, &MainWindow::onGamepadStateUpdate);
    connect(m_gamepadManager, &GamepadManager::playerConnectedSignal, this, &MainWindow::onPlayerConnected);
    connect(m_gamepadManager, &GamepadManager::playerDisconnectedSignal, this, &MainWindow::onPlayerDisconnected);
    connect(m_connectionManager, &ConnectionManager::logMessage, this, &MainWindow::onLogMessage);

    setupUI();

    // Inicialização dos serviços
    if (m_gamepadManager->initialize()) {
        m_connectionManager->startServices();
    }
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI()
{
    setWindowTitle("Servidor GamePadVirtual");
    setMinimumSize(800, 600);

    // Criação do widget de abas principal
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

    // Seção de configuração de rede
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

    // Seção de configuração Bluetooth
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
    QWidget* mainTabContainer = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(mainTabContainer);

    // Criação das abas para cada jogador
    m_playerTabs = new QTabWidget();
    mainLayout->addWidget(m_playerTabs);

    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        QWidget* playerPage = new QWidget();
        QVBoxLayout* pageLayout = new QVBoxLayout(playerPage);

        // Layout dos botões
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        QPushButton* disconnectButton = new QPushButton("Desconectar Jogador");
        QPushButton* vibrateButton = new QPushButton("Testar Vibração");

        buttonLayout->addWidget(disconnectButton);
        buttonLayout->addWidget(vibrateButton);
        buttonLayout->addStretch();

        pageLayout->addLayout(buttonLayout); // Adiciona a barra de botões no topo da aba

        // Conecta os sinais dos botões
        connect(disconnectButton, &QPushButton::clicked, this, [this, i]() {
            onDisconnectPlayerClicked(i);
            });

        connect(vibrateButton, &QPushButton::clicked, m_gamepadManager, [this, i]() {
            m_gamepadManager->testVibration(i);
            });

        // Display visual do gamepad
        m_gamepadDisplays[i] = new GamepadDisplayWidget();
        pageLayout->addWidget(m_gamepadDisplays[i], 1); // O '1' faz o display se expandir

        // Container para dados dos sensores
        QWidget* sensorContainer = new QWidget();
        QHBoxLayout* sensorLayout = new QHBoxLayout(sensorContainer);

        m_gyroLabels[i] = new QLabel("Gyro: (0.00, 0.00, 0.00)");
        m_accelLabels[i] = new QLabel("Accel: (0.00, 0.00, 0.00)");

        sensorLayout->addStretch();
        sensorLayout->addWidget(m_gyroLabels[i]);
        sensorLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
        sensorLayout->addWidget(m_accelLabels[i]);
        sensorLayout->addStretch();

        pageLayout->addWidget(sensorContainer);

        m_playerTabs->addTab(playerPage, QString("❌ Jogador %1").arg(i + 1));
    }

    return mainTabContainer;
}

void MainWindow::onGamepadStateUpdate(int playerIndex, const GamepadPacket& packet)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    // Atualização da interface com dados do gamepad
    m_gamepadDisplays[playerIndex]->updateState(packet);

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

    // Atualização da interface para jogador conectado
    m_playerConnectionTypes[playerIndex] = type;
    m_playerTabs->setTabText(playerIndex, QString("✅ Jogador %1 (%2)").arg(playerIndex + 1).arg(type));
    updateConnectionStatus();
}

void MainWindow::onPlayerDisconnected(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    // Reset do estado do jogador desconectado
    m_playerConnectionTypes[playerIndex] = "Nenhum";
    m_playerTabs->setTabText(playerIndex, QString("❌ Jogador %1").arg(playerIndex + 1));
    m_gamepadDisplays[playerIndex]->resetState();

    m_gyroLabels[playerIndex]->setText("Gyro: (0.00, 0.00, 0.00)");
    m_accelLabels[playerIndex]->setText("Accel: (0.00, 0.00, 0.00)");

    updateConnectionStatus();
}

void MainWindow::onDisconnectPlayerClicked(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    // Se o jogador estiver conectado, força a desconexão
    if (m_playerConnectionTypes[playerIndex] != "Nenhum") {
        qDebug() << "Desconexão manual solicitada para o Jogador" << playerIndex + 1;

        // Esta é a mesma função que o ConnectionManager deve chamar
        // ao detectar uma desconexão. Ela limpa o gamepad virtual
        // e emite o sinal 'playerDisconnectedSignal'
        m_gamepadManager->playerDisconnected(playerIndex);
    }
}

void MainWindow::updateConnectionStatus()
{
    // Contagem de jogadores por tipo de conexão
    int networkCount = 0;
    int btCount = 0;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_playerConnectionTypes[i] == "Wi-Fi" || m_playerConnectionTypes[i] == "Ancoragem USB") networkCount++;
        else if (m_playerConnectionTypes[i] == "Bluetooth" || m_playerConnectionTypes[i] == "Bluetooth LE") btCount++;
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