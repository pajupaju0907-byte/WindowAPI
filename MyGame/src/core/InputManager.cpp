#include "InputManager.h"

InputManager& InputManager::GetInstance()
{
    static InputManager instance;
    return instance;
}

bool InputManager::IsKeyDown(int key) const
{
    // TODO: GetAsyncKeyState 등을 이용한 실제 키 상태 판정 직접 구현
    (void)key;
    return false;
}

bool InputManager::IsKeyPressed(int key) const
{
    // TODO: 이전 프레임 키 상태와 비교하는 엣지 판정 직접 구현
    (void)key;
    return false;
}
