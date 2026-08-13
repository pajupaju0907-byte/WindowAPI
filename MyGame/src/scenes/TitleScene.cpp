#include "pch.h"

#include <algorithm>
#include <cmath>

#include "TitleScene.h"
#include "../managers/ResourceManager.h"
#include "../managers/RenderManager.h"
#include "../managers/SoundManager.h"
#include "../core/InputManager.h"
#include "../core/SceneManager.h"
#include "../core/WindowManager.h"
#include "../util/Constants.h"
#include "../util/Types.h"

namespace
{
    // Button.png(1536x1024)는 2열 x 3행으로 이어진 6버튼 스프라이트시트.
    // 순서(왼쪽 위부터 오른쪽으로, 그 다음 줄): 0=Start 1=Option 2=Ranking 3=Exit 4=Title 5=Retry
    // (Title/Retry는 GameOverScene.cpp에서 씀). 알파 채널을 스캔해 실제 그림 영역만 측정한 값이라
    // 원본 나눗셈이 아니라 이 배열에서 바로 찾아 쓴다.
    constexpr int BUTTON_SLOT_START = 0;
    constexpr int BUTTON_SLOT_OPTION = 1;
    constexpr int BUTTON_SLOT_RANKING = 2;
    constexpr int BUTTON_SLOT_EXIT = 3;
    constexpr D2D1_RECT_F BUTTON_SHEET_SOURCE_RECTS[6] = {
        { 42.0f, 114.0f, 766.0f, 362.0f },
        { 771.0f, 114.0f, 1492.0f, 362.0f },
        { 42.0f, 385.0f, 766.0f, 635.0f },
        { 771.0f, 385.0f, 1492.0f, 635.0f },
        { 42.0f, 656.0f, 766.0f, 905.0f },
        { 771.0f, 656.0f, 1492.0f, 905.0f },
    };
    // Ranking.png(1024x1536)는 캔버스 전체가 아니라 가운데의 순위표 팝업 모양만 그림이고 나머지는
// 투명 배경이다 — 알파 채널을 픽셀 단위로 스캔해 실제 그림이 있는 영역만 오려 쓴다.
    constexpr D2D1_RECT_F RANKING_PANEL_SOURCE_RECT = { 23.0f, 48.0f, 999.0f, 1450.0f };
    // Ranking.png 안에서 팝업 닫기(X) 버튼이 차지하는 실제 그림 영역
    constexpr D2D1_RECT_F RANKING_CLOSE_BUTTON_SOURCE_RECT = { 871.0f, 106.0f, 964.0f, 200.0f };

    // Pop.png(1536x1024)는 캔버스 전체가 아니라 가운데의 둥근 팝업창 모양만 그림이고 나머지는
    // 투명 배경이다 — 알파 채널을 픽셀 단위로 스캔해 실제 그림이 있는 영역만 오려 쓴다.
    constexpr D2D1_RECT_F POPUP_PANEL_SOURCE_RECT = { 88.0f, 100.0f, 1448.0f, 895.0f };
    // Pop.png 안에서 팝업 닫기(X) 버튼이 차지하는 실제 그림 영역
    constexpr D2D1_RECT_F POPUP_CLOSE_BUTTON_SOURCE_RECT = { 1268.0f, 144.0f, 1391.0f, 256.0f };
    // soundbar.png(1536x1024) 안에서 스피커 아이콘(몸통+음파)과 볼륨 바 5칸이 차지하는 실제 그림 영역.
    // 알파 채널을 픽셀 단위로 스캔해 측정한 값(글로우 번짐 경계까지 포함해 살짝 여유를 뒀다).
    constexpr D2D1_RECT_F SOUND_BAR_SPEAKER_SOURCE_RECT = { 70.0f, 436.0f, 438.0f, 768.0f };
    constexpr D2D1_RECT_F SOUND_BAR_SLOT_SOURCE_RECTS[Constants::VOLUME_BAR_COUNT] = {
        { 482.0f, 585.0f, 665.0f, 750.0f },
        { 683.0f, 508.0f, 865.0f, 750.0f },
        { 889.0f, 428.0f, 1068.0f, 750.0f },
        { 1087.0f, 354.0f, 1271.0f, 750.0f },
        { 1290.0f, 268.0f, 1468.0f, 750.0f },
    };

