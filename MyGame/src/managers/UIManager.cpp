#include "UIManager.h"
#include "../ui/UIElement.h"

UIManager& UIManager::GetInstance()
{
    static UIManager instance;
    return instance;
}

UIManager::~UIManager() = default;

void UIManager::AddElement(std::unique_ptr<UIElement> element)
{
    m_elements.push_back(std::move(element));
}

void UIManager::Update(float deltaTime)
{
    for (auto& element : m_elements)
    {
        element->Update(deltaTime);
    }
}

void UIManager::Render()
{
    for (auto& element : m_elements)
    {
        element->Render();
    }
}
