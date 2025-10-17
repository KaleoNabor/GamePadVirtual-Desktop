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

// --- Construtor principal da janela ---
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    
    // Inicializa os tipos de conexão dos jogadores
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerConnectionTypes[i] = "Nenhum";
    }

    // Cria o gerenciador de gamepads virtuais
    m_gamepadManager = new GamepadManager(this);
    // Cria o gerenciador de conexões passando o gamepad manager
    m_connectionManager = new ConnectionManager(m_gamepadManager, this);

    // Conecta os sinais do gamepad manager para atualizar a interface
    connect(m_gamepadManager, &GamepadManager::gamepadStateUpdated, this, &MainWindow::onGamepadStateUpdate);
    connect(m_gamepadManager, &GamepadManager::playerConnectedSignal, this, &MainWindow::onPlayerConnected);
    connect(m_gamepadManager, &GamepadManager::playerDisconnectedSignal, this, &MainWindow::onPlayerDisconnected);
    connect(m_connectionManager, &ConnectionManager::logMessage, this, &MainWindow::onLogMessage);

    // Configura a interface e inicia os serviços
    setupUI();

    // Inicializa o gamepad manager antes de iniciar serviços de rede
    if (m_gamepadManager->initialize()) {
        m_connectionManager->startServices();
    }
}

// --- Destrutor ---
MainWindow::~MainWindow() {}

// --- Configuração da interface gráfica principal ---
void MainWindow::setupUI()
{
    setWindowTitle("Servidor GamePadVirtual");
    setMinimumSize(800, 600);

    // Cria widget de abas para organizar as diferentes seções
    QTabWidget* tabWidget = new QTabWidget();
    QWidget* connectionsTab = createConnectionsTab();
    QWidget* testTab = createTestTab();

    // Adiciona as abas ao widget principal
    tabWidget->addTab(connectionsTab, "Conexões");
    tabWidget->addTab(testTab, "Teste de Controles");

    setCentralWidget(tabWidget);
}

// --- Cria a aba de informações de conexão ---
QWidget* MainWindow::createConnectionsTab()
{
    QWidget* tab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(tab);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // --- Grupo de configuração de rede ---
    QGroupBox* networkGroup = new QGroupBox("Conexão de Rede (Wi-Fi / Ancoragem USB)");
    QVBoxLayout* networkLayout = new QVBoxLayout(networkGroup);

    // Instruções para conexão Wi-Fi
    QLabel* wifiInstruction = new QLabel("<b>Para Wi-Fi:</b> Garanta que o PC e o celular estejam na mesma rede e use a Descoberta Automática no app.");
    wifiInstruction->setWordWrap(true);

    // Instruções para conexão USB
    QLabel* usbInstruction = new QLabel("<b>Para USB:</b> Conecte o cabo, ative a 'Ancoragem USB' (USB Tethering) no celular e use a Descoberta Automática no app.");
    usbInstruction->setWordWrap(true);

    // Label que mostra o status das conexões de rede
    m_networkStatusLabel = new QLabel("Status: Aguardando Conexões...");

    // Adiciona widgets ao layout de rede
    networkLayout->addWidget(wifiInstruction);
    networkLayout->addWidget(usbInstruction);
    networkLayout->addWidget(m_networkStatusLabel);

    // --- Grupo de configuração Bluetooth ---
    QGroupBox* btGroup = new QGroupBox("Bluetooth");
    QVBoxLayout* btLayout = new QVBoxLayout(btGroup);

    // Label que mostra o status das conexões Bluetooth
    m_btStatusLabel = new QLabel("Status: Aguardando Conexões...");
    btLayout->addWidget(m_btStatusLabel);

    // Adiciona os grupos ao layout principal
    mainLayout->addWidget(networkGroup);
    mainLayout->addWidget(btGroup);
    mainLayout->addStretch();

    return tab;
}

