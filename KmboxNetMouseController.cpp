#include "KmboxNetMouseController.h"
#include <iostream>
#include "kmboxNet.h"
#include <thread>

KmboxNetMouseController::KmboxNetMouseController(const std::string& deviceIp,
    const std::string& devicePort,
    const std::string& deviceMac)
    : connected(false), monitoring(false), ip(deviceIp), port(devicePort), mac(deviceMac),
    lastMouseX(0), lastMouseY(0), mouseMoving(false) {

    // ��ʼ��kmboxNet�豸����
    int result = kmNet_init(const_cast<char*>(ip.c_str()),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(mac.c_str()));

    if (result == 0) {
        connected.store(true);
        std::cout << "KmboxNet���ӳɹ�" << std::endl;
    }
    else {
        std::cerr << "KmboxNet����ʧ�ܣ�������: " << result << std::endl;
    }

    // ��ʼ������ƶ���ز���
    lastMoveTime = std::chrono::steady_clock::now();
}

KmboxNetMouseController::~KmboxNetMouseController() {
    // ȷ���ͷż���
    if (monitoring.load()) {
        StopMonitor();
    }
    // kmboxNet�����û����ʽ�ر����ӵĺ���
}


void KmboxNetMouseController::MoveToWithTime(int x, int y, int during) {
    if (!connected.load()) {
        std::cerr << "KmboxNetδ���ӣ��޷��ƶ����" << std::endl;
        return;
    }

    int result = kmNet_mouse_move_auto(static_cast<short>(x), static_cast<short>(y), during);

    if (result != 0) {
        std::cerr << "KmboxNet�ƶ����ʧ�ܣ�������: " << result << std::endl;
    }
}

void KmboxNetMouseController::GetCurrentPosition(int& x, int& y) {
    // ����kmboxNetû��ֱ���ṩ��ȡ���λ�õĽӿڣ�����ʹ��Windows API��ȡ
    x = 0, y = 0;

    // ����Ѿ��ڼ�������ӻ����λ�û�ȡ
    if (monitoring.load()) {
        x = lastMouseX;
        y = lastMouseY;
    }
    else {
        // �����������ػ�ȡ��ǰλ��
        if (connected.load()) {
            kmNet_monitor_mouse_xy(&x, &y);
            lastMouseX = x;
            lastMouseY = y;
        }
    }
}

void KmboxNetMouseController::LeftClick() {
    if (!connected.load()) {
        std::cerr << "KmboxNetδ���ӣ��޷�������" << std::endl;
        return;
    }

    // ���º��ͷ����ģ����
    LeftDown();
    Sleep(10); // �����ӳ�ģ��������
    LeftUp();
}

void KmboxNetMouseController::LeftDown() {
    if (!connected.load()) {
        std::cerr << "KmboxNetδ���ӣ��޷�����������" << std::endl;
        return;
    }

    int result = kmNet_mouse_left(1); // 1��ʾ����
    if (result != 0) {
        std::cerr << "KmboxNet����������ʧ�ܣ�������: " << result << std::endl;
    }
}

void KmboxNetMouseController::LeftUp() {
    if (!connected.load()) {
        std::cerr << "KmboxNetδ���ӣ��޷��ͷ�������" << std::endl;
        return;
    }

    int result = kmNet_mouse_left(0); // 0��ʾ�ͷ�
    if (result != 0) {
        std::cerr << "KmboxNet�ͷ�������ʧ�ܣ�������: " << result << std::endl;
    }
}

void KmboxNetMouseController::RightClick() {
    if (!connected.load()) {
        std::cerr << "KmboxNetδ���ӣ��޷��������Ҽ�" << std::endl;
        return;
    }

    // ���º��ͷ��Ҽ�ģ����
    int result1 = kmNet_mouse_right(1); // ����
    Sleep(10); // �����ӳ�
    int result2 = kmNet_mouse_right(0); // �ͷ�

    if (result1 != 0 || result2 != 0) {
        std::cerr << "KmboxNet�������Ҽ�ʧ��" << std::endl;
    }
}

void KmboxNetMouseController::MoveRelative(int deltaX, int deltaY) {
    if (!connected.load()) {
        std::cerr << "KmboxNetδ���ӣ��޷��ƶ����" << std::endl;
        return;
    }

    // ֱ��ʹ������ƶ�����
    int result = kmNet_mouse_move(static_cast<short>(deltaX), static_cast<short>(deltaY));

    if (result != 0) {
        std::cerr << "KmboxNet����ƶ����ʧ�ܣ�������: " << result << std::endl;
    }
}

void KmboxNetMouseController::SideButton1Down() {
    if (!connected.load()) {
        std::cerr << "KmboxNetδ���ӣ��޷����������1" << std::endl;
        return;
    }

    int result = kmNet_mouse_side1(1); // 1��ʾ����
    if (result != 0) {
        std::cerr << "KmboxNet���������1ʧ�ܣ�������: " << result << std::endl;
    }
}

void KmboxNetMouseController::SideButton1Up() {
    if (!connected.load()) {
        std::cerr << "KmboxNetδ���ӣ��޷��ͷ������1" << std::endl;
        return;
    }

    int result = kmNet_mouse_side1(0); // 0��ʾ�ͷ�
    if (result != 0) {
        std::cerr << "KmboxNet�ͷ������1ʧ�ܣ�������: " << result << std::endl;
    }
}

