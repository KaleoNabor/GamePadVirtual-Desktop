// mainwindow.cpp

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
#include <QMessageBox>

// Construtor principal - inicializa componentes e conexões
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // Inicializa tipos de conexão dos jogadores
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerConnectionTypes[i] = "Nenhum";
    }

    // Cria gerenciadores principais
    m_gamepadManager = new GamepadManager(this);
    m_connectionManager = new ConnectionManager(m_gamepadManager, this);

    // Conecta sinais entre componentes
    connect(m_gamepadManager, &GamepadManager::gamepadStateUpdated, this, &MainWindow::onGamepadStateUpdate);
    connect(m_gamepadManager, &GamepadManager::playerConnectedSignal, this, &MainWindow::onPlayerConnected);
    connect(m_gamepadManager, &GamepadManager::playerDisconnectedSignal, this, &MainWindow::onPlayerDisconnected);
    connect(m_connectionManager, &ConnectionManager::logMessage, this, &MainWindow::onLogMessage);

    // Configura interface e inicia serviços
    setupUI();

    if (m_gamepadManager->initialize()) {
        m_connectionManager->startServices();
    }
}

// Destrutor
MainWindow::~MainWindow() {}

// Configura a interface gráfica principal
void MainWindow::setupUI()
{
    setWindowTitle("Servidor GamePadVirtual");
    setMinimumSize(800, 600);

    // Cria sistema de abas principal
    QTabWidget* tabWidget = new QTabWidget();
    QWidget* connectionsTab = createConnectionsTab();
    QWidget* testTab = createTestTab();

    tabWidget->addTab(connectionsTab, "Conexões");
    tabWidget->addTab(testTab, "Configurações de Controle");

    setCentralWidget(tabWidget);
}

// Cria a aba de configurações de conexão
QWidget* MainWindow::createConnectionsTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(tab);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // Grupo para conexões de rede (Wi-Fi/USB)
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

    // Grupo para conexões Bluetooth
    QGroupBox* btGroup = new QGroupBox("Bluetooth");
    QVBoxLayout* btLayout = new QVBoxLayout(btGroup);

    m_btStatusLabel = new QLabel("Status: Aguardando Conexões...");
    btLayout->addWidget(m_btStatusLabel);

    mainLayout->addWidget(networkGroup);
    mainLayout->addWidget(btGroup);
    mainLayout->addStretch();

    return tab;
}

// Cria a aba de teste e configuração de controles
QWidget* MainWindow::createTestTab()
{
    QWidget* mainTabContainer = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(mainTabContainer);

    m_playerTabs = new QTabWidget();
    mainLayout->addWidget(m_playerTabs);

    // Cria uma página de configuração para cada jogador
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        QWidget* playerPage = new QWidget();
        QVBoxLayout* pageLayout = new QVBoxLayout(playerPage);

        // Layout superior com botões de ação
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        QPushButton* disconnectButton = new QPushButton("Desconectar Jogador");
        QPushButton* vibrateButton = new QPushButton("Testar Vibração");

        buttonLayout->addWidget(disconnectButton);
        buttonLayout->addWidget(vibrateButton);
        buttonLayout->addStretch();

        pageLayout->addLayout(buttonLayout);

        // Conecta botões às funções correspondentes
        connect(disconnectButton, &QPushButton::clicked, this, [this, i]() {
            onDisconnectPlayerClicked(i);
            });

        connect(vibrateButton, &QPushButton::clicked, m_gamepadManager, [this, i]() {
            m_gamepadManager->testVibration(i);
            });

        // Display visual do controle
        m_gamepadDisplays[i] = new GamepadDisplayWidget();
        pageLayout->addWidget(m_gamepadDisplays[i], 1);

        // Container para dados dos sensores
        m_sensorWidgetWrappers[i] = new QWidget();
        QVBoxLayout* sensorMainLayout = new QVBoxLayout(m_sensorWidgetWrappers[i]);
        sensorMainLayout->setContentsMargins(0, 5, 0, 5);

        // Linha horizontal para dados dos sensores
        QWidget* dataRowWidget = new QWidget();
        QHBoxLayout* sensorDataLayout = new QHBoxLayout(dataRowWidget);

        // Labels para dados dos sensores do celular
        m_gyroLabels[i] = new QLabel("Gyro Celular: (0.00, 0.00, 0.00)");
        m_accelLabels[i] = new QLabel("Accel Celular: (0.00, 0.00, 0.00)");

        m_gyroLabels[i]->setStyleSheet("color: #4CAF50;");
        m_accelLabels[i]->setStyleSheet("color: #4CAF50;");

        // Organiza labels na linha horizontal
        sensorDataLayout->addStretch();
        sensorDataLayout->addWidget(m_gyroLabels[i]);
        sensorDataLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
        sensorDataLayout->addWidget(m_accelLabels[i]);
        sensorDataLayout->addStretch();

        // Label informativo sobre servidor Cemuhook
        QLabel* cemuhookLabel = new QLabel(
            QString("Servidor de Movimento (CemuhookUDP): 127.0.0.1:%1")
            .arg(26760)
        );
        cemuhookLabel->setAlignment(Qt::AlignCenter);
        cemuhookLabel->setStyleSheet("color: #AAA; font-size: 10px;");

        // Adiciona componentes ao layout principal dos sensores
        sensorMainLayout->addWidget(dataRowWidget);
        sensorMainLayout->addWidget(cemuhookLabel);

        pageLayout->addWidget(m_sensorWidgetWrappers[i]);

        // Garante que os sensores estejam sempre visíveis
        m_sensorWidgetWrappers[i]->setVisible(true);

        // Adiciona aba do jogador
        m_playerTabs->addTab(playerPage, QString("❌ Jogador %1").arg(i + 1));

        // Esconde abas 5-8 inicialmente
        if (i >= 4) {
            m_playerTabs->setTabVisible(i, false);
        }
    }

    return mainTabContainer;
}

