#include "pch.h"

#include "GameOverScene.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>


#include "../managers/ResourceManager.h"
#include "../managers/RenderManager.h"
#include "../core/InputManager.h"
#include "../core/SceneManager.h"
#include "../core/WindowManager.h"
#include "../util/Constants.h"
#include "../util/Types.h"
#include "../managers/BlockManager.h"
#include "../managers/PlayerManager.h"
#include "../managers/RankingManager.h"
#include "../managers/SoundManager.h"
namespace
{
	// 화면에 쌓는 순서(위=0, 아래=1) — Button.png 시트 안의 어느 칸을 쓸지와는 별개의 값
	constexpr int BUTTON_SLOT_RETRY = 0;
	constexpr int BUTTON_SLOT_TITLE = 1;

	// Button.png(TitleScene.cpp와 같이 쓰는 6버튼 시트) 안에서 Title/Retry가 차지하는 실제 그림 영역.
	// 전체 6칸(TitleScene.cpp의 BUTTON_SHEET_SOURCE_RECTS) 중 5번째=Title, 6번째=Retry.
	constexpr D2D1_RECT_F TITLE_BUTTON_SOURCE_RECT = { 42.0f, 656.0f, 766.0f, 905.0f };
	constexpr D2D1_RECT_F RETRY_BUTTON_SOURCE_RECT = { 771.0f, 656.0f, 1492.0f, 905.0f };

	// NewRecord.png(1672x941)는 캔버스 전체가 아니라 가운데 "NEW RECORD" 글씨 부분만 그림이다.
	// 알파 채널을 스캔해 실제 그림이 있는 영역만 오려 쓴다.
	constexpr D2D1_RECT_F NEW_RECORD_TEXT_SOURCE_RECT = { 32.0f, 82.0f, 1654.0f, 915.0f };

	// Particles.png(1536x1024) 안에 흩어진 파티클 조각 18개 각각의 실제 그림 영역.
	// 알파 채널을 스캔해 서로 이어지지 않은 덩어리(연결 요소)를 하나씩 찾아 측정한 값 —
	// 한 프레임에 이 중 하나를 무작위로 골라 그리면 매번 다른 모양의 조각이 튄다.
	constexpr D2D1_RECT_F PARTICLE_SOURCE_RECTS[18] = {
		{ 109.0f, 52.0f, 368.0f, 518.0f },
		{ 647.0f, 84.0f, 989.0f, 320.0f },
		{ 1032.0f, 92.0f, 1176.0f, 256.0f },
		{ 432.0f, 97.0f, 609.0f, 270.0f },
		{ 1241.0f, 111.0f, 1441.0f, 336.0f },
		{ 946.0f, 338.0f, 1166.0f, 537.0f },
		{ 752.0f, 356.0f, 844.0f, 444.0f },
		{ 1275.0f, 381.0f, 1435.0f, 500.0f },
		{ 328.0f, 407.0f, 495.0f, 563.0f },
		{ 576.0f, 430.0f, 677.0f, 530.0f },
		{ 1275.0f, 569.0f, 1471.0f, 788.0f },
		{ 923.0f, 589.0f, 1272.0f, 789.0f },
		{ 765.0f, 590.0f, 869.0f, 678.0f },
		{ 398.0f, 621.0f, 734.0f, 785.0f },
		{ 102.0f, 648.0f, 267.0f, 732.0f },
		{ 238.0f, 817.0f, 337.0f, 932.0f },
		{ 674.0f, 818.0f, 1043.0f, 951.0f },
		{ 1128.0f, 823.0f, 1232.0f, 911.0f },
	};

	constexpr float TWO_PI = 6.28318530f;

	// [0, 1) 사이 난수 하나 — rand()/RAND_MAX 조합을 여기저기서 반복하지 않으려고 모아둔 헬퍼
	float RandomUnit()
	{
		return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	}
}

