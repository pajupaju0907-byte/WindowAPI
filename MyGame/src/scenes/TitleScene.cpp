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
    // Button.png 안에서 Start=0, Option=1, Ranking=2 순서로 같은 높이 3칸이 이어져 있다는 뜻
    constexpr int BUTTON_SLOT_START = 0;
    constexpr int BUTTON_SLOT_OPTION = 1;
    constexpr int BUTTON_SLOT_RANKING = 2;

    // 볼륨 바 5칸 색상(참고 이미지처럼 왼쪽부터 빨강~보라 순서)
    constexpr COLORREF VOLUME_BAR_COLORS[Constants::VOLUME_BAR_COUNT] = {
        RGB(240, 90, 105), RGB(245, 150, 60), RGB(170, 210, 60), RGB(80, 190, 230), RGB(160, 110, 220)
    };
}

void TitleScene::Enter()
{
    ResourceManager::GetInstance().LoadSprite("assets/TitleBackGround.png");
    ResourceManager::GetInstance().LoadSprite("assets/Title.png");
    ResourceManager::GetInstance().LoadSprite("assets/Button.png");
    ResourceManager::GetInstance().LoadSprite("assets/Exit.png");

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
    const SpriteInfo& buttonSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Button.png");
    if (!buttonSprite.bitmap)
    {
        return D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
    }

    D2D1_SIZE_F nativeSize = buttonSprite.bitmap->GetSize();
    float slotHeight = nativeSize.height / 3.0f;
    float top = slotHeight * static_cast<float>(slotIndex);
    return D2D1::RectF(0.0f, top, nativeSize.width, top + slotHeight);
}

Vector2 TitleScene::GetButtonSlotCenter(int slotIndex) const
{
    // slotIndex가 클수록 Start 자리 밑으로 버튼 한 칸 높이씩(GetButtonSize().y) 이어 붙는다
    float stepY = GetButtonSize().y;
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
    Vector2 center = GetButtonSlotCenter(slotIndex);
    center.y += Constants::TITLE_BUTTON_HITBOX_OFFSET_Y;
    return center;
}

Vector2 TitleScene::GetButtonHitboxSize() const
{
    // 그리는 크기보다 좁게(또는 넓게) 잡아서, 옆 칸(Option/Ranking)까지 눌리지 않으면서도 실제
    // 버튼 그림 모양(가로로 넓고 얇은 형태 등)에 맞출 수 있게 가로/세로를 따로 조절한다
    Vector2 drawnSize = GetButtonSize();
    return { drawnSize.x * Constants::TITLE_BUTTON_HITBOX_SCALE_X, drawnSize.y * Constants::TITLE_BUTTON_HITBOX_SCALE_Y };
}

Vector2 TitleScene::GetExitButtonCenter() const
{
    return { Constants::WINDOW_WIDTH / 2.0f, Constants::TITLE_EXIT_BUTTON_CENTER_Y };
}

Vector2 TitleScene::GetExitButtonSize() const
{
    const SpriteInfo& exitSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Exit.png");
    if (!exitSprite.bitmap)
    {
        return { 0.0f, 0.0f };
    }

    D2D1_SIZE_F nativeSize = exitSprite.bitmap->GetSize();
    float scale = Constants::TITLE_EXIT_BUTTON_TARGET_WIDTH / nativeSize.width;
    return { nativeSize.width * scale, nativeSize.height * scale };
}

Vector2 TitleScene::GetOptionPanelCenter() const
{
    return { Constants::WINDOW_WIDTH / 2.0f, Constants::OPTION_PANEL_CENTER_Y };
}

Vector2 TitleScene::GetOptionPanelSize() const
{
    return { Constants::OPTION_PANEL_WIDTH, Constants::OPTION_PANEL_HEIGHT };
}

Vector2 TitleScene::GetSpeakerIconCenter() const
{
    Vector2 panelCenter = GetOptionPanelCenter();
    float panelLeft = panelCenter.x - GetOptionPanelSize().x / 2.0f;
    float iconWidth = Constants::SPEAKER_ICON_BODY_WIDTH + Constants::SPEAKER_ICON_CONE_WIDTH;
    return { panelLeft + Constants::OPTION_PANEL_PADDING_X + iconWidth / 2.0f, panelCenter.y };
}

