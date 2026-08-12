#pragma once

#include "../core/Scene.h"
#include "../util/Types.h"

// 게임오버 결과를 보여주는 씬
class GameOverScene : public Scene
{
public:
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render(ID2D1RenderTarget* renderTarget) override;

private:
    D2D1_RECT_F GetButtonSlotSourceRect(int slotIndex) const;

   
    Vector2 GetButtonSlotCenter(int slotIndex) const;
    Vector2 GetButtonSize() const;
    
    Vector2 GetButtonHitboxCenter(int slotIndex) const;
   

    Vector2 GetButtonHitboxSize(int slotIndex) const;

    static bool IsPointInRect(Vector2 point, Vector2 rectCenter, Vector2 rectSize);

    bool m_showDebug = false;

    float m_gameOverDropOffsetY = 0.0f;
    float m_gameOverDropVelocityY = 0.0f;
    bool m_gameOverDropSettled  = false;
};
