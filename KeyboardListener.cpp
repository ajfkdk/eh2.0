#include "KeyboardListener.h"
#include <iostream>

void KeyboardListener::ListenerLoop(KeyboardListener* instance) {
    while (instance->running.load()) {
        // 检查Q/W键
        if (IsKeyDown('Q') && instance->onKeyPress) instance->onKeyPress('Q');
        if (IsKeyDown('W') && instance->onKeyPress) instance->onKeyPress('W');

        // 检查A/S键
        if (IsKeyDown('A') && instance->onKeyPress) instance->onKeyPress('A');
        if (IsKeyDown('S') && instance->onKeyPress) instance->onKeyPress('S');

        // 检查Z/X键
        if (IsKeyDown('Z') && instance->onKeyPress) instance->onKeyPress('Z');
        if (IsKeyDown('X') && instance->onKeyPress) instance->onKeyPress('X');

        // 检查E/R
        if (IsKeyDown('E') && instance->onKeyPress) instance->onKeyPress('E');
        if (IsKeyDown('R') && instance->onKeyPress) instance->onKeyPress('R');

        // 检查F/G
        if (IsKeyDown('F') && instance->onKeyPress) instance->onKeyPress('F');
        if (IsKeyDown('G') && instance->onKeyPress) instance->onKeyPress('G');

        // 检查C/V
        if (IsKeyDown('C') && instance->onKeyPress) instance->onKeyPress('C');
        if (IsKeyDown('V') && instance->onKeyPress) instance->onKeyPress('V');
        // 降低CPU使用率
        Sleep(100);
    }
}

bool KeyboardListener::Start() {
    if (running.load()) return false;

    running.store(true);
    listenerThread = std::thread(ListenerLoop, this);
    return true;
}

void KeyboardListener::Stop() {
    running.store(false);
    if (listenerThread.joinable()) {
        listenerThread.join();
    }
}

bool KeyboardListener::IsKeyDown(int vkCode) {
    return (GetAsyncKeyState(vkCode) & 0x8000) != 0;
}

KeyboardListener::~KeyboardListener() {
    Stop();
}