void KmboxNetMouseController::SideButton2Down() {
    if (!connected.load()) {
        std::cerr << "KmboxNetδ���ӣ��޷����������2" << std::endl;
        return;
    }

    int result = kmNet_mouse_side2(1); // 1��ʾ����
    if (result != 0) {
        std::cerr << "KmboxNet���������2ʧ�ܣ�������: " << result << std::endl;
    }
}

void KmboxNetMouseController::SideButton2Up() {
    if (!connected.load()) {
        std::cerr << "KmboxNetδ���ӣ��޷��ͷ������2" << std::endl;
        return;
    }

    int result = kmNet_mouse_side2(0); // 0��ʾ�ͷ�
    if (result != 0) {
        std::cerr << "KmboxNet�ͷ������2ʧ�ܣ�������: " << result << std::endl;
    }
}

bool KmboxNetMouseController::IsConnected() const {
    return connected.load();
}

bool KmboxNetMouseController::Reconnect() {
    int result = kmNet_init(const_cast<char*>(ip.c_str()),
        const_cast<char*>(port.c_str()),
        const_cast<char*>(mac.c_str()));

    if (result == 0) {
        connected.store(true);
        std::cout << "KmboxNet�������ӳɹ�" << std::endl;
        return true;
    }
    else {
        std::cerr << "KmboxNet��������ʧ�ܣ�������: " << result << std::endl;
        return false;
    }
}

// ʵ�ּ�����ط���
bool KmboxNetMouseController::StartMonitor(int port) {
    if (!connected.load()) {
        std::cerr << "KmboxNetδ���ӣ��޷���������" << std::endl;
        return false;
    }

    int result = kmNet_monitor(port);
    if (result == 0) {
        monitoring.store(true);
        std::cout << "KmboxNet�������������˿�: " << port << std::endl;

        // �����������󣬿�ʼ�������ƶ�״̬
        std::thread([this]() {
            while (monitoring.load()) {
                UpdateMouseMovingState();
                std::this_thread::sleep_for(std::chrono::milliseconds(50)); // ÿ50ms���һ��
            }
            }).detach();

        return true;
    }
    else {
        std::cerr << "KmboxNet��������ʧ�ܣ�������: " << result << std::endl;
        return false;
    }
}

void KmboxNetMouseController::StopMonitor() {
    if (monitoring.load()) {
        // ���kmboxNet����ֹͣ�����ĺ�����Ӧ�����������
        // ����: kmNet_stop_monitor();

        // ����û����ȷ��ֹͣ����������������Ҫͨ��������ʽʵ��
        // ���߽���Ǽ���״̬��ֹͣ
        monitoring.store(false);
        std::cout << "KmboxNet������ֹͣ" << std::endl;
    }
}

bool KmboxNetMouseController::IsLeftButtonDown() {
    if (!connected.load() || !monitoring.load()) {
        std::cerr << "KmboxNetδ���ӻ�δ�����������޷����������״̬" << std::endl;
        return false;
    }

    return kmNet_monitor_mouse_left() == 1;
}

bool KmboxNetMouseController::IsMiddleButtonDown() {
    if (!connected.load() || !monitoring.load()) {
        std::cerr << "KmboxNetδ���ӻ�δ�����������޷��������м�״̬" << std::endl;
        return false;
    }

    return kmNet_monitor_mouse_middle() == 1;
}

bool KmboxNetMouseController::IsRightButtonDown() {
    if (!connected.load() || !monitoring.load()) {
        std::cerr << "KmboxNetδ���ӻ�δ�����������޷��������Ҽ�״̬" << std::endl;
        return false;
    }

    return kmNet_monitor_mouse_right() == 1;
}

bool KmboxNetMouseController::IsSideButton1Down() {
    if (!connected.load() || !monitoring.load()) {
        std::cerr << "KmboxNetδ���ӻ�δ�����������޷���������1״̬" << std::endl;
        return false;
    }

    return kmNet_monitor_mouse_side1() == 1;
}

bool KmboxNetMouseController::IsSideButton2Down() {
    if (!connected.load() || !monitoring.load()) {
        std::cerr << "KmboxNetδ���ӻ�δ�����������޷���������2״̬" << std::endl;
        return false;
    }

    return kmNet_monitor_mouse_side2() == 1;
}

bool KmboxNetMouseController::IsMouseMoving() {
    return mouseMoving.load();
}

void KmboxNetMouseController::UpdateMouseMovingState() {
    if (!connected.load() || !monitoring.load()) {
        return;
    }

    int currentX, currentY;
    int result = kmNet_monitor_mouse_xy(&currentX, &currentY);

    if (result == 1) { // ���λ�÷����仯
        lastMouseX = currentX;
        lastMouseY = currentY;
        mouseMoving.store(true);
        lastMoveTime = std::chrono::steady_clock::now(); // ��������ƶ�ʱ��
    }
    else {
        // ������ϴ��ƶ����Ƿ񾭹�����ֵʱ��
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastMoveTime).count();

        if (elapsed > MOVE_THRESHOLD_MS) {
            mouseMoving.store(false);
        }
    }
}