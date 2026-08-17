#pragma once

#include "../util/Types.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <vector>
// 화면에 실제로 그리는 역할만 담당하는 싱글톤.
// 다른 매니저의 상태를 읽기만 하며, 게임 로직을 갖지 않는다.
class RenderManager
{
public:
	static RenderManager& GetInstance();
	/// @brief 스프라이트를 지정한 중심점 기준으로 회전시켜 그린다.
	/// @param renderTarget 그릴 대상 Direct2D 렌더타겟
	/// @param sprite 그릴 스프라이트 정보
	/// @param center 회전 피벗이자 스프라이트의 중심이 위치할 좌표 (좌상단이 아님 — 회전이 있는 블럭은
	///        반드시 무게중심 기준으로 이미 회전된 칸 중심(Block::GetCellCenterRotated)을 넘겨야 콜라이더와 일치한다)
	/// @param angle 회전 각도(도 단위)
	/// @param  size 그릴 가로/세로	크기
	/// @param opacity 불투명도(0~1). 1보다 작으면 반투명하게 그려진다 — 비활성화된 버튼처럼 흐리게
	///        보여주고 싶을 때 쓴다.
	/// @param sourceRect 원본 비트맵에서 잘라 쓸 영역(픽셀 좌표). nullptr이면 비트맵 전체를 쓴다 —
	///        한 이미지에 여러 그림이 들어있을 때(예: 버튼 3개가 세로로 이어진 스프라이트시트) 일부만
	///        그리고 싶을 때 쓴다.
	void DrawSpriteRotated(ID2D1RenderTarget* renderTarget, const SpriteInfo& sprite, Vector2 center, Vector2 size, float angle, int frameIndex, float opacity = 1.0f, const D2D1_RECT_F* sourceRect = nullptr);

	/// @brief 현재 프레임의 화면을 그린다 (지금은 격자선만).
	/// @param renderTarget WM_PAINT에서 얻은 Direct2D 렌더타겟
	void Render(ID2D1RenderTarget* renderTarget);

	/// @brief 좌상단에 현재 FPS를 텍스트로 표시한다 (디버그용). 다른 내용 위에 덮이지 않도록 항상 맨 마지막에 그려야 한다.
	/// @param renderTarget WM_PAINT에서 얻은 Direct2D 렌더타겟
	void DrawFps(ID2D1RenderTarget* renderTarget);
	/// @brief [임시 디버그] 현재 탑 높이(m)와 CloudManager가 들고 있는 구름 개수를 좌상단에 표시한다.
	///        구름이 안 보일 때 "애초에 안 만들어졌는지"와 "만들어졌는데 안 그려지는지"를 구분하기 위한 용도.
	/// @param renderTarget WM_PAINT에서 얻은 Direct2D 렌더타겟
	void DrawCloudDebugText(ID2D1RenderTarget* renderTarget, float heightMeters, int cloudCount);
	void DrawHeightRecord(ID2D1RenderTarget* renderTarget, float heightMeters);
	void DrawScorePanel(ID2D1RenderTarget* renderTarget, Vector2 center, float heightMeters); \
	void DrawNicknameInputPanel(ID2D1RenderTarget* renderTarget, Vector2 center, Vector2 panelSize, const std::wstring& currentInput);
	/// @brief 랭킹 목록을 Ranking.png 위 NAME/SCORE 빈 칸 각각에 겹쳐 그린다. nameCellCenters/scoreCellCenters는
	/// 화면 좌표로 이미 변환된 각 줄의 칸 중심(TitleScene::GetRankingCellCenter로 계산)이고, rankings[i]가
	/// 그 i번째 칸에 대응한다 — 세 벡터 중 가장 짧은 길이만큼만 그린다.
	void DrawRankingList(ID2D1RenderTarget* renderTarget, const std::vector<RankingEntry>& rankings,
		const std::vector<Vector2>& nameCellCenters, const std::vector<Vector2>& scoreCellCenters,
		Vector2 nameCellSize, Vector2 scoreCellSize);
	/// @brief 모든 블럭의 칸(cell)마다 실제 충돌 판정에 쓰이는 회전된 사각형(콜라이더)을 빨간 테두리로 그린다 (디버그용).
	/// @param renderTarget WM_PAINT에서 얻은 Direct2D 렌더타겟
	void DrawBlockColliders(ID2D1RenderTarget* renderTarget);

