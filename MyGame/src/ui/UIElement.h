#pragma once

// 모든 UI 요소(점수, 높이 게이지, 게임오버 패널)가 구현해야 하는 인터페이스
class UIElement
{
public:
    virtual ~UIElement() = default;

    virtual void Update(float deltaTime) = 0;
    virtual void Render() = 0;
};