    // 스피커+바 그룹 안 모든 요소가 공유하는 원본(soundbar.png) 기준점.
    // 그룹 원점(GetSoundBarGroupOrigin)이 화면에서 이 좌표에 대응한다 — 스피커 왼쪽 끝(가로),
    // 바들의 공통 바닥선(세로, SOUND_BAR_SLOT_SOURCE_RECTS가 전부 bottom=750으로 공유).
    constexpr float SOUND_BAR_GROUP_ORIGIN_SOURCE_X = SOUND_BAR_SPEAKER_SOURCE_RECT.left;
    constexpr float SOUND_BAR_GROUP_BASELINE_SOURCE_Y = 750.0f;
}

void TitleScene::Enter()
{
    ResourceManager::GetInstance().LoadSprite("assets/TitleBackGround.png");
    ResourceManager::GetInstance().LoadSprite("assets/Title.png");
    ResourceManager::GetInstance().LoadSprite("assets/Button.png");
    ResourceManager::GetInstance().LoadSprite("assets/Pop.png");
    ResourceManager::GetInstance().LoadSprite("assets/soundbar.png");
    ResourceManager::GetInstance().LoadSprite("assets/Ranking.png");

    SoundManager::GetInstance().PlayBgm("assets/Sound/LifeIsFullOfHappiness.mp3");

    m_titleDropOffsetY = Constants::TITLE_DROP_START_OFFSET_Y;
    m_titleDropVelocityY = 0.0f;
    m_titleDropSettled = false;
}

void TitleScene::Exit()
{
}

D2D1_RECT_F TitleScene::GetButtonSlotSourceRect(int slotIndex) const
{
    return BUTTON_SHEET_SOURCE_RECTS[slotIndex];
}

Vector2 TitleScene::GetButtonSlotCenter(int slotIndex) const
{
    // slotIndex가 클수록 Start 자리 밑으로 버튼 한 칸 높이 + 여백(TITLE_BUTTON_GAP_Y)씩 이어 붙는다
    float stepY = GetButtonSize().y + Constants::TITLE_BUTTON_GAP_Y;
    return { Constants::WINDOW_WIDTH / 2.0f, Constants::TITLE_BUTTON_CENTER_Y + stepY * static_cast<float>(slotIndex) };
}

Vector2 TitleScene::GetButtonSize() const
{
    // 실제로 그리는 건 한 칸(원본의 1/3)뿐이니, 그 부분의 가로세로 비율을 기준으로 계산해야
    // 이미지 전체 비율로 계산했을 때처럼 세로로 길쭉하게 늘어나 보이지 않는다.
    D2D1_RECT_F sourceRect = GetButtonSlotSourceRect(BUTTON_SLOT_START);
    float croppedWidth = sourceRect.right - sourceRect.left;
    float croppedHeight = sourceRect.bottom - sourceRect.top;
    if (croppedWidth <= 0.0f)
    {
        return { 0.0f, 0.0f };
    }

    float scale = Constants::TITLE_BUTTON_TARGET_WIDTH / croppedWidth;
    return { croppedWidth * scale, croppedHeight * scale };
}

Vector2 TitleScene::GetButtonHitboxCenter(int slotIndex) const
{
    // BUTTON_SHEET_SOURCE_RECTS가 알파 채널로 측정한 실제 버튼 모양이라, 그리는 중심을 그대로 쓴다
    return GetButtonSlotCenter(slotIndex);
}

Vector2 TitleScene::GetButtonHitboxSize() const
{
    // 그리는 크기 자체가 이미 버튼 모양에 맞게 측정된 값이라 그대로 판정 크기로 쓴다
    return GetButtonSize();
}

Vector2 TitleScene::GetOptionPanelCenter() const
{
    return { Constants::WINDOW_WIDTH / 2.0f, Constants::OPTION_PANEL_CENTER_Y };
}

