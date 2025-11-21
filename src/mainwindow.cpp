#include "mainwindow.h"
#include "gamepaddisplaywidget.h"
#include "communication/connection_manager.h"
#include "virtual_gamepad/gamepad_manager.h"
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
#include <QMessageBox>
#include <QComboBox>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerConnectionTypes[i] = "Nenhum";
    }

    m_gamepadManager = new GamepadManager(this);
    m_connectionManager = new ConnectionManager(m_gamepadManager, this);

    // Conexões de estado do gamepad
    connect(m_gamepadManager, &GamepadManager::gamepadStateUpdated, this, &MainWindow::onGamepadStateUpdate);
    connect(m_connectionManager, &ConnectionManager::logMessage, this, &MainWindow::onLogMessage);
    connect(m_gamepadManager, &GamepadManager::dsuClientConnected, this, &MainWindow::onDsuClientConnected);
    connect(m_gamepadManager, &GamepadManager::dsuClientDisconnected, this, &MainWindow::onDsuClientDisconnected);

    // Conexões de jogadores - ConnectionManager para GamepadManager
    connect(m_connectionManager, &ConnectionManager::playerConnected,
        m_gamepadManager, &GamepadManager::playerConnected);
    connect(m_connectionManager, &ConnectionManager::playerDisconnected,
        m_gamepadManager, &GamepadManager::playerDisconnected);

    // Conexões de jogadores - ConnectionManager para Interface
    connect(m_connectionManager, &ConnectionManager::playerConnected,
        this, &MainWindow::onPlayerConnected);
    connect(m_connectionManager, &ConnectionManager::playerDisconnected,
        this, &MainWindow::onPlayerDisconnected);

    setupUI();

    if (m_gamepadManager->initialize()) {
        m_connectionManager->startServices();
    }
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
    tabWidget->addTab(testTab, "Configurações de Controle");

    setCentralWidget(tabWidget);
}

QWidget* MainWindow::createConnectionsTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(tab);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // --- NOVO: GRUPO DE TRANSMISSÃO ---
    QGroupBox* streamGroup = new QGroupBox("Transmissão de Tela (Stream)");
    QVBoxLayout* streamLayout = new QVBoxLayout(streamGroup);

    QLabel* infoLabel = new QLabel(
        "⚠️ <b>Recurso Experimental</b><br>"
        "Ativar a transmissão consome recursos do PC (CPU/GPU) e banda da rede Wi-Fi.<br>"
        "Para melhor performance, use uma rede 5GHz e mantenha os drivers de vídeo atualizados."
    );
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #555;");

    // O BOTÃO MESTRE
    QPushButton* toggleStreamBtn = new QPushButton("Ativar Transmissão de Tela");
    toggleStreamBtn->setCheckable(true);
    toggleStreamBtn->setFixedHeight(40);
    toggleStreamBtn->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; font-weight: bold; border-radius: 4px; }"
        "QPushButton:checked { background-color: #F44336; }"
    );

    // Conexão do Botão
    connect(toggleStreamBtn, &QPushButton::toggled, this, [this, toggleStreamBtn](bool checked) {
        if (checked) {
            toggleStreamBtn->setText("Parar Transmissão");
            m_connectionManager->setStreamingEnabled(true);
        }
        else {
            toggleStreamBtn->setText("Ativar Transmissão de Tela");
            m_connectionManager->setStreamingEnabled(false);
        }
        });

    streamLayout->addWidget(infoLabel);
    streamLayout->addWidget(toggleStreamBtn);

    // Adiciona ao topo
    mainLayout->addWidget(streamGroup);

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

    QGroupBox* btGroup = new QGroupBox("Bluetooth");
    QVBoxLayout* btLayout = new QVBoxLayout(btGroup);
    QLabel* btDevLabel = new QLabel("<i>Ainda em desenvolvimento...</i>");
    btDevLabel->setAlignment(Qt::AlignCenter);
    btLayout->addWidget(btDevLabel);
    mainLayout->addWidget(btGroup);

    QGroupBox* dsuGroup = new QGroupBox("Servidor de Movimento (Cemuhook DSU)");
    QVBoxLayout* dsuLayout = new QVBoxLayout(dsuGroup);

    QLabel* dsuInfo = new QLabel("Fornece dados de movimento para emuladores (ex: Cemu, Yuzu) quando o controle está no modo 'Xbox 360'.");
    dsuInfo->setWordWrap(true);

    m_cemuhookStatusLabel = new QLabel("Status: Inativo. (Aguardando na porta 26760)");
    m_cemuhookStatusLabel->setStyleSheet("color: #FF9800;");

    dsuLayout->addWidget(dsuInfo);
    dsuLayout->addWidget(m_cemuhookStatusLabel);

    mainLayout->addWidget(networkGroup);
    mainLayout->addWidget(btGroup);
    mainLayout->addWidget(dsuGroup);
    mainLayout->addStretch();

    return tab;
}

