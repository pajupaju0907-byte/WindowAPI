#pragma once

#include "UIElement.h"

// 현재 점수를 표시하는 UI
class ScoreText : public UIElement
{
public:
    ScoreText();

    void Update(float deltaTime) override;
    void Render() override;

private:
    int m_score = 0;
};