Vector2 TitleScene::GetOptionPanelSize() const
{
    // 실제로 그리는 건 POPUP_PANEL_SOURCE_RECT로 오려낸 부분뿐이니, 캔버스 전체가 아니라
    // 그 잘린 영역의 가로세로 비율을 기준으로 계산해야 흰 여백까지 포함해 찌부러지지 않는다
    const SpriteInfo& popupSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Pop.png");
    if (!popupSprite.bitmap)
    {
        return { 0.0f, 0.0f };
    }

    float croppedWidth = POPUP_PANEL_SOURCE_RECT.right - POPUP_PANEL_SOURCE_RECT.left;
    float croppedHeight = POPUP_PANEL_SOURCE_RECT.bottom - POPUP_PANEL_SOURCE_RECT.top;
    float scale = Constants::OPTION_PANEL_IMAGE_TARGET_WIDTH / croppedWidth;
    return { croppedWidth * scale, croppedHeight * scale };
}

Vector2 TitleScene::GetPopupCloseButtonCenter() const
{
    Vector2 panelCenter = GetOptionPanelCenter();
    Vector2 panelSize = GetOptionPanelSize();
    Vector2 panelTopLeft = { panelCenter.x - panelSize.x / 2.0f, panelCenter.y - panelSize.y / 2.0f };

    float croppedWidth = POPUP_PANEL_SOURCE_RECT.right - POPUP_PANEL_SOURCE_RECT.left;
    float scale = panelSize.x / croppedWidth;

    float offsetX = (POPUP_CLOSE_BUTTON_SOURCE_RECT.left + POPUP_CLOSE_BUTTON_SOURCE_RECT.right) / 2.0f - POPUP_PANEL_SOURCE_RECT.left;
    float offsetY = (POPUP_CLOSE_BUTTON_SOURCE_RECT.top + POPUP_CLOSE_BUTTON_SOURCE_RECT.bottom) / 2.0f - POPUP_PANEL_SOURCE_RECT.top;

    return { panelTopLeft.x + offsetX * scale, panelTopLeft.y + offsetY * scale };
}

Vector2 TitleScene::GetPopupCloseButtonSize() const
{
    Vector2 panelSize = GetOptionPanelSize();
    float croppedWidth = POPUP_PANEL_SOURCE_RECT.right - POPUP_PANEL_SOURCE_RECT.left;
    float scale = panelSize.x / croppedWidth;

    return { (POPUP_CLOSE_BUTTON_SOURCE_RECT.right - POPUP_CLOSE_BUTTON_SOURCE_RECT.left) * scale,
        (POPUP_CLOSE_BUTTON_SOURCE_RECT.bottom - POPUP_CLOSE_BUTTON_SOURCE_RECT.top) * scale };
}
Vector2 TitleScene::GetSoundBarGroupOrigin() const
{
    Vector2 panelCenter = GetOptionPanelCenter();
    float panelLeft = panelCenter.x - GetOptionPanelSize().x / 2.0f;
    float panelBottom = panelCenter.y + GetOptionPanelSize().y / 2.0f;
    return { panelLeft + Constants::OPTION_PANEL_PADDING_X, panelBottom - Constants::OPTION_PANEL_BAR_BOTTOM_MARGIN };
}

Vector2 TitleScene::GetSoundBarElementSize(const D2D1_RECT_F& sourceRect)
{
    return { (sourceRect.right - sourceRect.left) * Constants::SOUND_BAR_GROUP_SCALE,
        (sourceRect.bottom - sourceRect.top) * Constants::SOUND_BAR_GROUP_SCALE };
}