Vector2 TitleScene::GetVolumeBarCenter(int barIndex) const
{
    Vector2 panelCenter = GetOptionPanelCenter();
    float panelLeft = panelCenter.x - GetOptionPanelSize().x / 2.0f;
    float iconWidth = Constants::SPEAKER_ICON_BODY_WIDTH + Constants::SPEAKER_ICON_CONE_WIDTH;

    // 스피커 아이콘 오른쪽에 여백을 두고 첫 번째 바를 놓은 뒤, 바 너비+간격만큼씩 오른쪽으로 이어 붙인다
    float firstBarLeft = panelLeft + Constants::OPTION_PANEL_PADDING_X + iconWidth + Constants::OPTION_PANEL_PADDING_X;
    float barCenterX = firstBarLeft + Constants::VOLUME_BAR_WIDTH / 2.0f
        + static_cast<float>(barIndex) * (Constants::VOLUME_BAR_WIDTH + Constants::VOLUME_BAR_GAP);

    // 모든 바의 아랫변(바닥선)을 패널 아래쪽 끝 기준으로 맞추고, 칸마다 다른 높이만큼 중심을 위로 올린다
    float panelBottom = panelCenter.y + GetOptionPanelSize().y / 2.0f;
    float baselineY = panelBottom - Constants::OPTION_PANEL_BAR_BOTTOM_MARGIN;
    float barCenterY = baselineY - GetVolumeBarSize(barIndex).y / 2.0f;

    return { barCenterX, barCenterY };
}

