#ifndef CONTROLLER_TYPES_H
#define CONTROLLER_TYPES_H

// Enum centralizado para evitar redefinição
enum class ControllerType {
    Xbox360 = 0,
    DualShock4 = 1
};

// CORREÇÃO: Definição centralizada de constantes
static constexpr int MAX_PLAYERS = 8;
static constexpr int DSU_MAX_CONTROLLERS = 4;

#endif // CONTROLLER_TYPES_H