void GameOverScene::Enter()
{
	ResourceManager::GetInstance().LoadSprite("assets/GameOverBG.png");
	ResourceManager::GetInstance().LoadSprite("assets/GameOverText.png");
	ResourceManager::GetInstance().LoadSprite("assets/Button.png");
	ResourceManager::GetInstance().LoadSprite("assets/NewRecord.png");
	ResourceManager::GetInstance().LoadSprite("assets/Particles.png");

	float heightMeters = BlockManager::GetInstance().GetTallestHeightMeters();

	// 온라인 랭킹(RankingManager)과는 별개로, "이 컴퓨터에서 이 닉네임의 역대 최고 기록"을 로컬에
	// 들고 있다가 이번 점수가 그걸 넘었을 때만 신기록 팝업을 띄운다 — 온라인 등록은 네트워크
	// 스레드가 끝나야 결과를 알 수 있어서, 팝업처럼 즉시 반응해야 하는 연출에는 못 쓴다.
	m_isNewRecordActive = PlayerManager::GetInstance().TryUpdateBestScore(heightMeters);
	if (m_isNewRecordActive)
	{
		SpawnNewRecordPopup();
		SoundManager::GetInstance().PlaySfx("assets/Sound/newrecord.mp3");
	}
	else
	{
		SoundManager::GetInstance().PlaySfx("assets/Sound/GameOver.mp3");
	}

	RankingManager::GetInstance().SubmitScore(PlayerManager::GetInstance().GetNickname(), heightMeters);

	m_gameOverDropOffsetY = Constants::GAME_OVER_DROP_START_OFFSET_Y;
	m_gameOverDropVelocityY = 0.0f;
	m_gameOverDropSettled = false;
}

void GameOverScene::Exit()
{
	// TODO: 게임오버 씬 정리 로직 직접 구현
}

void GameOverScene::Update(float deltaTime)
{
	if (!m_gameOverDropSettled)
	{
		m_gameOverDropVelocityY += Constants::GAME_OVER_DROP_GRAVITY * deltaTime;
		m_gameOverDropOffsetY += m_gameOverDropVelocityY * deltaTime;

		if (m_gameOverDropOffsetY >= 0.0f)
		{
			m_gameOverDropOffsetY = 0.0f;

			if (std::fabs(m_gameOverDropVelocityY) < Constants::GAME_OVER_DROP_SETTLE_SPEED)
			{
				m_gameOverDropVelocityY = 0.0f;
				m_gameOverDropSettled = true;
			}
			else
			{
				m_gameOverDropVelocityY = -m_gameOverDropVelocityY * Constants::GAME_OVER_DROP_BOUNCE_RESTITUTION;
			}
		}
	}
	Vector2 mousePos = InputManager::GetInstance().GetMousePosition();
	bool isLeftClick = InputManager::GetInstance().IsKeyPressed(VK_LBUTTON);

	bool isMouseOverTitle = IsPointInRect(mousePos, GetButtonHitboxCenter(BUTTON_SLOT_TITLE), GetButtonHitboxSize(BUTTON_SLOT_TITLE));
	if (isMouseOverTitle && isLeftClick)
	{
		SoundManager::GetInstance().PlaySfx("assets/Sound/Bbong.mp3");
		SceneManager::GetInstance().ChangeScene(SceneType::Title);
	}
	bool isMouseOverRetry = IsPointInRect(mousePos, GetButtonHitboxCenter(BUTTON_SLOT_RETRY), GetButtonHitboxSize(BUTTON_SLOT_RETRY));
	if (isMouseOverRetry && isLeftClick)
	{
		SoundManager::GetInstance().PlaySfx("assets/Sound/Bbong.mp3");
		SceneManager::GetInstance().ChangeScene(SceneType::Play);
	}
	if (InputManager::GetInstance().IsKeyPressed(VK_F1))
	{
		m_showDebug = !m_showDebug;
	}

	UpdateNewRecordPopup(deltaTime);
}

