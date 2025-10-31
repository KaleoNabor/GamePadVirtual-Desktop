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

// --- CONSTRUTOR ---
// Inicializa a janela principal e todos os componentes
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

// --- DESTRUTOR ---
MainWindow::~MainWindow() {}

// --- CONFIGURAR INTERFACE ---
// Cria toda a interface gráfica da aplicação
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

// --- CRIAR ABA DE CONEXÕES ---
// Cria a aba com informações de status das conexões
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

// --- CRIAR ABA DE TESTE ---
// Cria a aba com controles visuais para cada jogador
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

        pageLayout->addLayout(buttonLayout);

        // Conecta os sinais dos botões
        connect(disconnectButton, &QPushButton::clicked, this, [this, i]() {
            onDisconnectPlayerClicked(i);
            });

        connect(vibrateButton, &QPushButton::clicked, m_gamepadManager, [this, i]() {
            m_gamepadManager->testVibration(i);
            });

        // Display visual do gamepad
        m_gamepadDisplays[i] = new GamepadDisplayWidget();
        pageLayout->addWidget(m_gamepadDisplays[i], 1);

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

        // Ocultação das abas 5-8 por padrão
        if (i >= 4) {
            m_playerTabs->setTabVisible(i, false);
        }
    }

    return mainTabContainer;
}

// --- ATUALIZAR ESTADO DO GAMEPAD ---
// Atualiza a interface com os dados recebidos do gamepad
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

// --- JOGADOR CONECTADO ---
// Trata a conexão de um novo jogador
void MainWindow::onPlayerConnected(int playerIndex, const QString& type)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    // Atualização da interface para jogador conectado
    m_playerConnectionTypes[playerIndex] = type;
    m_playerTabs->setTabText(playerIndex, QString("✅ Jogador %1 (%2)").arg(playerIndex + 1).arg(type));

    // Exibição dinâmica das abas 5-8 quando conectadas
    if (playerIndex >= 4) {
        m_playerTabs->setTabVisible(playerIndex, true);

        // Exibição do aviso de limite estendido apenas uma vez
        if (!m_warningShown) {
            QMessageBox::information(this, "Limite Estendido",
                "Mais de 4 jogadores conectados. O limite do servidor foi estendido para 8 jogadores.");
            m_warningShown = true;
        }
    }

    updateConnectionStatus();
}

// --- JOGADOR DESCONECTADO ---
// Trata a desconexão de um jogador
void MainWindow::onPlayerDisconnected(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    // Reset do estado do jogador desconectado
    m_playerConnectionTypes[playerIndex] = "Nenhum";
    m_playerTabs->setTabText(playerIndex, QString("❌ Jogador %1").arg(playerIndex + 1));
    m_gamepadDisplays[playerIndex]->resetState();

    m_gyroLabels[playerIndex]->setText("Gyro: (0.00, 0.00, 0.00)");
    m_accelLabels[playerIndex]->setText("Accel: (0.00, 0.00, 0.00)");

    // Ocultação dinâmica das abas 5-8 quando desconectadas
    if (playerIndex >= 4) {
        m_playerTabs->setTabVisible(playerIndex, false);

        // Verificação se todos os jogadores extras estão desconectados
        bool extraPlayersActive = false;
        for (int i = 4; i < MAX_PLAYERS; ++i) {
            if (m_playerConnectionTypes[i] != "Nenhum") {
                extraPlayersActive = true;
                break;
            }
        }

        // Reset da flag de aviso quando nenhum jogador extra estiver ativo
        if (!extraPlayersActive) {
            m_warningShown = false;
        }
    }

    updateConnectionStatus();
}

// --- BOTÃO DESCONECTAR JOGADOR ---
// Trata o clique no botão de desconexão manual
void MainWindow::onDisconnectPlayerClicked(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    if (m_playerConnectionTypes[playerIndex] != "Nenhum") {
        qDebug() << "Desconexão manual solicitada para o Jogador" << playerIndex + 1;

        // --- SUBSTITUIÇÃO DO CÓDIGO ANTIGO (REQ 2 FIX) ---

        // 1. Pega o tipo de conexão que este jogador usou
        const QString& type = m_playerConnectionTypes[playerIndex];

        // 2. Diz ao ConnectionManager para forçar a desconexão do servidor correto
        // (Isso irá fechar o socket/limpar o slot UDP e disparar a
        // cadeia de sinais 'playerDisconnected' de qualquer maneira)
        m_connectionManager->forceDisconnectPlayer(playerIndex, type);

        // (A linha antiga 'm_gamepadManager->playerDisconnected(playerIndex);'
        // não é mais necessária, pois a cadeia de sinais fará isso por nós)
    }
}

// --- ATUALIZAR STATUS DE CONEXÃO ---
// Atualiza os labels de status com a contagem de jogadores conectados
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

// --- MENSAGEM DE LOG ---
// Exibe mensagens de log na barra de status
void MainWindow::onLogMessage(const QString& message)
{
    statusBar()->showMessage(message, 5000);
}

// --- EVENTO DE FECHAR ---
// Garante a parada adequada dos serviços ao fechar a aplicação
void MainWindow::closeEvent(QCloseEvent* event)
{
    qDebug() << "Fechando a aplicacao...";
    m_connectionManager->stopServices();
    event->accept();
}