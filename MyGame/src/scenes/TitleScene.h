#pragma once

#include "../core/Scene.h"
#include "../util/Types.h"
#include <string>
#include <vector>
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
    // Button.png(6버튼 스프라이트시트) 중 원하는 칸의 원본 픽셀 좌표 영역.
    // slotIndex: 0=Start, 1=Option, 2=Ranking, 3=Exit
    D2D1_RECT_F GetButtonSlotSourceRect(int slotIndex) const;

    // 버튼 중심 좌표와 (원본 비율을 유지한) 그릴 크기. Update(클릭 판정)와 Render(그리기)가
    // 서로 다른 크기를 계산해서 어긋나는 일이 없도록 한 곳에서 계산해 공유한다.
    // slotIndex가 클수록 한 칸 아래(TITLE_BUTTON_CENTER_Y 기준)에 이어 붙는다 — Exit도 이제 이
    // 스택의 마지막 칸(BUTTON_SLOT_EXIT)일 뿐, 따로 떨어진 위치를 갖지 않는다.
    Vector2 GetButtonSlotCenter(int slotIndex) const;
    Vector2 GetButtonSize() const;
    // 클릭 판정 중심/크기. BUTTON_SHEET_SOURCE_RECTS 자체가 알파 채널로 측정한 버튼의 실제 모양
    // 영역이라, 그리는 중심/크기(GetButtonSlotCenter/GetButtonSize)를 그대로 판정에 써도 버튼 모양과
    // 일치한다 — 별도의 축소 배율이나 위치 보정이 필요 없다.
    Vector2 GetButtonHitboxCenter(int slotIndex) const;
    Vector2 GetButtonHitboxSize() const;

    // [옵션 패널] Option 버튼을 누르면 화면 중앙에 뜨는 볼륨 조절 오버레이 배경의 중심/크기
    Vector2 GetOptionPanelCenter() const;
    Vector2 GetOptionPanelSize() const;

    // [옵션 패널] Pop.png에 그려진 X(닫기) 버튼의 화면 중심/크기
    Vector2 GetPopupCloseButtonCenter() const;
    Vector2 GetPopupCloseButtonSize() const;

    Vector2 GetRankingPanelCenter() const;
    Vector2 GetRankingPanelSize() const;
    Vector2 GetRankingCloseButtonCenter() const;
    Vector2 GetRankingCloseButtonSize() const;

    // Ranking.png 원본 픽셀 좌표(cellSourceRect)를, 지금 화면에 그려진 랭킹 패널 위의 실제 화면
    // 좌표로 변환한다. GetRankingCloseButtonCenter/Size와 같은 계산이라 그 두 칸(NAME/SCORE) 뿐
    // 아니라 임의의 칸에 재사용할 수 있게 일반화한 버전.
    Vector2 GetRankingCellCenter(const D2D1_RECT_F& cellSourceRect) const;
    Vector2 GetRankingCellSize(const D2D1_RECT_F& cellSourceRect) const;

    // [옵션 패널] 스피커+볼륨 바 그룹의 원점(그룹의 왼쪽 끝, 바들의 공통 바닥선) 화면 좌표.
    // soundbar.png 원본 픽셀 좌표를 이 원점 기준 상대 위치로 옮겨서 그리기 때문에, 그룹 안 배치
    // 비율은 그대로 유지된다.
    Vector2 GetSoundBarGroupOrigin() const;

    // [옵션 패널] soundbar.png 안 한 요소(스피커 또는 바 하나)의 원본 잘림 영역(sourceRect)을 받아,
    // 그룹 원점 기준으로 SOUND_BAR_GROUP_SCALE만큼 줄인 화면 중심/크기를 계산한다.
    // 개별 폭/높이/간격을 따로 정하지 않고 원본 스프라이트에 그려진 비율을 그대로 쓰기 위한 공용 계산.
    Vector2 GetSoundBarElementCenter(const D2D1_RECT_F& sourceRect) const;
    static Vector2 GetSoundBarElementSize(const D2D1_RECT_F& sourceRect);

    // [옵션 패널] 패널 왼쪽에 그리는 스피커 아이콘 중심
    Vector2 GetSpeakerIconCenter() const;

    // [옵션 패널] barIndex(0~VOLUME_BAR_COUNT-1)번째 볼륨 바의 중심/크기.
    Vector2 GetVolumeBarCenter(int barIndex) const;
    Vector2 GetVolumeBarSize(int barIndex) const;

    // 현재 SoundManager 볼륨을 몇 번째 칸까지 채워야 하는지로 환산 (0~VOLUME_BAR_COUNT)
    int GetFilledVolumeBarCount() const;

    static bool IsPointInRect(Vector2 point, Vector2 rectCenter, Vector2 rectSize);

    // F1로 토글하는, 버튼 클릭 판정 영역(히트박스) 표시 여부
    bool m_showDebug = false;

    // Option 버튼을 눌러서 연 볼륨 조절 오버레이 패널이 떠 있는지
    bool m_showOptionPanel = false;
    bool m_showRankingPanel = false;

    bool m_showNicknamePanel = false;
    std::wstring m_nicknameInput;
    // [연출] 제목 낙하+통통 튕기는 연출용 상태. Enter()에서 화면 위(TITLE_DROP_START_OFFSET_Y)로
    // 초기화되고, Update()에서 중력처럼 가속하며 떨어지다가 제자리(오프셋 0)에 닿으면 튕겨 올랐다
    // 잦아들며 멈춘다. Render()가 TITLE_IMAGE_CENTER_Y에 이 오프셋을 더해서 그린다.
    float m_titleDropOffsetY = 0.0f;
    float m_titleDropVelocityY = 0.0f;
    bool m_titleDropSettled = false;
};
