#ifndef CONTROLLER_TYPES_H
#define CONTROLLER_TYPES_H

// CORREÇÃO: Use constexpr em vez de static constexpr para evitar problemas de linkage
constexpr int MAX_PLAYERS = 8;
constexpr int DSU_MAX_CONTROLLERS = 4;

// Enum centralizado para evitar redefinição
enum class ControllerType {
    Xbox360 = 0,
    DualShock4 = 1
};

#endif // CONTROLLER_TYPES_H