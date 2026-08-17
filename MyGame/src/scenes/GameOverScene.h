#pragma once

#include <vector>

#include "../core/Scene.h"
#include "../util/Types.h"

// 신기록 팝업이 터질 때 흩날리는 파티클 조각 하나의 상태
struct NewRecordParticle
{
    Vector2 position;
    Vector2 velocity;
    float rotation = 0.0f;         // 도(degree) 단위
    float angularVelocity = 0.0f;  // 도/초
    int sourceRectIndex = 0;       // Particles.png 안 어떤 조각을 쓸지
};

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

    // [신기록 팝업] 개인 최고기록을 갱신했을 때만 호출 — 파티클을 사방으로 흩뿌리고 텍스트 등장
    // 애니메이션 상태를 초기화한다
    void SpawnNewRecordPopup();
    void UpdateNewRecordPopup(float deltaTime);
    void RenderNewRecordPopup(ID2D1RenderTarget* renderTarget) const;
    // 표시 시간이 끝나기 직전(NEW_RECORD_FADE_DURATION) 동안 1.0에서 0.0으로 줄어드는 불투명도.
    // 텍스트와 파티클이 같이 서서히 사라지도록 둘 다 이 값을 공유해서 쓴다.
    float GetNewRecordPopupOpacity() const;

    bool m_showDebug = false;

    float m_gameOverDropOffsetY = 0.0f;
    float m_gameOverDropVelocityY = 0.0f;
    bool m_gameOverDropSettled  = false;

    bool m_isNewRecordActive = false;
    float m_newRecordElapsed = 0.0f;
    float m_newRecordScale = 0.0f;
    float m_newRecordScaleVelocity = 0.0f;
    std::vector<NewRecordParticle> m_newRecordParticles;
};
