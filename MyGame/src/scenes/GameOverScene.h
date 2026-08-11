#pragma once

#include "../core/Scene.h"

// 게임오버 결과를 보여주는 씬
class GameOverScene : public Scene
{
public:
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render(ID2D1RenderTarget* renderTarget) override;
};
