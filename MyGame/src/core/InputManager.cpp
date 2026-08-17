#include "pch.h"

#include "InputManager.h"
#include "WindowManager.h"
#include <cwctype>

InputManager& InputManager::GetInstance()
{
    static InputManager instance;
    return instance;
}

void InputManager::Update()
{
    for (int key = 0; key < KEY_COUNT; ++key)
    {
        m_previousKeys[key] = m_currentKeys[key];
        m_currentKeys[key] = (GetAsyncKeyState(key) & 0x8000) != 0;
    }

    // 마우스 커서는 화면 좌표로 나오므로, 창 클라이언트 영역 기준 좌표로 바꿔서 저장해둔다
    // (렌더링에 쓰는 좌표계와 동일하게 맞춰야 버튼 히트 테스트에 그대로 쓸 수 있다)
    POINT cursorPos{};
    GetCursorPos(&cursorPos);
    ScreenToClient(WindowManager::GetInstance().GetWindowHandle(), &cursorPos);
    m_mousePosition = { static_cast<float>(cursorPos.x), static_cast<float>(cursorPos.y) };
}

Vector2 InputManager::GetMousePosition() const
{
    return m_mousePosition;
}

bool InputManager::IsKeyDown(int key) const
{
    if (key < 0 || key >= KEY_COUNT)
    {
        return false;
    }
    return m_currentKeys[key];
}

bool InputManager::IsKeyPressed(int key) const
{
    if (key < 0 || key >= KEY_COUNT)
    {
        return false;
    }
    return m_currentKeys[key] && !m_previousKeys[key];
}
void InputManager::OnChar(wchar_t ch)
{
    bool isAsciiAlnum = (ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z');
    if (isAsciiAlnum)
    {
        m_typedChars += ch;
    }
}

std::wstring InputManager::ConsumeTypedChars()
{
    std::wstring typed = std::move(m_typedChars);
    m_typedChars.clear();
    return typed;
}