void GameOverScene::Render(ID2D1RenderTarget* renderTarget)
{
	const SpriteInfo& backgroundSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/GameOverBG.png");
	if (backgroundSprite.bitmap)
	{
		D2D1_SIZE_F nativeSize = backgroundSprite.bitmap->GetSize();
		float scale = std::max(Constants::WINDOW_WIDTH / nativeSize.width, Constants::WINDOW_HEIGHT / nativeSize.height);
		Vector2 backgroundSize = { nativeSize.width * scale, nativeSize.height * scale };
		Vector2 backgroundCenter = { Constants::WINDOW_WIDTH / 2.0f, Constants::WINDOW_HEIGHT - backgroundSize.y / 2.0f };
		RenderManager::GetInstance().DrawSpriteRotated(renderTarget, backgroundSprite, backgroundCenter, backgroundSize, 0.0f, 0);
	}

	const SpriteInfo& GameoverSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/GameOverText.png");
	if (GameoverSprite.bitmap)
	{
		D2D1_SIZE_F nativeSize = GameoverSprite.bitmap->GetSize();
		float scale = std::min(Constants::WINDOW_WIDTH / nativeSize.width, Constants::WINDOW_HEIGHT / nativeSize.height) * Constants::GAME_OVER_IMAGE_SCALE;
		Vector2 GameoverSize = { nativeSize.width * scale, nativeSize.height * scale };
		Vector2 GameoverCenter = { Constants::WINDOW_WIDTH / 2.0f, Constants::GAME_OVER_IMAGE_CENTER_Y + m_gameOverDropOffsetY };
		RenderManager::GetInstance().DrawSpriteRotated(renderTarget, GameoverSprite, GameoverCenter, GameoverSize, 0.0f, 0);
	}
	RenderManager::GetInstance().DrawScorePanel(
		renderTarget,
		{ Constants::WINDOW_WIDTH / 2.0f, Constants::GAME_OVER_SCORE_CENTER_Y },
		BlockManager::GetInstance().GetTallestHeightMeters());

	RenderNewRecordPopup(renderTarget);

	const SpriteInfo& buttonSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Button.png");
	if (buttonSprite.bitmap)
	{
		D2D1_RECT_F TitleRect = GetButtonSlotSourceRect(BUTTON_SLOT_TITLE);
		RenderManager::GetInstance().DrawSpriteRotated(renderTarget, buttonSprite, GetButtonSlotCenter(BUTTON_SLOT_TITLE), GetButtonSize(), 0.0f, 0, 1.0f, &TitleRect);

		D2D1_RECT_F RetryRect = GetButtonSlotSourceRect(BUTTON_SLOT_RETRY);
		RenderManager::GetInstance().DrawSpriteRotated(renderTarget, buttonSprite, GetButtonSlotCenter(BUTTON_SLOT_RETRY), GetButtonSize(), 0.0f, 0, 1.0f, &RetryRect);



	}
	if (m_showDebug)
	{
		RenderManager::GetInstance().DrawDebugRect(renderTarget, GetButtonHitboxCenter(BUTTON_SLOT_TITLE), GetButtonHitboxSize(BUTTON_SLOT_TITLE), Constants::COLLIDER_DEBUG_COLOR);
		RenderManager::GetInstance().DrawDebugRect(renderTarget, GetButtonHitboxCenter(BUTTON_SLOT_RETRY), GetButtonHitboxSize(BUTTON_SLOT_RETRY), Constants::COLLIDER_DEBUG_COLOR);
	}
}
Vector2 GameOverScene::GetButtonSlotCenter(int slotIndex) const
{
	// slotIndex가 클수록 버튼 한 칸 높이 + 여백(TitleScene과 같은 TITLE_BUTTON_GAP_Y)씩 이어 붙는다
	float stepY = GetButtonSize().y + Constants::TITLE_BUTTON_GAP_Y;
	return { Constants::WINDOW_WIDTH / 2.0f, Constants::GAME_OVER_BUTTON_CENTER_Y + stepY * static_cast<float>(slotIndex) };
}

