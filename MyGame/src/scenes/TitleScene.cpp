#include "pch.h"

#include <algorithm>
#include <cmath>

#include "TitleScene.h"
#include "../managers/ResourceManager.h"
#include "../managers/RenderManager.h"
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
}

void TitleScene::Enter()
{
    ResourceManager::GetInstance().LoadSprite("assets/TitleBackGround.png");
    ResourceManager::GetInstance().LoadSprite("assets/Title.png");
    ResourceManager::GetInstance().LoadSprite("assets/Button.png");
    ResourceManager::GetInstance().LoadSprite("assets/Exit.png");

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

Vector2 TitleScene::GetButtonHitboxCenter() const
{
    Vector2 center = GetButtonSlotCenter(BUTTON_SLOT_START);
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

    bool isMouseOverStart = IsPointInRect(mousePos, GetButtonHitboxCenter(), GetButtonHitboxSize());
    if (isMouseOverStart && isLeftClick)
    {
        SceneManager::GetInstance().ChangeScene(SceneType::Play);
    }

    // Option/Ranking은 아직 구현이 없어 비활성 — 클릭 판정 자체를 안 한다(그려지기만 함)

    bool isMouseOverExit = IsPointInRect(mousePos, GetExitButtonCenter(), GetExitButtonSize());
    if (isMouseOverExit && isLeftClick)
    {
        // WM_DESTROY -> PostQuitMessage로 이어지도록, DefWindowProc이 처리하는 정상 종료 경로(WM_CLOSE)를 태운다
        PostMessage(WindowManager::GetInstance().GetWindowHandle(), WM_CLOSE, 0, 0);
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

        // Option/Ranking은 아직 기능이 없다는 걸 눈으로 알 수 있게 흐리게 그린다
        D2D1_RECT_F optionRect = GetButtonSlotSourceRect(BUTTON_SLOT_OPTION);
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, buttonSprite, GetButtonSlotCenter(BUTTON_SLOT_OPTION), GetButtonSize(), 0.0f, 0, Constants::TITLE_DISABLED_BUTTON_OPACITY, &optionRect);

        D2D1_RECT_F rankingRect = GetButtonSlotSourceRect(BUTTON_SLOT_RANKING);
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, buttonSprite, GetButtonSlotCenter(BUTTON_SLOT_RANKING), GetButtonSize(), 0.0f, 0, Constants::TITLE_DISABLED_BUTTON_OPACITY, &rankingRect);
    }

    const SpriteInfo& exitSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Exit.png");
    if (exitSprite.bitmap)
    {
        RenderManager::GetInstance().DrawSpriteRotated(renderTarget, exitSprite, GetExitButtonCenter(), GetExitButtonSize(), 0.0f, 0);
    }

    if (m_showDebug)
    {
        RenderManager::GetInstance().DrawDebugRect(renderTarget, GetButtonHitboxCenter(), GetButtonHitboxSize(), Constants::COLLIDER_DEBUG_COLOR);
        RenderManager::GetInstance().DrawDebugRect(renderTarget, GetExitButtonCenter(), GetExitButtonSize(), Constants::COLLIDER_DEBUG_COLOR);
    }
}
