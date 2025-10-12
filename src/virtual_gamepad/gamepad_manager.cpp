#include "gamepad_manager.h"
#include <QDebug>
#include <cmath>
#include <algorithm>

// CORREÇÃO: Função callback com assinatura correta
VOID CALLBACK x360_notification_callback(
    PVIGEM_CLIENT Client,
    PVIGEM_TARGET Target,
    UCHAR LargeMotor,
    UCHAR SmallMotor,
    UCHAR LedNumber,
    LPVOID UserData
)
{
    GamepadManager* manager = static_cast<GamepadManager*>(UserData);
    if (manager) {
        manager->onVibrationReceived(Target, LargeMotor, SmallMotor);
    }
}

GamepadManager::GamepadManager(QObject* parent)
    : QObject(parent), vigem_client(nullptr)
{
    // Inicializar arrays
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        virtual_gamepads[i] = nullptr;
        ZeroMemory(&last_reports[i], sizeof(XUSB_REPORT));
    }
}

GamepadManager::~GamepadManager()
{
    qDebug() << "Liberando recursos do GamepadManager...";
    if (vigem_client) {
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            if (virtual_gamepads[i]) {
                vigem_target_remove(vigem_client, virtual_gamepads[i]);
                vigem_target_free(virtual_gamepads[i]);
                virtual_gamepads[i] = nullptr;
            }
        }
        vigem_disconnect(vigem_client);
        vigem_free(vigem_client);
        vigem_client = nullptr;
    }
}

bool GamepadManager::initialize()
{
    vigem_client = vigem_alloc();
    if (vigem_client == nullptr) {
        qDebug() << "Erro: Nao foi possivel alocar o cliente ViGEm.";
        return false;
    }

    const VIGEM_ERROR err = vigem_connect(vigem_client);
    if (!VIGEM_SUCCESS(err)) {
        qDebug() << "Erro: Nao foi possivel conectar ao driver ViGEm Bus. Verifique se ele esta instalado.";
        vigem_free(vigem_client);
        vigem_client = nullptr;
        return false;
    }

    qDebug() << "Driver ViGEm Bus conectado com sucesso.";

    // Aloca e adiciona 4 controles virtuais do tipo Xbox 360
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        virtual_gamepads[i] = vigem_target_x360_alloc();

        // Registra a função de callback para receber eventos de vibração
        vigem_target_x360_register_notification(vigem_client, virtual_gamepads[i], x360_notification_callback, this);

        VIGEM_ERROR pad_err = vigem_target_add(vigem_client, virtual_gamepads[i]);

        if (!VIGEM_SUCCESS(pad_err)) {
            qDebug() << "Erro ao adicionar o controle virtual" << i + 1;
            // Limpa recursos em caso de falha
            for (int j = 0; j <= i; ++j) {
                if (virtual_gamepads[j]) {
                    vigem_target_remove(vigem_client, virtual_gamepads[j]);
                    vigem_target_free(virtual_gamepads[j]);
                    virtual_gamepads[j] = nullptr;
                }
            }
            vigem_disconnect(vigem_client);
            vigem_free(vigem_client);
            vigem_client = nullptr;
            return false;
        }
    }

    qDebug() << "Gamepad Manager inicializado. 4 controles virtuais criados e prontos.";
    return true;
}

void GamepadManager::onPacketReceived(int playerIndex, const GamepadPacket& packet)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS || virtual_gamepads[playerIndex] == nullptr) {
        return;
    }

    XUSB_REPORT report;
    ZeroMemory(&report, sizeof(XUSB_REPORT));

    // Mapeia botões e gatilhos
    report.wButtons = packet.buttons;
    report.bLeftTrigger = packet.leftTrigger;
    report.bRightTrigger = packet.rightTrigger;

    // Mapeia analógico esquerdo
    report.sThumbLX = static_cast<SHORT>(packet.leftStickX * 257);
    report.sThumbLY = static_cast<SHORT>(packet.leftStickY * -257); // Invertido

    // Usa giroscópio para o analógico direito se houver movimento
    if (abs(packet.gyroX) > 50 || abs(packet.gyroY) > 50) {
        const double sensitivity = 327.67 * 2.0;

        short thumbRX = static_cast<short>((packet.gyroY / 100.0) * sensitivity);
        short thumbRY = static_cast<short>((-packet.gyroX / 100.0) * sensitivity);

        // FUNÇÃO CLAMP PRÓPRIA - elimina problemas com std::max/std::min
        auto clamp = [](short value, short min_val, short max_val) -> short {
            if (value < min_val) return min_val;
            if (value > max_val) return max_val;
            return value;
            };

        report.sThumbRX = clamp(thumbRX, -32767, 32767);
        report.sThumbRY = clamp(thumbRY, -32767, 32767);
    }
    else {
        // Se não, usa o analógico direito normal
        report.sThumbRX = static_cast<SHORT>(packet.rightStickX * 257);
        report.sThumbRY = static_cast<SHORT>(packet.rightStickY * -257); // Invertido
    }

    // Envia o estado para o driver
    vigem_target_x360_update(vigem_client, virtual_gamepads[playerIndex], report);

    // Emite o sinal para atualizar a UI
    emit gamepadStateUpdated(playerIndex, packet);
}

void GamepadManager::onVibrationReceived(PVIGEM_TARGET target, unsigned char largeMotor, unsigned char smallMotor)
{
    int playerIndex = -1;
    // Encontra qual jogador recebeu o evento de vibração
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (virtual_gamepads[i] == target) {
            playerIndex = i;
            break;
        }
    }

    if (playerIndex != -1) {
        if (largeMotor > 0 || smallMotor > 0) {
            // Calcula uma duração simples baseada na intensidade
            int duration = std::max(static_cast<int>(largeMotor), static_cast<int>(smallMotor));
            duration = std::min(duration * 2, 500); // Mapeia [0-255] para [0-510], com um teto de 500ms.

            // Monta o comando JSON que o app Flutter espera
            QByteArray command = QByteArray("{\"type\":\"vibration\",\"pattern\":[") + QByteArray::number(duration) + QByteArray("]}");

            qDebug() << "Enviando comando de vibracao para o jogador" << playerIndex + 1 << "com duracao" << duration << "ms";

            // Emite o sinal para que o ConnectionManager possa enviá-lo
            emit vibrationCommandReady(playerIndex, command);
        }
    }
}