#include "pch.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "RenderManager.h"
#include "../util/Constants.h"
#include "../core/TimeManager.h"
#include "../core/WindowManager.h"
#include "../managers/BlockManager.h"
#include "../managers/CameraManager.h"
#include "../managers/PhysicsManager.h"
#include "../objects/Block.h"

namespace
{
	// COLORREF(0x00BBGGRR)를 D2D1_COLOR_F(0~1 사이 float RGBA)로 바꾼다
	D2D1_COLOR_F ToColorF(COLORREF color)
	{
		return D2D1::ColorF(GetRValue(color) / 255.0f, GetGValue(color) / 255.0f, GetBValue(color) / 255.0f, 1.0f);
	}
}

RenderManager& RenderManager::GetInstance()
{
	static RenderManager instance;
	return instance;
}

IDWriteTextFormat* RenderManager::GetOrCreateTextFormat()
{
	// [성능] 디바이스에 종속되지 않는 리소스라 최초 1회만 만들어서 계속 재사용한다
	if (!m_textFormat)
	{
		WindowManager::GetInstance().GetWriteFactory()->CreateTextFormat(
			L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
			14.0f, L"en-us", m_textFormat.GetAddressOf());
	}
	return m_textFormat.Get();
}

IDWriteTextFormat* RenderManager::GetOrCreateCenteredTextFormat()
{
	if (!m_centeredTextFormat)
	{
		WindowManager::GetInstance().GetWriteFactory()->CreateTextFormat(
			L"Consolas", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
			Constants::HEIGHT_RECORD_FONT_SIZE, L"en-us", m_centeredTextFormat.GetAddressOf());
		m_centeredTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	}
	return m_centeredTextFormat.Get();
}