Vector2 GameOverScene::GetButtonSize() const
{
	// 실제로 그리는 건 한 칸(원본의 1/3)뿐이니, 그 부분의 가로세로 비율을 기준으로 계산해야
	// 이미지 전체 비율로 계산했을 때처럼 세로로 길쭉하게 늘어나 보이지 않는다.
	D2D1_RECT_F sourceRect = GetButtonSlotSourceRect(BUTTON_SLOT_TITLE);
	float croppedWidth = sourceRect.right - sourceRect.left;
	float croppedHeight = sourceRect.bottom - sourceRect.top;
	if (croppedWidth <= 0.0f)
	{
		return { 0.0f, 0.0f };
	}

	float scale = Constants::TITLE_BUTTON_TARGET_WIDTH / croppedWidth;
	return { croppedWidth * scale, croppedHeight * scale };
}

Vector2 GameOverScene::GetButtonHitboxCenter(int slotIndex) const
{
	// TITLE_BUTTON_SOURCE_RECT/RETRY_BUTTON_SOURCE_RECT가 알파 채널로 측정한 실제 버튼 모양이라,
	// 그리는 중심을 그대로 쓴다 (TitleScene::GetButtonHitboxCenter와 같은 이유)
	return GetButtonSlotCenter(slotIndex);
}

Vector2 GameOverScene::GetButtonHitboxSize(int slotIndex) const
{
	(void)slotIndex;
	// 그리는 크기 자체가 이미 버튼 모양에 맞게 측정된 값이라 그대로 판정 크기로 쓴다
	return GetButtonSize();
}


bool GameOverScene::IsPointInRect(Vector2 point, Vector2 rectCenter, Vector2 rectSize)
{
	return point.x >= rectCenter.x - rectSize.x / 2.0f && point.x <= rectCenter.x + rectSize.x / 2.0f &&
		point.y >= rectCenter.y - rectSize.y / 2.0f && point.y <= rectCenter.y + rectSize.y / 2.0f;
}
D2D1_RECT_F GameOverScene::GetButtonSlotSourceRect(int slotIndex) const
{
	return (slotIndex == BUTTON_SLOT_TITLE) ? TITLE_BUTTON_SOURCE_RECT : RETRY_BUTTON_SOURCE_RECT;
}

void GameOverScene::SpawnNewRecordPopup()
{
	m_newRecordElapsed = 0.0f;
	m_newRecordScale = Constants::NEW_RECORD_SPRING_START_SCALE;
	m_newRecordScaleVelocity = 0.0f;

	Vector2 center = { Constants::WINDOW_WIDTH / 2.0f, Constants::NEW_RECORD_POPUP_CENTER_Y };

	m_newRecordParticles.clear();
	for (int i = 0; i < Constants::NEW_RECORD_PARTICLE_COUNT; ++i)
	{
		// 360도 어느 방향으로나 튀어나가되(폭죽 터지는 느낌), 속도는 매번 달라서 흩어지는 정도가 제각각이다
		float angle = RandomUnit() * TWO_PI;
		float speed = Constants::NEW_RECORD_PARTICLE_MIN_SPEED
			+ RandomUnit() * (Constants::NEW_RECORD_PARTICLE_MAX_SPEED - Constants::NEW_RECORD_PARTICLE_MIN_SPEED);

		NewRecordParticle particle;
		particle.position = center;
		particle.velocity = { std::cos(angle) * speed, std::sin(angle) * speed };
		particle.angularVelocity = (RandomUnit() - 0.5f) * 2.0f * Constants::NEW_RECORD_PARTICLE_MAX_ANGULAR_SPEED;
		particle.sourceRectIndex = rand() % static_cast<int>(std::size(PARTICLE_SOURCE_RECTS));

		m_newRecordParticles.push_back(particle);
	}
}

