#pragma once

#include "../core/Scene.h"

// 타이틀 화면 씬
class TitleScene : public Scene
{
public:
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
};
