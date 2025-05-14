#ifndef KEYBOARD_LISTENER_H
#define KEYBOARD_LISTENER_H

#include <atomic>
#include <thread>
#include <functional>
#include <windows.h>

class KeyboardListener {
private:
    std::thread listenerThread;
    std::atomic<bool> running{ false };

    static void ListenerLoop(KeyboardListener* instance);

public:
    std::function<void(int)> onKeyPress = nullptr;

    bool Start();
    void Stop();

    // ���ָ�����Ƿ񱻰���
    static bool IsKeyDown(int vkCode);

    // ��������
    ~KeyboardListener();
};

#endif // KEYBOARD_LISTENER_H