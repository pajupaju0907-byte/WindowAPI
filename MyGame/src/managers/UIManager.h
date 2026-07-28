#pragma once

#include <memory>
#include <vector>

class UIElement;

// UI 요소들의 소유와 일괄 Update/Render를 담당하는 싱글톤
class UIManager
{
public:
    static UIManager& GetInstance();

    void AddElement(std::unique_ptr<UIElement> element);
    void Update(float deltaTime);
    void Render();

private:
    UIManager() = default;
    ~UIManager();
    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;

    std::vector<std::unique_ptr<UIElement>> m_elements;
};