// Atualiza interface com dados recebidos do gamepad
void MainWindow::onGamepadStateUpdate(int playerIndex, const GamepadPacket& packet)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    // Atualiza display visual do controle
    m_gamepadDisplays[playerIndex]->updateState(packet);

    // O Giroscópio está correto (dividido por 100.0)
    m_gyroLabels[playerIndex]->setText(QString("Gyro Celular: (%1, %2, %3)")
        .arg(packet.gyroX / 100.0, 0, 'f', 2)
        .arg(packet.gyroY / 100.0, 0, 'f', 2)
        .arg(packet.gyroZ / 100.0, 0, 'f', 2));

    // --- CORREÇÃO AQUI ---
    // Mude a divisão do Acelerômetro para a escala correta (g's)
    m_accelLabels[playerIndex]->setText(QString("Accel Celular: (%1, %2, %3)")
        .arg(packet.accelX / 4096.0, 0, 'f', 2) // ANTES: 100.0
        .arg(packet.accelY / 4096.0, 0, 'f', 2) // ANTES: 100.0
        .arg(packet.accelZ / 4096.0, 0, 'f', 2)); // ANTES: 100.0
    // --- FIM DA CORREÇÃO ---
}

// Gerencia conexão de novos jogadores
void MainWindow::onPlayerConnected(int playerIndex, const QString& type)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    m_playerConnectionTypes[playerIndex] = type;
    m_playerTabs->setTabText(playerIndex, QString("✅ Jogador %1 (%2)").arg(playerIndex + 1).arg(type));

    // Mostra abas extras se mais de 4 jogadores conectarem
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

// Gerencia desconexão de jogadores
void MainWindow::onPlayerDisconnected(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    // Reseta estado de conexão do jogador
    m_playerConnectionTypes[playerIndex] = "Nenhum";
    m_playerTabs->setTabText(playerIndex, QString("❌ Jogador %1").arg(playerIndex + 1));

    // Reseta interface do jogador
    m_sensorWidgetWrappers[playerIndex]->setVisible(true);
    m_gamepadDisplays[playerIndex]->resetState();

    // Reseta labels dos sensores
    m_gyroLabels[playerIndex]->setText("Gyro Celular: (0.00, 0.00, 0.00)");
    m_accelLabels[playerIndex]->setText("Accel Celular: (0.00, 0.00, 0.00)");

    // Gerencia visibilidade das abas extras
    if (playerIndex >= 4) {
        m_playerTabs->setTabVisible(playerIndex, false);

        // Verifica se ainda há jogadores extras conectados
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

// Processa clique no botão de desconectar jogador
void MainWindow::onDisconnectPlayerClicked(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    if (m_playerConnectionTypes[playerIndex] != "Nenhum") {
        const QString& type = m_playerConnectionTypes[playerIndex];
        m_connectionManager->forceDisconnectPlayer(playerIndex, type);
    }
}

// Atualiza os status de conexão na interface
void MainWindow::updateConnectionStatus()
{
    int networkCount = 0;
    int btCount = 0;

    // Conta jogadores por tipo de conexão
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_playerConnectionTypes[i] == "Wi-Fi" || m_playerConnectionTypes[i] == "Ancoragem USB") networkCount++;
        else if (m_playerConnectionTypes[i] == "Bluetooth" || m_playerConnectionTypes[i] == "Bluetooth LE") btCount++;
    }

    // Atualiza labels de status
    m_networkStatusLabel->setText(networkCount > 0 ? QString("Status: %1 jogador(es) conectado(s).").arg(networkCount) : "Status: Aguardando Conexões...");
    m_btStatusLabel->setText(btCount > 0 ? QString("Status: %1 jogador(es) conectado(s).").arg(btCount) : "Status: Aguardando Conexões...");
}

// Exibe mensagens de log na barra de status
void MainWindow::onLogMessage(const QString& message)
{
    statusBar()->showMessage(message, 5000);
}

// Gerencia evento de fechamento da aplicação
void MainWindow::closeEvent(QCloseEvent* event)
{
    qDebug() << "Fechando a aplicacao...";
    m_connectionManager->stopServices();
    event->accept();
}