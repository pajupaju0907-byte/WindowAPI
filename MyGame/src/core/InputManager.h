#pragma once

// 키보드 입력 상태를 관리하는 싱글톤
class InputManager
{
public:
    static InputManager& GetInstance();

    // 해당 키가 눌려있는 상태인지
    bool IsKeyDown(int key) const;

    // 해당 키가 이번 프레임에 새로 눌렸는지
    bool IsKeyPressed(int key) const;

private:
    InputManager() = default;
    ~InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
};
