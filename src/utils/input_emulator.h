#ifndef INPUT_EMULATOR_H
#define INPUT_EMULATOR_H

#include <Windows.h>

class InputEmulator
{
public:
    static void moveMouse(int dx, int dy);
    static void mouseClick(bool left, bool down);
};

#endif