QWidget* MainWindow::createTestTab()
{
    QWidget* mainTabContainer = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(mainTabContainer);

    m_playerTabs = new QTabWidget();
    mainLayout->addWidget(m_playerTabs);

    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        QWidget* playerPage = new QWidget();
        QVBoxLayout* pageLayout = new QVBoxLayout(playerPage);

        QHBoxLayout* buttonLayout = new QHBoxLayout();
        QPushButton* disconnectButton = new QPushButton("Desconectar Jogador");
        QPushButton* vibrateButton = new QPushButton("Testar Vibração");

        m_controllerTypeSelectors[i] = new QComboBox();
        m_controllerTypeSelectors[i]->addItem("Xbox 360", 0);
        m_controllerTypeSelectors[i]->addItem("DualShock 4", 1);
        m_controllerTypeSelectors[i]->setToolTip("Muda o tipo de controle virtual. A mudança ocorre na próxima conexão.");
        m_controllerTypeSelectors[i]->setCurrentIndex(1);

        buttonLayout->addWidget(disconnectButton);
        buttonLayout->addWidget(vibrateButton);
        buttonLayout->addWidget(m_controllerTypeSelectors[i]);
        buttonLayout->addStretch();

        pageLayout->addLayout(buttonLayout);

        connect(disconnectButton, &QPushButton::clicked, this, [this, i]() {
            onDisconnectPlayerClicked(i);
            });

        connect(vibrateButton, &QPushButton::clicked, m_gamepadManager, [this, i]() {
            m_gamepadManager->testVibration(i);
            });

        connect(m_controllerTypeSelectors[i], QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_gamepadManager, [this, i](int index) {
                m_gamepadManager->onControllerTypeChanged(i, index);
                m_gamepadDisplays[i]->setControllerType(index);
            });

        m_gamepadDisplays[i] = new GamepadDisplayWidget();
        m_gamepadDisplays[i]->setControllerType(1);
        pageLayout->addWidget(m_gamepadDisplays[i], 1);

        m_sensorWidgetWrappers[i] = new QWidget();
        QVBoxLayout* sensorMainLayout = new QVBoxLayout(m_sensorWidgetWrappers[i]);
        sensorMainLayout->setContentsMargins(0, 5, 0, 5);

        QWidget* dataRowWidget = new QWidget();
        QHBoxLayout* sensorDataLayout = new QHBoxLayout(dataRowWidget);

        m_gyroLabels[i] = new QLabel("Gyro Celular: (0.00, 0.00, 0.00)");
        m_accelLabels[i] = new QLabel("Accel Celular: (0.00, 0.00, 0.00)");
        m_gyroLabels[i]->setStyleSheet("color: #4CAF50;");
        m_accelLabels[i]->setStyleSheet("color: #4CAF50;");

        sensorDataLayout->addStretch();
        sensorDataLayout->addWidget(m_gyroLabels[i]);
        sensorDataLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
        sensorDataLayout->addWidget(m_accelLabels[i]);
        sensorDataLayout->addStretch();

        sensorMainLayout->addWidget(dataRowWidget);
        pageLayout->addWidget(m_sensorWidgetWrappers[i]);
        m_sensorWidgetWrappers[i]->setVisible(true);

        m_playerTabs->addTab(playerPage, QString("❌ Jogador %1").arg(i + 1));

        if (i >= 4) {
            m_playerTabs->setTabVisible(i, false);
        }
    }

    return mainTabContainer;
}