Vector2 TitleScene::GetVolumeBarSize(int barIndex) const
{
    float height = Constants::VOLUME_BAR_MIN_HEIGHT + Constants::VOLUME_BAR_HEIGHT_STEP * static_cast<float>(barIndex);
    return { Constants::VOLUME_BAR_WIDTH, height };
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

    // Option 버튼은 패널이 열려있는지와 무관하게 항상 눌러서 토글할 수 있어야(닫을 수도 있어야) 한다
    bool isMouseOverOption = IsPointInRect(mousePos, GetButtonHitboxCenter(BUTTON_SLOT_OPTION), GetButtonHitboxSize());
    if (isMouseOverOption && isLeftClick)
    {
        m_showOptionPanel = !m_showOptionPanel;
    }

    if (m_showOptionPanel)
    {
        // 패널이 떠 있는 동안은 모달처럼 동작 — 뒤의 Start/Exit는 판정하지 않고 볼륨 바만 본다
        if (isLeftClick)
        {
            for (int barIndex = 0; barIndex < Constants::VOLUME_BAR_COUNT; ++barIndex)
            {
                // 바마다 높이가 다르므로 클릭 판정도 GetVolumeBarSize와 같은 계단식 크기를 그대로 써야
                // 짧은 칸의 빈 위쪽 공간까지 눌리지 않는다
                if (IsPointInRect(mousePos, GetVolumeBarCenter(barIndex), GetVolumeBarSize(barIndex)))
                {
                    SoundManager::GetInstance().SetVolume(static_cast<float>(barIndex + 1) * Constants::VOLUME_BAR_STEP);
                    break;
                }
            }
        }
    }
    else
    {
        bool isMouseOverStart = IsPointInRect(mousePos, GetButtonHitboxCenter(BUTTON_SLOT_START), GetButtonHitboxSize());
        if (isMouseOverStart && isLeftClick)
        {
            SceneManager::GetInstance().ChangeScene(SceneType::Play);
        }

        // Ranking은 아직 구현이 없어 비활성 — 클릭 판정 자체를 안 한다(그려지기만 함)

        bool isMouseOverExit = IsPointInRect(mousePos, GetExitButtonCenter(), GetExitButtonSize());
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
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, buttonSprite, GetButtonSlotCenter(BUTTON_SLOT_RANKING), GetButtonSize(), 0.0f, 0, Constants::TITLE_DISABLED_BUTTON_OPACITY, &rankingRect);
    }

    const SpriteInfo& exitSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Exit.png");
    if (exitSprite.bitmap)
    {
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, exitSprite, GetExitButtonCenter(), GetExitButtonSize(), 0.0f, 0);
    }

    if (m_showOptionPanel)
    {
        RenderManager::GetInstance().FillRect(renderTarget, GetOptionPanelCenter(), GetOptionPanelSize(),
            Constants::OPTION_PANEL_BACKGROUND_COLOR, Constants::OPTION_PANEL_BACKGROUND_OPACITY);

        // 스피커 아이콘 = 사각 몸통 + 오른쪽으로 벌어지는 사다리꼴 콘, 두 도형을 이어붙여 하나의 스피커
        // 모양처럼 보이게 한다
        Vector2 speakerCenter = GetSpeakerIconCenter();
        float speakerLeft = speakerCenter.x - (Constants::SPEAKER_ICON_BODY_WIDTH + Constants::SPEAKER_ICON_CONE_WIDTH) / 2.0f;
        float bodyRight = speakerLeft + Constants::SPEAKER_ICON_BODY_WIDTH;
        float coneRight = bodyRight + Constants::SPEAKER_ICON_CONE_WIDTH;
        float bodyHalfHeight = Constants::SPEAKER_ICON_BODY_HEIGHT / 2.0f;
        float coneHalfHeight = Constants::SPEAKER_ICON_CONE_HEIGHT / 2.0f;

        Vector2 bodyCenter = { speakerLeft + Constants::SPEAKER_ICON_BODY_WIDTH / 2.0f, speakerCenter.y };
        Vector2 bodySize = { Constants::SPEAKER_ICON_BODY_WIDTH, Constants::SPEAKER_ICON_BODY_HEIGHT };
        RenderManager::GetInstance().FillRect(renderTarget, bodyCenter, bodySize, Constants::SPEAKER_ICON_COLOR, 1.0f);

        Vector2 conePoints[4] = {
            { bodyRight, speakerCenter.y - bodyHalfHeight },
            { coneRight, speakerCenter.y - coneHalfHeight },
            { coneRight, speakerCenter.y + coneHalfHeight },
            { bodyRight, speakerCenter.y + bodyHalfHeight }
        };
        RenderManager::GetInstance().FillPolygon(renderTarget, conePoints, 4, Constants::SPEAKER_ICON_COLOR, 1.0f);

        // 현재 볼륨만큼 앞 칸부터 원색으로, 그 뒤 칸은 흐리게 — 몇 칸이 채워졌는지 한눈에 보이게 한다
        int filledCount = GetFilledVolumeBarCount();
        for (int barIndex = 0; barIndex < Constants::VOLUME_BAR_COUNT; ++barIndex)
        {
            float opacity = barIndex < filledCount ? 1.0f : Constants::VOLUME_BAR_DIM_OPACITY;
            RenderManager::GetInstance().FillRect(renderTarget, GetVolumeBarCenter(barIndex), GetVolumeBarSize(barIndex),
                VOLUME_BAR_COLORS[barIndex], opacity);
        }
    }

    if (m_showDebug)
    {
        RenderManager::GetInstance().DrawDebugRect(renderTarget, GetButtonHitboxCenter(BUTTON_SLOT_START), GetButtonHitboxSize(), Constants::COLLIDER_DEBUG_COLOR);
        RenderManager::GetInstance().DrawDebugRect(renderTarget, GetButtonHitboxCenter(BUTTON_SLOT_OPTION), GetButtonHitboxSize(), Constants::COLLIDER_DEBUG_COLOR);
        RenderManager::GetInstance().DrawDebugRect(renderTarget, GetExitButtonCenter(), GetExitButtonSize(), Constants::COLLIDER_DEBUG_COLOR);

        if (m_showOptionPanel)
        {
            for (int barIndex = 0; barIndex < Constants::VOLUME_BAR_COUNT; ++barIndex)
            {
                RenderManager::GetInstance().DrawDebugRect(renderTarget, GetVolumeBarCenter(barIndex), GetVolumeBarSize(barIndex), Constants::COLLIDER_DEBUG_COLOR);
            }
        }
    }
}