Vector2 TitleScene::GetSoundBarElementCenter(const D2D1_RECT_F& sourceRect) const
{
    // 원본에서 이 요소가 그룹 기준점(SOUND_BAR_GROUP_ORIGIN_SOURCE_X/BASELINE_SOURCE_Y)으로부터
    // 얼마나 떨어져 있는지를 그대로 화면 배율로 옮긴다 — 그래서 요소 사이 간격 비율이 원본과 같다
    Vector2 origin = GetSoundBarGroupOrigin();
    Vector2 size = GetSoundBarElementSize(sourceRect);
    float left = origin.x + (sourceRect.left - SOUND_BAR_GROUP_ORIGIN_SOURCE_X) * Constants::SOUND_BAR_GROUP_SCALE;
    float bottom = origin.y + (sourceRect.bottom - SOUND_BAR_GROUP_BASELINE_SOURCE_Y) * Constants::SOUND_BAR_GROUP_SCALE;
    return { left + size.x / 2.0f, bottom - size.y / 2.0f };
}

Vector2 TitleScene::GetSpeakerIconCenter() const
{
    return GetSoundBarElementCenter(SOUND_BAR_SPEAKER_SOURCE_RECT);
}

Vector2 TitleScene::GetVolumeBarCenter(int barIndex) const
{
    return GetSoundBarElementCenter(SOUND_BAR_SLOT_SOURCE_RECTS[barIndex]);
}

Vector2 TitleScene::GetVolumeBarSize(int barIndex) const
{
    return GetSoundBarElementSize(SOUND_BAR_SLOT_SOURCE_RECTS[barIndex]);
}

int TitleScene::GetFilledVolumeBarCount() const
{
    float volume = SoundManager::GetInstance().GetVolume();
    int count = static_cast<int>(std::lround(volume / Constants::VOLUME_BAR_STEP));
    return std::clamp(count, 0, Constants::VOLUME_BAR_COUNT);
}

bool TitleScene::IsPointInRect(Vector2 point, Vector2 rectCenter, Vector2 rectSize)
{
    return point.x >= rectCenter.x - rectSize.x / 2.0f && point.x <= rectCenter.x + rectSize.x / 2.0f &&
        point.y >= rectCenter.y - rectSize.y / 2.0f && point.y <= rectCenter.y + rectSize.y / 2.0f;
}
Vector2 TitleScene::GetRankingPanelCenter() const
{
    return { Constants::WINDOW_WIDTH / 2.0f, Constants::OPTION_PANEL_CENTER_Y };
}

Vector2 TitleScene::GetRankingPanelSize() const
{
    // (창 너비*WIDTH_RATIO, 창 높이*HEIGHT_RATIO) 박스 안에 원본 비율을 유지한 채 최대한 크게 들어가도록
    // 맞춘다 — 가로 기준 배율과 세로 기준 배율 중 더 작은 쪽을 써야 어느 한쪽이 박스를 넘치지 않는다
    // (Title.png를 창에 맞춰 그릴 때 쓰는 것과 같은 방식)
    float croppedWidth = RANKING_PANEL_SOURCE_RECT.right - RANKING_PANEL_SOURCE_RECT.left;
    float croppedHeight = RANKING_PANEL_SOURCE_RECT.bottom - RANKING_PANEL_SOURCE_RECT.top;

    float scale = std::min(
        (Constants::WINDOW_WIDTH * Constants::RANKING_PANEL_WIDTH_RATIO) / croppedWidth,
        (Constants::WINDOW_HEIGHT * Constants::RANKING_PANEL_HEIGHT_RATIO) / croppedHeight);
    return { croppedWidth * scale, croppedHeight * scale };
}

Vector2 TitleScene::GetRankingCloseButtonCenter() const
{
    Vector2 panelCenter = GetRankingPanelCenter();
    Vector2 panelSize = GetRankingPanelSize();
    Vector2 panelTopLeft = { panelCenter.x - panelSize.x / 2.0f, panelCenter.y - panelSize.y / 2.0f };

    float croppedWidth = RANKING_PANEL_SOURCE_RECT.right - RANKING_PANEL_SOURCE_RECT.left;
    float scale = panelSize.x / croppedWidth;

    float offsetX = (RANKING_CLOSE_BUTTON_SOURCE_RECT.left + RANKING_CLOSE_BUTTON_SOURCE_RECT.right) / 2.0f - RANKING_PANEL_SOURCE_RECT.left;
    float offsetY = (RANKING_CLOSE_BUTTON_SOURCE_RECT.top + RANKING_CLOSE_BUTTON_SOURCE_RECT.bottom) / 2.0f - RANKING_PANEL_SOURCE_RECT.top;

    return { panelTopLeft.x + offsetX * scale, panelTopLeft.y + offsetY * scale };
}