	/// @brief 착지한 블럭마다 지지 범위(가로선)와 결합 무게중심(세로 틱)을 그린다 (디버그용).
	/// 무게중심이 지지 범위 안이면 초록, 벗어났으면 빨강 — "왜 안 넘어지는지"를 코드 안 보고 눈으로 바로 확인용
	/// @param renderTarget WM_PAINT에서 얻은 Direct2D 렌더타겟
	void DrawSupportDebug(ID2D1RenderTarget* renderTarget);

	/// @brief 블럭마다 물리 상태(Airborne/Awake/Sleeping/Toppling), 선속도, 각속도, 강제취침 타이머(activeTimer)를
	/// 텍스트로 표시한다 (디버그용). 물리 리팩토링 중 Sleep/Wake 진동이나 지지 소실 문제를 코드 안 보고 눈으로 확인용
	/// @param renderTarget WM_PAINT에서 얻은 Direct2D 렌더타겟
	void DrawPhysicsDebugText(ID2D1RenderTarget* renderTarget);

	/// @brief 블럭마다 무게중심(회전 피벗이기도 함)을 빨간 원으로 표시한다 (디버그용).
	/// @param renderTarget WM_PAINT에서 얻은 Direct2D 렌더타겟
	void DrawCenterOfMass(ID2D1RenderTarget* renderTarget);

	/// @brief center를 중심으로 하는 축 정렬 사각형 테두리를 그린다 (디버그용). 회전이 필요 없는
	/// UI 클릭 판정 영역(예: 타이틀 화면 버튼 히트박스) 등을 눈으로 확인할 때 재사용한다.
	/// @param renderTarget WM_PAINT에서 얻은 Direct2D 렌더타겟
	void DrawDebugRect(ID2D1RenderTarget* renderTarget, Vector2 center, Vector2 size, COLORREF color);

	/// @brief center를 중심으로 하는 축 정렬 사각형을 반투명하게 채워 그린다. 디버그용이 아니라 실제
	/// 게임 화면 요소(예: 데스존 표시)를 그릴 때 쓴다 — F1과 무관하게 항상 그려진다.
	/// @param renderTarget WM_PAINT에서 얻은 Direct2D 렌더타겟
	/// @param opacity 불투명도(0~1)
	void FillRect(ID2D1RenderTarget* renderTarget, Vector2 center, Vector2 size, COLORREF color, float opacity);

	/// @brief points를 순서대로 이어 만든 다각형을 채워 그린다. 사각형이 아닌 UI 모양(예: 스피커 아이콘의
	/// 세모 콘)을 그릴 때 쓴다.
	/// @param points 다각형 꼭짓점(화면 좌표) 배열, 최소 3개
	/// @param pointCount points 배열의 꼭짓점 개수
	void FillPolygon(ID2D1RenderTarget* renderTarget, const Vector2* points, int pointCount, COLORREF color, float opacity = 1.0f);

private:
	RenderManager() = default;
	~RenderManager() = default;
	RenderManager(const RenderManager&) = delete;
	RenderManager& operator=(const RenderManager&) = delete;

	// [성능/편의] 디버그 텍스트(FPS, 물리 상태)에 쓰는 IDWriteTextFormat은 디바이스에 종속되지 않아
	// 최초 1회만 만들어서 재사용한다.
	IDWriteTextFormat* GetOrCreateTextFormat();

	// 가운데 정렬이 필요한 텍스트(최고 높이 기록 등)용. GetOrCreateTextFormat()과 폰트는 같지만
	// 정렬만 다르다 — 하나를 공유하면 FPS(왼쪽 정렬)까지 같이 가운데로 밀려버려서 따로 둔다.
	IDWriteTextFormat* GetOrCreateCenteredTextFormat();

	Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormat;
	Microsoft::WRL::ComPtr<IDWriteTextFormat> m_centeredTextFormat;
};
