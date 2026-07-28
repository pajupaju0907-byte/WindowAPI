#pragma once

#include "UIElement.h"

// 게임오버 시 표시되는 패널
class GameOverPanel : public UIElement
{
public:
    GameOverPanel();

    void Update(float deltaTime) override;
    void Render() override;
};
