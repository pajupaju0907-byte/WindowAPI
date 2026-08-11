#pragma once

#include "../core/Scene.h"
#include "../util/Types.h"

// 타이틀 화면 씬
class TitleScene : public Scene
{
public:
    void Enter() override;
    void Exit() override;
    void Update(float deltaTime) override;
    void Render(ID2D1RenderTarget* renderTarget) override;

private:
    // Button.png(Start/Option/Ranking 3칸 스프라이트시트) 중 원하는 칸의 원본 픽셀 좌표 영역.
    // slotIndex: 0=Start, 1=Option, 2=Ranking
    D2D1_RECT_F GetButtonSlotSourceRect(int slotIndex) const;

    // 버튼 중심 좌표와 (원본 비율을 유지한) 그릴 크기. Update(클릭 판정)와 Render(그리기)가
    // 서로 다른 크기를 계산해서 어긋나는 일이 없도록 한 곳에서 계산해 공유한다.
    // slotIndex가 클수록 한 칸 아래(TITLE_BUTTON_CENTER_Y 기준)에 이어 붙는다.
    Vector2 GetButtonSlotCenter(int slotIndex) const;
    Vector2 GetButtonSize() const;
    // 클릭 판정 중심. Start 칸 중심에서 TITLE_BUTTON_HITBOX_OFFSET_Y만큼 세로로 옮긴 위치
    Vector2 GetButtonHitboxCenter() const;
    // 클릭 판정용 크기. GetButtonSize()에 가로/세로 각각 다른 배율(HITBOX_SCALE_X/Y)을 곱해서,
    // 배경에 같이 그려진 다른 버튼과 안 겹치면서도 실제 버튼 모양에 맞출 수 있게 한다
    Vector2 GetButtonHitboxSize() const;

    Vector2 GetExitButtonCenter() const;
    Vector2 GetExitButtonSize() const;

    static bool IsPointInRect(Vector2 point, Vector2 rectCenter, Vector2 rectSize);

    // F1로 토글하는, 버튼 클릭 판정 영역(히트박스) 표시 여부
    bool m_showDebug = false;

    // [연출] 제목 낙하+통통 튕기는 연출용 상태. Enter()에서 화면 위(TITLE_DROP_START_OFFSET_Y)로
    // 초기화되고, Update()에서 중력처럼 가속하며 떨어지다가 제자리(오프셋 0)에 닿으면 튕겨 올랐다
    // 잦아들며 멈춘다. Render()가 TITLE_IMAGE_CENTER_Y에 이 오프셋을 더해서 그린다.
    float m_titleDropOffsetY = 0.0f;
    float m_titleDropVelocityY = 0.0f;
    bool m_titleDropSettled = false;
};