void MainWindow::onGamepadStateUpdate(int playerIndex, const GamepadPacket& packet)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    m_gamepadDisplays[playerIndex]->updateState(packet);

    m_gyroLabels[playerIndex]->setText(QString("Gyro Celular: (%1, %2, %3)")
        .arg(packet.gyroX / 100.0, 0, 'f', 2)
        .arg(packet.gyroY / 100.0, 0, 'f', 2)
        .arg(packet.gyroZ / 100.0, 0, 'f', 2));

    m_accelLabels[playerIndex]->setText(QString("Accel Celular: (%1, %2, %3)")
        .arg(packet.accelX / 4096.0, 0, 'f', 2)
        .arg(packet.accelY / 4096.0, 0, 'f', 2)
        .arg(packet.accelZ / 4096.0, 0, 'f', 2));
}

void MainWindow::onPlayerConnected(int playerIndex, const QString& type)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    m_playerConnectionTypes[playerIndex] = type;
    m_playerTabs->setTabText(playerIndex, QString("✅ Jogador %1 (%2)").arg(playerIndex + 1).arg(type));

    if (playerIndex >= 4) {
        m_playerTabs->setTabVisible(playerIndex, true);

        if (!m_warningShown) {
            QMessageBox::information(this, "Limite Estendido",
                "Mais de 4 jogadores conectados. O limite do servidor foi estendido para 8 jogadores.");
            m_warningShown = true;
        }
    }

    updateConnectionStatus();
}

void MainWindow::onPlayerDisconnected(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    m_playerConnectionTypes[playerIndex] = "Nenhum";
    m_playerTabs->setTabText(playerIndex, QString("❌ Jogador %1").arg(playerIndex + 1));

    m_sensorWidgetWrappers[playerIndex]->setVisible(true);
    m_gamepadDisplays[playerIndex]->resetState();

    m_gyroLabels[playerIndex]->setText("Gyro Celular: (0.00, 0.00, 0.00)");
    m_accelLabels[playerIndex]->setText("Accel Celular: (0.00, 0.00, 0.00)");

    if (playerIndex >= 4) {
        m_playerTabs->setTabVisible(playerIndex, false);

        bool extraPlayersActive = false;
        for (int i = 4; i < MAX_PLAYERS; ++i) {
            if (m_playerConnectionTypes[i] != "Nenhum") {
                extraPlayersActive = true;
                break;
            }
        }

        if (!extraPlayersActive) {
            m_warningShown = false;
        }
    }

    updateConnectionStatus();
}

void MainWindow::onDisconnectPlayerClicked(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    if (m_playerConnectionTypes[playerIndex] != "Nenhum") {
        const QString& type = m_playerConnectionTypes[playerIndex];
        m_connectionManager->forceDisconnectPlayer(playerIndex, type);
    }
}

void MainWindow::updateConnectionStatus()
{
    int networkCount = 0;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_playerConnectionTypes[i] == "Wi-Fi" || m_playerConnectionTypes[i] == "Ancoragem USB") networkCount++;
    }

    m_networkStatusLabel->setText(networkCount > 0 ? QString("Status: %1 jogador(es) conectado(s).").arg(networkCount) : "Status: Aguardando Conexões...");
}

void MainWindow::onLogMessage(const QString& message)
{
    statusBar()->showMessage(message, 5000);
}

void MainWindow::onDsuClientConnected(const QString& address, quint16 port)
{
    if (m_cemuhookStatusLabel) {
        m_cemuhookStatusLabel->setText(QString("Status: Conectado ao cliente %1:%2").arg(address).arg(port));
        m_cemuhookStatusLabel->setStyleSheet("color: #4CAF50;");
    }
}

void MainWindow::onDsuClientDisconnected()
{
    if (m_cemuhookStatusLabel) {
        m_cemuhookStatusLabel->setText("Status: Inativo. (Aguardando na porta 26760)");
        m_cemuhookStatusLabel->setStyleSheet("color: #FF9800;");
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    qDebug() << "Fechando a aplicacao...";

    // Desconecta sinais do GamepadManager
    disconnect(m_gamepadManager, &GamepadManager::gamepadStateUpdated,
        this, &MainWindow::onGamepadStateUpdate);

    // Desconecta sinais do ConnectionManager
    disconnect(m_connectionManager, &ConnectionManager::playerConnected,
        this, &MainWindow::onPlayerConnected);
    disconnect(m_connectionManager, &ConnectionManager::playerDisconnected,
        this, &MainWindow::onPlayerDisconnected);
    disconnect(m_connectionManager, &ConnectionManager::logMessage,
        this, &MainWindow::onLogMessage);

    // Para os serviços com segurança
    m_connectionManager->stopServices();

    event->accept();
}