// --- Cria a aba de teste dos controles ---
QWidget* MainWindow::createTestTab()
{
    QWidget* mainTabContainer = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(mainTabContainer);

    // Cria abas para cada jogador
    m_playerTabs = new QTabWidget();
    mainLayout->addWidget(m_playerTabs);

    // Cria uma página de teste para cada jogador
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        QWidget* playerPage = new QWidget();
        QVBoxLayout* pageLayout = new QVBoxLayout(playerPage);
        pageLayout->setAlignment(Qt::AlignCenter);

        // Widget que mostra visualmente o estado do gamepad
        m_gamepadDisplays[i] = new GamepadDisplayWidget();
        pageLayout->addWidget(m_gamepadDisplays[i], 1);

        // Container para os sensores (giroscópio e acelerômetro)
        QWidget* sensorContainer = new QWidget();
        QHBoxLayout* sensorLayout = new QHBoxLayout(sensorContainer);

        // Labels para mostrar dados dos sensores
        m_gyroLabels[i] = new QLabel("Gyro: (0.00, 0.00, 0.00)");
        m_accelLabels[i] = new QLabel("Accel: (0.00, 0.00, 0.00)");

        // Organiza os labels dos sensores no layout
        sensorLayout->addStretch();
        sensorLayout->addWidget(m_gyroLabels[i]);
        sensorLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
        sensorLayout->addWidget(m_accelLabels[i]);
        sensorLayout->addStretch();

        pageLayout->addWidget(sensorContainer);

        // Adiciona a aba do jogador (inicialmente desconectado)
        m_playerTabs->addTab(playerPage, QString("❌ Jogador %1").arg(i + 1));
    }

    return mainTabContainer;
}

// --- Atualiza a interface quando chegam dados do gamepad ---
void MainWindow::onGamepadStateUpdate(int playerIndex, const GamepadPacket& packet)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    // Atualiza a visualização do gamepad
    m_gamepadDisplays[playerIndex]->updateState(packet);

    // Atualiza os labels do giroscópio (convertendo valores)
    m_gyroLabels[playerIndex]->setText(QString("Gyro: (%1, %2, %3)")
        .arg(packet.gyroX / 100.0, 0, 'f', 2)
        .arg(packet.gyroY / 100.0, 0, 'f', 2)
        .arg(packet.gyroZ / 100.0, 0, 'f', 2));

    // Atualiza os labels do acelerômetro (convertendo valores)
    m_accelLabels[playerIndex]->setText(QString("Accel: (%1, %2, %3)")
        .arg(packet.accelX / 100.0, 0, 'f', 2)
        .arg(packet.accelY / 100.0, 0, 'f', 2)
        .arg(packet.accelZ / 100.0, 0, 'f', 2));
}

// --- Processa quando um jogador se conecta ---
void MainWindow::onPlayerConnected(int playerIndex, const QString& type)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    // Atualiza o tipo de conexão e a interface
    m_playerConnectionTypes[playerIndex] = type;
    m_playerTabs->setTabText(playerIndex, QString("✅ Jogador %1 (%2)").arg(playerIndex + 1).arg(type));
    updateConnectionStatus();
}

// --- Processa quando um jogador se desconecta ---
void MainWindow::onPlayerDisconnected(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    // Reseta o estado do jogador desconectado
    m_playerConnectionTypes[playerIndex] = "Nenhum";
    m_playerTabs->setTabText(playerIndex, QString("❌ Jogador %1").arg(playerIndex + 1));
    m_gamepadDisplays[playerIndex]->resetState();

    // Reseta os labels dos sensores
    m_gyroLabels[playerIndex]->setText("Gyro: (0.00, 0.00, 0.00)");
    m_accelLabels[playerIndex]->setText("Accel: (0.00, 0.00, 0.00)");

    updateConnectionStatus();
}

// --- Atualiza os status de conexão na interface ---
void MainWindow::updateConnectionStatus()
{
    int networkCount = 0;
    int btCount = 0;

    // Conta quantos jogadores estão conectados por cada tipo
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_playerConnectionTypes[i] == "Wi-Fi" || m_playerConnectionTypes[i] == "Ancoragem USB") networkCount++;
        else if (m_playerConnectionTypes[i] == "Bluetooth") btCount++;
    }

    // Atualiza os labels de status
    m_networkStatusLabel->setText(networkCount > 0 ? QString("Status: %1 jogador(es) conectado(s).").arg(networkCount) : "Status: Aguardando Conexões...");
    m_btStatusLabel->setText(btCount > 0 ? QString("Status: %1 jogador(es) conectado(s).").arg(btCount) : "Status: Aguardando Conexões...");
}

// --- Exibe mensagens de log na barra de status ---
void MainWindow::onLogMessage(const QString& message)
{
    statusBar()->showMessage(message, 5000);
}

// --- Processa o fechamento da aplicação ---
void MainWindow::closeEvent(QCloseEvent* event)
{
    qDebug() << "Fechando a aplicacao...";
    // Para os serviços de conexão antes de fechar
    m_connectionManager->stopServices();
    event->accept();
}