void GameOverScene::UpdateNewRecordPopup(float deltaTime)
{
	if (!m_isNewRecordActive)
	{
		return;
	}

	m_newRecordElapsed += deltaTime;

	// 스프링 물리(F = -k*x - c*v)로 목표 크기 1.0을 향해 수렴시킨다. 감쇠(DAMPING)가 강성(STIFFNESS)에
	// 비해 약해서 한 번 살짝 오버슈트(1.0을 넘었다가 줄어듦)하는 "튀어나오는" 느낌이 난다 —
	// TitleScene의 낙하-반발 연출과 같은 아이디어를 위치 대신 크기에 적용한 것.
	float springAccel = (1.0f - m_newRecordScale) * Constants::NEW_RECORD_SPRING_STIFFNESS
		- m_newRecordScaleVelocity * Constants::NEW_RECORD_SPRING_DAMPING;
	m_newRecordScaleVelocity += springAccel * deltaTime;
	m_newRecordScale += m_newRecordScaleVelocity * deltaTime;

	for (NewRecordParticle& particle : m_newRecordParticles)
	{
		particle.velocity.y += Constants::NEW_RECORD_PARTICLE_GRAVITY * deltaTime;
		particle.position += particle.velocity * deltaTime;
		particle.rotation += particle.angularVelocity * deltaTime;
	}

	if (m_newRecordElapsed >= Constants::NEW_RECORD_POPUP_DURATION)
	{
		m_isNewRecordActive = false;
	}
}

float GameOverScene::GetNewRecordPopupOpacity() const
{
	float fadeStartTime = Constants::NEW_RECORD_POPUP_DURATION - Constants::NEW_RECORD_FADE_DURATION;
	if (m_newRecordElapsed <= fadeStartTime)
	{
		return 1.0f;
	}

	float fadeProgress = (m_newRecordElapsed - fadeStartTime) / Constants::NEW_RECORD_FADE_DURATION;
	return std::clamp(1.0f - fadeProgress, 0.0f, 1.0f);
}

void GameOverScene::RenderNewRecordPopup(ID2D1RenderTarget* renderTarget) const
{
	if (!m_isNewRecordActive)
	{
		return;
	}

	float opacity = GetNewRecordPopupOpacity();

	// 파티클을 먼저(뒤에) 그리고 그 위에(앞에) "NEW RECORD" 글씨를 그려서, 글씨가 항상 또렷하게 읽힌다
	const SpriteInfo& particleSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/Particles.png");
	if (particleSprite.bitmap)
	{
		for (const NewRecordParticle& particle : m_newRecordParticles)
		{
			D2D1_RECT_F sourceRect = PARTICLE_SOURCE_RECTS[particle.sourceRectIndex];
			Vector2 size = {
				(sourceRect.right - sourceRect.left) * Constants::NEW_RECORD_PARTICLE_SCALE,
				(sourceRect.bottom - sourceRect.top) * Constants::NEW_RECORD_PARTICLE_SCALE };

			RenderManager::GetInstance().DrawSpriteRotated(renderTarget, particleSprite, particle.position, size,
				particle.rotation, 0, opacity, &sourceRect);
		}
	}

	const SpriteInfo& newRecordSprite = ResourceManager::GetInstance().GetSpriteInfo("assets/NewRecord.png");
	if (newRecordSprite.bitmap)
	{
		float croppedWidth = NEW_RECORD_TEXT_SOURCE_RECT.right - NEW_RECORD_TEXT_SOURCE_RECT.left;
		float croppedHeight = NEW_RECORD_TEXT_SOURCE_RECT.bottom - NEW_RECORD_TEXT_SOURCE_RECT.top;
		float baseScale = Constants::NEW_RECORD_TEXT_TARGET_WIDTH / croppedWidth;

		Vector2 size = { croppedWidth * baseScale * m_newRecordScale, croppedHeight * baseScale * m_newRecordScale };
		Vector2 center = { Constants::WINDOW_WIDTH / 2.0f, Constants::NEW_RECORD_POPUP_CENTER_Y };

		RenderManager::GetInstance().DrawSpriteRotated(renderTarget, newRecordSprite, center, size, 0.0f, 0, opacity, &NEW_RECORD_TEXT_SOURCE_RECT);
	}
}
