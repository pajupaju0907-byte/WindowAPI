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

 
    bool CanOccupy(int originGridX, int originGridY) const;
    void MarkOccupiedCells() const;

    // 공중 상태 전용: 그리드(서브셀) 기준 이동. 물리 연산 없이 좌표만 스텝 이동한다.
    void MoveHorizontal(int subCellDelta);

    // 한 칸 아래로 이동을 시도한다. GridManager 점유 칸을 확인해서
    // 이동했으면 true, 바닥/다른 블럭에 막혀 착지해야 하면 false를 반환한다.
    bool StepDown();

    //부호로 방향을 받아 모양을 90도 회전시키되, 회전한 모양이 안 들어가면 취소한다
    void Rotate(int direction);
    // 바닥 상태 전용: 착지 이후의 물리 연산(흔들림 등). Awake 상태에서만 호출되어야 한다.
    void ApplyForce(Vector2 force);
    void Integrate(float deltaTime);
    void WakeUp();

    // Airborne -> Awake 전환. 착지 시 BlockManager가 호출해 그리드 좌표를 월드 좌표(m_position)로
    // 확정하고 물리 시뮬레이션을 시작시키는 지점.
    void Land();

    // 스폰 직후 BlockManager가 초기 그리드 좌표를 지정할 때 사용
    void SetGridPosition(int gridX, int gridY);

    // 현재 물리 상태에 맞는 렌더링용 월드 좌표 (Airborne이면 그리드 좌표를 픽셀로 환산, 아니면 m_position)
    Vector2 GetRenderPosition() const;
    int GetCellCount() const;
    Vector2 GetCellRenderPosition(int cellIndex) const;
   
    // 이 블럭이 차지하는 칸 전체를 감싸는 사각형(월드/픽셀 좌표). 블럭끼리 충돌 판정에 씀
    AABB GetWorldBounds() const;
    const std::string& GetSpriteId() const;

    // 질량 상관없이 같은 중력 가속도를 주려면, 힘을 걸 때 질량을 곱해줘야 해서 필요 (F=ma에서 a를 고정하려는 것)
    float GetMass() const;

    // 속도의 제곱크기(sqrt 안 써서 가볍게). 안정성 판정에서 임계값이랑 비교할 때 씀
    float GetSpeedSquared() const;

    // PhysicsManager가 Awake 상태 블럭만 골라내려고 씀
    PhysicsState GetPhysicsState() const;


    // 아래로 파고든 만큼(penetration) 위로 밀어내고 낙하를 멈춘다. 바닥/다른 블럭 양쪽에 다 씀
    void ResolveVerticalPenetration(float penetration);

protected:
    // unique_ptr<Collider>가 불완전 타입을 가리키므로, 기본 생성자는 반드시 .cpp(Collider가 완전한 타입인 곳)에서 정의한다.
    Block();

    int m_id = 0;

    // 블럭을 구성하는 칸들의 원점 기준 상대 좌표 (테트리스 칸 단위, 예: O자는 (0,0)(1,0)(0,1)(1,1)).
    // 이동/충돌(StepDown, MoveHorizontal)과 렌더링이 여기를 순회하므로, 파생 클래스 생성자에서 반드시 채워야 한다.
    static constexpr int CELL_COUNT = 4;
    Vector2 m_cellShape[CELL_COUNT];
    Vector2 m_pivot;
    bool m_canRotate = true;
    // 공중 상태(Airborne)에서 쓰는 그리드 좌표. 단위는 서브셀(1 서브셀 = 0.5 테트리스 칸).
    int m_gridX = 0;
    int m_gridY = 0;

    // 바닥 상태(Land() 호출 이후)에서 쓰는 월드 좌표/속도. 그 전에는 의미 없는 값이다.
    Vector2 m_position;
    Vector2 m_velocity;
    Vector2 m_accumulatedForce;
    float m_angle = 0.0f;
    float m_angularVelocity = 0.0f;
    float m_mass = 1.0f;
    PhysicsState m_physicsState = PhysicsState::Airborne;
    std::unique_ptr<Collider> m_collider;
    std::string m_spriteId;
};
