#pragma once

#include "UIElement.h"

// 탑이 쌓인 높이를 게이지로 보여주는 UI
class HeightGauge : public UIElement
{
public:
    HeightGauge();

    void Update(float deltaTime) override;
    void Render() override;

private:
    float m_currentHeight = 0.0f;
};
