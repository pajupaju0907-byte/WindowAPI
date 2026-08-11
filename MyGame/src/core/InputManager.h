#pragma once

#include "../util/Types.h"

// 키보드/마우스 입력 상태를 관리하는 싱글톤.
// 마우스 왼쪽/오른쪽 버튼도 가상 키 코드(VK_LBUTTON 등)가 0~255 범위 안이라 키보드와 같은
// IsKeyDown/IsKeyPressed로 그대로 조회할 수 있다 — 버튼 전용 메서드를 따로 만들 필요가 없다.
class InputManager
{
public:
    static InputManager& GetInstance();

    // 매 프레임 1회 호출: 지금 키/마우스 상태를 이전 상태로 넘기고 새로 다시 읽어들인다
    void Update();

    // 해당 키가 눌려있는 상태인지
    bool IsKeyDown(int key) const;

    // 해당 키가 이번 프레임에 새로 눌렸는지 (직전 프레임엔 안 눌려있다가 지금 눌린 순간만 true)
    bool IsKeyPressed(int key) const;

    // 현재 마우스 커서 위치(창 클라이언트 영역 기준 좌표, 화면 좌표계와 동일)
    Vector2 GetMousePosition() const;

private:
    InputManager() = default;
    ~InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // 가상 키 코드(0~255) 범위를 그대로 인덱스로 사용
    static constexpr int KEY_COUNT = 256;
    bool m_currentKeys[KEY_COUNT] = {};
    bool m_previousKeys[KEY_COUNT] = {};

    Vector2 m_mousePosition;
};
