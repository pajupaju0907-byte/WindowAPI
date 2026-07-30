#pragma once

// 모든 씬(타이틀/플레이/게임오버)이 공통으로 구현해야 하는 인터페이스
class Scene
{
public:
    virtual ~Scene() = default;

    // 씬 진입 시 1회 호출 (리소스 준비, 상태 초기화 등)
    virtual void Enter() = 0;

    // 씬 이탈 시 1회 호출 (정리 작업)
    virtual void Exit() = 0;

    // 매 프레임 호출되는 갱신 로직
    virtual void Update(float deltaTime) = 0;

    // 매 프레임 호출되는 렌더링 로직
    // hdc는 WM_PAINT에서 얻은 디바이스 컨텍스트를 그대로 받아 RenderManager에 전달하기 위함
    virtual void Render(HDC hdc) = 0;
};