Vector2 TitleScene::GetRankingCloseButtonSize() const
{
    Vector2 panelSize = GetRankingPanelSize();
    float croppedWidth = RANKING_PANEL_SOURCE_RECT.right - RANKING_PANEL_SOURCE_RECT.left;
    float scale = panelSize.x / croppedWidth;

    return { (RANKING_CLOSE_BUTTON_SOURCE_RECT.right - RANKING_CLOSE_BUTTON_SOURCE_RECT.left) * scale,
        (RANKING_CLOSE_BUTTON_SOURCE_RECT.bottom - RANKING_CLOSE_BUTTON_SOURCE_RECT.top) * scale };
}
void TitleScene::Update(float deltaTime)
{
    // [연출] 제목이 위에서 떨어지다가 제자리(오프셋 0)에 닿으면 튕겨 올랐다 잦아들며 멈춘다.
    // 블록 물리와 같은 "중력 가속 + 바닥 반발" 개념을 제목 하나의 세로 오프셋에만 적용한 것.
    if (!m_titleDropSettled)
    {
        m_titleDropVelocityY += Constants::TITLE_DROP_GRAVITY * deltaTime;
        m_titleDropOffsetY += m_titleDropVelocityY * deltaTime;

        if (m_titleDropOffsetY >= 0.0f)
        {
            m_titleDropOffsetY = 0.0f;

            if (std::fabs(m_titleDropVelocityY) < Constants::TITLE_DROP_SETTLE_SPEED)
            {
                m_titleDropVelocityY = 0.0f;
                m_titleDropSettled = true;
            }
            else
            {
                m_titleDropVelocityY = -m_titleDropVelocityY * Constants::TITLE_DROP_BOUNCE_RESTITUTION;
            }
        }
    }

    Vector2 mousePos = InputManager::GetInstance().GetMousePosition();
    bool isLeftClick = InputManager::GetInstance().IsKeyPressed(VK_LBUTTON);

    // 패널을 여는 판정은 항상 체크하되, 다른 패널이 이미 열려있으면 새로 열 수 없게 막는다
    // (그래야 Option/Ranking이 동시에 열려서 겹쳐 그려지는 일이 없다)
    bool isMouseOverOption = IsPointInRect(mousePos, GetButtonHitboxCenter(BUTTON_SLOT_OPTION), GetButtonHitboxSize());
    if (isMouseOverOption && isLeftClick && !m_showRankingPanel)
    {
        m_showOptionPanel = !m_showOptionPanel;
    }

    bool isMouseOverRanking = IsPointInRect(mousePos, GetButtonHitboxCenter(BUTTON_SLOT_RANKING), GetButtonHitboxSize());
    if (isMouseOverRanking && isLeftClick && !m_showOptionPanel)
    {
        m_showRankingPanel = !m_showRankingPanel;
    }

    if (m_showOptionPanel)
    {
        // 패널이 떠 있는 동안은 모달처럼 동작 — 뒤의 Start/Exit는 판정하지 않고 볼륨 바만 본다
        if (isLeftClick)
        {
            if (IsPointInRect(mousePos, GetPopupCloseButtonCenter(), GetPopupCloseButtonSize()))
            {
                m_showOptionPanel = false;
            }
            else
            {
                for (int barIndex = 0; barIndex < Constants::VOLUME_BAR_COUNT; ++barIndex)
                {
                    if (IsPointInRect(mousePos, GetVolumeBarCenter(barIndex), GetVolumeBarSize(barIndex)))
                    {
                        SoundManager::GetInstance().SetVolume(static_cast<float>(barIndex + 1) * Constants::VOLUME_BAR_STEP);
                        break;
                    }
                }
            }
        }
    }
    else if (m_showRankingPanel)
    {
        // Ranking도 마찬가지로 모달처럼 동작 — X 닫기만 판정한다
        if (isLeftClick && IsPointInRect(mousePos, GetRankingCloseButtonCenter(), GetRankingCloseButtonSize()))
        {
            m_showRankingPanel = false;
        }
    }
    else
    {
        bool isMouseOverStart = IsPointInRect(mousePos, GetButtonHitboxCenter(BUTTON_SLOT_START), GetButtonHitboxSize());
        if (isMouseOverStart && isLeftClick)
        {
            SceneManager::GetInstance().ChangeScene(SceneType::Play);
        }

        bool isMouseOverExit = IsPointInRect(mousePos, GetButtonHitboxCenter(BUTTON_SLOT_EXIT), GetButtonHitboxSize());
        if (isMouseOverExit && isLeftClick)
        {
            // WM_DESTROY -> PostQuitMessage로 이어지도록, DefWindowProc이 처리하는 정상 종료 경로(WM_CLOSE)를 태운다
            PostMessage(WindowManager::GetInstance().GetWindowHandle(), WM_CLOSE, 0, 0);
        }
    }
    if (InputManager::GetInstance().IsKeyPressed(VK_F1))
    {
        m_showDebug = !m_showDebug;
    }
}

