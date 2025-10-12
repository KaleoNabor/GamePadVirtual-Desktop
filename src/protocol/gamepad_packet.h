#ifndef GAMEPAD_PACKET_H
#define GAMEPAD_PACKET_H

#include <cstdint>

#pragma pack(push, 1)

// ATUALIZADO: O tamanho total é agora 20 bytes
struct GamepadPacket {
    // 2 bytes para os botões
    uint16_t buttons;

    // 4 bytes para os eixos dos analógicos (-128 a 127)
    int8_t leftStickX;
    int8_t leftStickY;
    int8_t rightStickX;
    int8_t rightStickY;

    // 2 bytes para os gatilhos (0 a 255)
    uint8_t leftTrigger;
    uint8_t rightTrigger;

    // 6 bytes para o giroscópio
    int16_t gyroX;
    int16_t gyroY;
    int16_t gyroZ;

    // ADICIONADO: 6 bytes para o acelerómetro
    int16_t accelX;
    int16_t accelY;
    int16_t accelZ;
};

#pragma pack(pop)

// O enum GamepadButton permanece o mesmo
enum GamepadButton {
    DPAD_UP = 1 << 0,
    DPAD_DOWN = 1 << 1,
    DPAD_LEFT = 1 << 2,
    DPAD_RIGHT = 1 << 3,
    START = 1 << 4,
    SELECT = 1 << 5,
    L3 = 1 << 6,
    R3 = 1 << 7,
    L1 = 1 << 8,
    R1 = 1 << 9,
    A = 1 << 12,
    B = 1 << 13,
    X = 1 << 14,
    Y = 1 << 15
};

#endif // GAMEPAD_PACKET_H