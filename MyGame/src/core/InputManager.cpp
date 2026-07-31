#include "pch.h"

#include "InputManager.h"

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
