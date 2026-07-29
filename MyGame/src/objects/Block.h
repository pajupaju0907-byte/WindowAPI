#pragma once

#include <memory>
#include <string>
#include "../util/Types.h"

class Collider;

// 모든 블럭 종류(테트로미노/원형/거대/무거운 블럭)의 공통 베이스.
// 물리 적분(Integrate)과 힘 적용(ApplyForce)은 블럭 종류와 무관하게 동일하므로 여기서 구현하고,
// 모양/크기 등 데이터만 다른 부분은 파생 클래스의 필드로 처리한다.
//
// 공중 상태(Airborne)와 바닥 상태(Awake/Sleeping)는 서로 다른 이동 방식을 쓴다:
// - 공중 상태: 물리 없이 그리드(서브셀) 좌표를 스텝 이동 (MoveHorizontal/StepDown)
// - 바닥 상태: Land()로 그리드 좌표를 월드 좌표로 확정한 뒤, 물리 적분(ApplyForce/Integrate)으로 이동
class Block
{
public:
    virtual ~Block();

    // 공중 상태 전용: 그리드(서브셀) 기준 이동. 물리 연산 없이 좌표만 스텝 이동한다.
    void MoveHorizontal(int subCellDelta);
    void StepDown();

    // 바닥 상태 전용: 착지 이후의 물리 연산(흔들림 등). Awake 상태에서만 호출되어야 한다.
    void ApplyForce(Vector2 force);
    void Integrate(float deltaTime);
    void WakeUp();

    // Airborne -> Awake 전환. 착지 시 BlockManager가 호출해 그리드 좌표를 월드 좌표(m_position)로
    // 확정하고 물리 시뮬레이션을 시작시키는 지점.
    void Land();

protected:
    // unique_ptr<Collider>가 불완전 타입을 가리키므로, 기본 생성자는 반드시 .cpp(Collider가 완전한 타입인 곳)에서 정의한다.
    Block();

    int m_id = 0;

    // 공중 상태(Airborne)에서 쓰는 그리드 좌표. 단위는 서브셀(1 서브셀 = 0.5 테트리스 칸).
    int m_gridX = 0;
    int m_gridY = 0;

    // 바닥 상태(Land() 호출 이후)에서 쓰는 월드 좌표/속도. 그 전에는 의미 없는 값이다.
    Vector2 m_position;
    Vector2 m_velocity;
    float m_angle = 0.0f;
    float m_angularVelocity = 0.0f;
    float m_mass = 1.0f;
    PhysicsState m_physicsState = PhysicsState::Airborne;
    std::unique_ptr<Collider> m_collider;
    std::string m_spriteId;
};