void TitleScene::Render(ID2D1RenderTarget* renderTarget)
{
    // 배경은 비율을 유지한 채, 창을 빈틈없이 덮을 때까지(둘 중 더 많이 늘려야 하는 쪽 기준) 키워서 그린다.
    // 그러면 남는 쪽(보통 세로)이 창보다 커지는데, 아래쪽을 기준으로 붙여서 위쪽만 잘리게 한다.
    const SpriteInfo& backgroundSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/TitleBackGround.png");
    if (backgroundSprite.bitmap)
    {
        D2D1_SIZE_F nativeSize = backgroundSprite.bitmap->GetSize();
        float scale = std::max(Constants::WINDOW_WIDTH / nativeSize.width, Constants::WINDOW_HEIGHT / nativeSize.height);
        Vector2 backgroundSize = { nativeSize.width * scale, nativeSize.height * scale };
        Vector2 backgroundCenter = { Constants::WINDOW_WIDTH / 2.0f, Constants::WINDOW_HEIGHT - backgroundSize.y / 2.0f };
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, backgroundSprite, backgroundCenter, backgroundSize, 0.0f, 0);
    }

    const SpriteInfo& titleSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Title.png");
    if (titleSprite.bitmap)
    {
        // 창 안에 완전히 들어오도록(넘치지 않게) 원본 비율을 유지한 채 축소해서 그린다
        D2D1_SIZE_F nativeSize = titleSprite.bitmap->GetSize();
        float scale = std::min(Constants::WINDOW_WIDTH / nativeSize.width, Constants::WINDOW_HEIGHT / nativeSize.height) * Constants::TITLE_IMAGE_SCALE;
        Vector2 titleSize = { nativeSize.width * scale, nativeSize.height * scale };
        Vector2 titleCenter = { Constants::WINDOW_WIDTH / 2.0f, Constants::TITLE_IMAGE_CENTER_Y + m_titleDropOffsetY };
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, titleSprite, titleCenter, titleSize, 0.0f, 0);
    }

    const SpriteInfo& buttonSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Button.png");
    if (buttonSprite.bitmap)
    {
        D2D1_RECT_F startRect = GetButtonSlotSourceRect(BUTTON_SLOT_START);
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, buttonSprite, GetButtonSlotCenter(BUTTON_SLOT_START), GetButtonSize(), 0.0f, 0, 1.0f, &startRect);

        // Option은 볼륨 패널이 연결돼 있어 정상 밝기로 그린다
        D2D1_RECT_F optionRect = GetButtonSlotSourceRect(BUTTON_SLOT_OPTION);
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, buttonSprite, GetButtonSlotCenter(BUTTON_SLOT_OPTION), GetButtonSize(), 0.0f, 0, 1.0f, &optionRect);

        D2D1_RECT_F rankingRect = GetButtonSlotSourceRect(BUTTON_SLOT_RANKING);
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, buttonSprite, GetButtonSlotCenter(BUTTON_SLOT_RANKING), GetButtonSize(), 0.0f, 0, 1.0f, &rankingRect);

        D2D1_RECT_F exitRect = GetButtonSlotSourceRect(BUTTON_SLOT_EXIT);
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, buttonSprite, GetButtonSlotCenter(BUTTON_SLOT_EXIT), GetButtonSize(), 0.0f, 0, 1.0f, &exitRect);
    }

    if (m_showOptionPanel)
    {
        const SpriteInfo& popupSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Pop.png");
        if (popupSprite.bitmap)
        {
            RenderManager::GetInstance().DrawSpriteRotated(renderTarget, popupSprite, GetOptionPanelCenter(), GetOptionPanelSize(),
                0.0f, 0, 1.0f, &POPUP_PANEL_SOURCE_RECT);
        }

        const SpriteInfo& soundBarSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/soundbar.png");
        if (soundBarSprite.bitmap)
        {
            // 스피커 아이콘(스피커 몸통+음파) 한 장을, 원본 크기 비율 그대로(SOUND_BAR_GROUP_SCALE만
            // 적용) 그린다
            RenderManager::GetInstance().DrawSpriteRotated(renderTarget, soundBarSprite, GetSpeakerIconCenter(), GetSoundBarElementSize(SOUND_BAR_SPEAKER_SOURCE_RECT),
                0.0f, 0, 1.0f, &SOUND_BAR_SPEAKER_SOURCE_RECT);

            // 현재 볼륨만큼 앞 칸부터 원래 밝기로, 그 뒤 칸은 흐리게 — 몇 칸이 채워졌는지 한눈에 보이게 한다
            int filledCount = GetFilledVolumeBarCount();
            for (int barIndex = 0; barIndex < Constants::VOLUME_BAR_COUNT; ++barIndex)
            {
                float opacity = barIndex < filledCount ? 1.0f : Constants::VOLUME_BAR_DIM_OPACITY;
                D2D1_RECT_F barSourceRect = SOUND_BAR_SLOT_SOURCE_RECTS[barIndex];
                RenderManager::GetInstance().DrawSpriteRotated(renderTarget, soundBarSprite, GetVolumeBarCenter(barIndex), GetVolumeBarSize(barIndex),
                    0.0f, 0, opacity, &barSourceRect);
            }
        }
    }

    if (m_showRankingPanel)
    {
        const SpriteInfo& rankingSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Ranking.png");
        if (rankingSprite.bitmap)
        {
            RenderManager::GetInstance().DrawSpriteRotated(renderTarget, rankingSprite, GetRankingPanelCenter(), GetRankingPanelSize(),
                0.0f, 0, 1.0f, &RANKING_PANEL_SOURCE_RECT);
        }
    }

    if (m_showDebug)
    {
        RenderManager::GetInstance().DrawDebugRect(renderTarget, GetButtonHitboxCenter(BUTTON_SLOT_START), GetButtonHitboxSize(), Constants::COLLIDER_DEBUG_COLOR);
        RenderManager::GetInstance().DrawDebugRect(renderTarget, GetButtonHitboxCenter(BUTTON_SLOT_OPTION), GetButtonHitboxSize(), Constants::COLLIDER_DEBUG_COLOR);
        RenderManager::GetInstance().DrawDebugRect(renderTarget, GetButtonHitboxCenter(BUTTON_SLOT_RANKING), GetButtonHitboxSize(), Constants::COLLIDER_DEBUG_COLOR);
        RenderManager::GetInstance().DrawDebugRect(renderTarget, GetButtonHitboxCenter(BUTTON_SLOT_EXIT), GetButtonHitboxSize(), Constants::COLLIDER_DEBUG_COLOR);

        if (m_showOptionPanel)
        {
            for (int barIndex = 0; barIndex < Constants::VOLUME_BAR_COUNT; ++barIndex)
            {
                RenderManager::GetInstance().DrawDebugRect(renderTarget, GetVolumeBarCenter(barIndex), GetVolumeBarSize(barIndex), Constants::COLLIDER_DEBUG_COLOR);
            }
        }
    }
}