void RenderManager::DrawSpriteRotated(ID2D1RenderTarget* renderTarget, const SpriteInfo& sprite, Vector2 center, Vector2 size, float angle, int frameIndex, float opacity, const D2D1_RECT_F* sourceRect)
{
	(void)frameIndex;
	if (!sprite.bitmap)
	{
		return;
	}

	// center를 회전 피벗으로 삼아 그 자리에서 회전시킨 뒤, size x size 크기로 그 자리에 그린다.
	// GDI+와 달리 D2D는 "중심점 기준 회전" 행렬을 한 번에 만들 수 있어서 이동->회전->이동을 안 해도 된다
	renderTarget->SetTransform(D2D1::Matrix3x2F::Rotation(angle, D2D1::Point2F(center.x, center.y)));

	D2D1_RECT_F destRect = D2D1::RectF(
		center.x - size.x / 2.0f, center.y - size.y / 2.0f,
		center.x + size.x / 2.0f, center.y + size.y / 2.0f);

	// 픽셀이 흐리게 보이는 것을 해결하기 위해 최근접 이웃 보간 사용 (예전 GDI+ 코드와 동일한 이유).
	// sourceRect가 있으면 원본 비트맵 중 그 영역만 잘라서 destRect에 그린다.
	if (sourceRect != nullptr)
	{
		renderTarget->DrawBitmap(sprite.bitmap.Get(), destRect, opacity, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, *sourceRect);
	}
	else
	{
		renderTarget->DrawBitmap(sprite.bitmap.Get(), destRect, opacity, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
	}

	// 다음 그리기가 이 회전의 영향을 받지 않도록 원상복구
	renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
}

void RenderManager::Render(ID2D1RenderTarget* renderTarget)
{
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> gridBrush;
	renderTarget->CreateSolidColorBrush(ToColorF(Constants::GRID_LINE_COLOR), gridBrush.GetAddressOf());

	// 세로선 그리기: x=0(왼쪽 끝)부터 GRID_WIDTH(오른쪽 끝)까지.
	// 칸이 10개면 칸 사이 경계선은 11개라서 <=를 씀
	for (int x = 0; x <= Constants::GRID_WIDTH_SUBCELLS; ++x)
	{
		float px = x * Constants::SUBCELL_SIZE;
		renderTarget->DrawLine(D2D1::Point2F(px, 0.0f), D2D1::Point2F(px, static_cast<float>(Constants::WINDOW_HEIGHT)), gridBrush.Get());
	}

	// 가로선 그리기: 위와 동일한 방식으로 y=0부터 GRID_HEIGHT_SUBCELLS까지
	for (int y = 0; y <= Constants::GRID_HEIGHT_SUBCELLS; ++y)
	{
		float py = y * Constants::SUBCELL_SIZE;
		renderTarget->DrawLine(D2D1::Point2F(0.0f, py), D2D1::Point2F(static_cast<float>(Constants::WINDOW_WIDTH), py), gridBrush.Get());
	}
}

void RenderManager::DrawFps(ID2D1RenderTarget* renderTarget)
{
	int fps = static_cast<int>(TimeManager::GetInstance().GetFPS());
	std::string fpsText = "FPS: " + std::to_string(fps);
	std::wstring wideFpsText(fpsText.begin(), fpsText.end());

	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush;
	renderTarget->CreateSolidColorBrush(ToColorF(Constants::FPS_TEXT_COLOR), textBrush.GetAddressOf());

	D2D1_RECT_F layoutRect = D2D1::RectF(
		static_cast<float>(Constants::FPS_TEXT_MARGIN), static_cast<float>(Constants::FPS_TEXT_MARGIN),
		static_cast<float>(Constants::WINDOW_WIDTH), static_cast<float>(Constants::FPS_TEXT_MARGIN) + 20.0f);

	renderTarget->DrawText(wideFpsText.c_str(), static_cast<UINT32>(wideFpsText.length()), GetOrCreateTextFormat(), layoutRect, textBrush.Get());
}
void RenderManager::DrawHeightRecord(ID2D1RenderTarget* renderTarget, float heightMeters)
{
	char text[64];
	std::snprintf(text, sizeof(text), "%.1fm", heightMeters);
	std::wstring wideText(text, text + std::strlen(text));

	float panelHeight = Constants::HEIGHT_RECORD_FONT_SIZE + Constants::HEIGHT_RECORD_PANEL_PADDING;
	Vector2 panelCenter = { Constants::WINDOW_WIDTH / 2.0f, static_cast<float>(Constants::FPS_TEXT_MARGIN) + panelHeight / 2.0f };
	FillRect(renderTarget, panelCenter, { Constants::HEIGHT_RECORD_PANEL_WIDTH, panelHeight }, Constants::HEIGHT_RECORD_BACKGROUND_COLOR, Constants::HEIGHT_RECORD_BACKGROUND_OPACITY);

	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush;
	renderTarget->CreateSolidColorBrush(ToColorF(Constants::HEIGHT_RECORD_TEXT_COLOR), textBrush.GetAddressOf());

	// 가로 전체(0~WINDOW_WIDTH)를 레이아웃 영역으로 주고, 가운데 정렬 포맷(GetOrCreateCenteredTextFormat)을
	// 써야 실제로 그 영역 안에서 텍스트가 중앙에 놓인다 — 왼쪽 정렬 포맷에 폭만 넓혀주는 것만으론 안 된다.
	D2D1_RECT_F layoutRect = D2D1::RectF(
		0.0f, static_cast<float>(Constants::FPS_TEXT_MARGIN),
		static_cast<float>(Constants::WINDOW_WIDTH), static_cast<float>(Constants::FPS_TEXT_MARGIN) + panelHeight);

	renderTarget->DrawText(wideText.c_str(), static_cast<UINT32>(wideText.length()), GetOrCreateCenteredTextFormat(), layoutRect, textBrush.Get());
}
void RenderManager::DrawBlockColliders(ID2D1RenderTarget* renderTarget)
{
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> colliderBrush;
	renderTarget->CreateSolidColorBrush(ToColorF(Constants::COLLIDER_DEBUG_COLOR), colliderBrush.GetAddressOf());

	for (Block* block : BlockManager::GetInstance().GetAllBlocks())
	{
		for (int cellIndex = 0; cellIndex < block->GetCellCount(); ++cellIndex)
		{
			// GetCellRotatedCorners가 실제 충돌 판정(SAT)에 쓰는 바로 그 사각형이라, 이걸 그대로 그리면
			// "진짜 콜라이더"가 어디에 있는지 보인다 (스프라이트 렌더링용 회전과는 별개 계산이라 어긋날 수 있음)
			Vector2 corners[4];
			block->GetCellRotatedCorners(cellIndex, corners);

			D2D1_POINT_2F screenPoints[4];
			for (int i = 0; i < 4; ++i)
			{
				Vector2 screenPos = CameraManager::GetInstance().WorldToScreen(corners[i]);
				screenPoints[i] = D2D1::Point2F(screenPos.x, screenPos.y);
			}

			// 네 변을 순서대로 이어 그려서 사각형을 닫는다 (회전된 사각형이라 DrawRectangle은 못 씀)
			for (int i = 0; i < 4; ++i)
			{
				renderTarget->DrawLine(screenPoints[i], screenPoints[(i + 1) % 4], colliderBrush.Get());
			}
		}
	}
}

void RenderManager::DrawSupportDebug(ID2D1RenderTarget* renderTarget)
{
	// [성능] "누가 누구 위에 얹혀 있는지"를 이 프레임에 한 번만 계산해서 모든 블럭이 재사용한다.
	// 블럭마다 새로 계산하면(4-인자 ComputeSupportDebugInfo) 물리 스텝에서 고쳤던 것과 같은 n³ 비용이
	// F1 디버그 오버레이에 그대로 남는다 — 블럭이 서로 가까이 붙어있는(예: 붕괴 중인) 상황일수록 심하다.
	std::unordered_map<Block*, std::vector<Block*>> restingChildren = PhysicsManager::GetInstance().BuildRestingChildrenMap();

	for (Block* block : BlockManager::GetInstance().GetAllBlocks())
	{
		if (block->GetPhysicsState() == PhysicsState::Airborne)
		{
			continue;
		}

		float minX = 0.0f, maxX = 0.0f, combinedComX = 0.0f;
		bool hasSupport = PhysicsManager::GetInstance().ComputeSupportDebugInfo(block, restingChildren, minX, maxX, combinedComX);
		if (!hasSupport)
		{
			continue;
		}

		// ResolveBalance와 같은 IMBALANCE_DEADZONE 여유를 적용해서, 실제로 넘어지지 않을 미세한
		// 오차까지 빨간색으로 표시해 헷갈리는 일이 없게 한다
		bool balanced = combinedComX >= minX - Constants::IMBALANCE_DEADZONE && combinedComX <= maxX + Constants::IMBALANCE_DEADZONE;
		COLORREF color = balanced ? RGB(0, 200, 80) : RGB(255, 0, 0);

		Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
		renderTarget->CreateSolidColorBrush(ToColorF(color), brush.GetAddressOf());

		// 블럭 위쪽 8px 지점에 지지 범위(가로선)를 그린다
		AABB bounds = block->GetWorldBounds();
		float lineY = bounds.min.y - 8.0f;

		Vector2 minScreen = CameraManager::GetInstance().WorldToScreen({ minX, lineY });
		Vector2 maxScreen = CameraManager::GetInstance().WorldToScreen({ maxX, lineY });
		renderTarget->DrawLine(D2D1::Point2F(minScreen.x, minScreen.y), D2D1::Point2F(maxScreen.x, maxScreen.y), brush.Get(), 3.0f);

		// 결합 무게중심 위치에 세로 틱을 그린다
		Vector2 comScreen = CameraManager::GetInstance().WorldToScreen({ combinedComX, lineY });
		renderTarget->DrawLine(D2D1::Point2F(comScreen.x, comScreen.y - 7.0f), D2D1::Point2F(comScreen.x, comScreen.y + 7.0f), brush.Get(), 3.0f);
	}
}

void RenderManager::DrawPhysicsDebugText(ID2D1RenderTarget* renderTarget)
{
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush;
	renderTarget->CreateSolidColorBrush(ToColorF(Constants::PHYSICS_DEBUG_TEXT_COLOR), textBrush.GetAddressOf());
	IDWriteTextFormat* textFormat = GetOrCreateTextFormat();

	for (Block* block : BlockManager::GetInstance().GetAllBlocks())
	{
		const char* stateName = "?";
		switch (block->GetPhysicsState())
		{
		case PhysicsState::Airborne: stateName = "Airborne"; break;
		case PhysicsState::Awake: stateName = "Awake"; break;
		case PhysicsState::Sleeping: stateName = "Sleeping"; break;
		case PhysicsState::Toppling: stateName = "Toppling"; break;
		}

		float speed = std::sqrt(block->GetSpeedSquared());

		char text[128];
		std::snprintf(text, sizeof(text), "%s v=%.0f w=%.0f t=%.2f",
			stateName, speed, block->GetAngularVelocity(), block->GetActiveTimer());
		std::wstring wideText(text, text + std::strlen(text));

		AABB bounds = block->GetWorldBounds();
		Vector2 textScreenPos = CameraManager::GetInstance().WorldToScreen({ bounds.min.x, bounds.min.y - 16.0f });

		D2D1_RECT_F layoutRect = D2D1::RectF(textScreenPos.x, textScreenPos.y, textScreenPos.x + 300.0f, textScreenPos.y + 20.0f);
		renderTarget->DrawText(wideText.c_str(), static_cast<UINT32>(wideText.length()), textFormat, layoutRect, textBrush.Get());
	}
}

void RenderManager::DrawDebugRect(ID2D1RenderTarget* renderTarget, Vector2 center, Vector2 size, COLORREF color)
{
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
	renderTarget->CreateSolidColorBrush(ToColorF(color), brush.GetAddressOf());

	D2D1_RECT_F rect = D2D1::RectF(
		center.x - size.x / 2.0f, center.y - size.y / 2.0f,
		center.x + size.x / 2.0f, center.y + size.y / 2.0f);
	renderTarget->DrawRectangle(rect, brush.Get(), 2.0f);
}

void RenderManager::FillRect(ID2D1RenderTarget* renderTarget, Vector2 center, Vector2 size, COLORREF color, float opacity)
{
	D2D1_COLOR_F colorF = ToColorF(color);
	colorF.a = opacity;

	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
	renderTarget->CreateSolidColorBrush(colorF, brush.GetAddressOf());

	D2D1_RECT_F rect = D2D1::RectF(
		center.x - size.x / 2.0f, center.y - size.y / 2.0f,
		center.x + size.x / 2.0f, center.y + size.y / 2.0f);
	renderTarget->FillRectangle(rect, brush.Get());
}

void RenderManager::DrawCenterOfMass(ID2D1RenderTarget* renderTarget)
{
	constexpr float RADIUS = 5.0f;

	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
	renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), brush.GetAddressOf());

	for (Block* block : BlockManager::GetInstance().GetAllBlocks())
	{
		// 무게중심은 회전 피벗이기도 해서, 회전 여부와 상관없이 항상 이 좌표에 그대로 있다
		// (ResolveRigidCollision이 centerOfMassWorld를 구할 때 쓰는 것과 동일한 계산)
		Vector2 centerOfMassWorld = block->GetRenderPosition() + block->GetCenterOfMassLocal() * Constants::TILE_SIZE;
		Vector2 screenPos = CameraManager::GetInstance().WorldToScreen(centerOfMassWorld);

		renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(screenPos.x, screenPos.y), RADIUS, RADIUS), brush.Get());
	}
}
