#pragma once

#include "../core/Scene.h"

// 실제 게임 플레이가 진행되는 씬.
// PhysicsManager/CollisionManager/BlockManager/GridManager/CameraManager/UIManager를 사용한다.
class PlayScene : public Scene
{
public:
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render() override;
};
