#pragma once

#include "../util/Constants.h"
#include "../util/Types.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

class Block;

// 바닥 상태(Awake)인 블럭들의 물리 갱신과 전체 안정성 판단을 담당하는 싱글톤.
// 공중 상태(Airborne) 블럭의 낙하는 그리드 스텝 방식이라 BlockManager가 처리하며, 여기서는 다루지 않는다.
class PhysicsManager
{
public:
    static PhysicsManager& GetInstance();

    // BlockManager가 가진 블럭 전체를 순회하며 Awake 상태만 골라 ApplyGravity + Integrate 호출
    void Update(float deltaTime);

    // 바닥 상태에서의 흔들림 표현을 위한 중력 적용. 공중 낙하 속도와는 무관하다.
    void ApplyGravity(Block* block);

    // block이 차지하는 칸들 중 바닥을 가장 깊이 파고든 정도를 찾아서, 파고들었으면 밀어냄
    void ResolveFloorCollision(Block* block);

    // [책임 분리] "겹쳤는지/어디를/얼마나 겹쳤는지" 판정은 CollisionManager::DetectPairCollision이 전담한다.
    // 여기서는 그 결과를 가지고 "그래서 어떻게 반응할지"(WakeUp 여부, 임펄스 적용)만 담당한다.
    // 둘 다 Awake면 쌍방향(ResolveRigidCollisionWithBlock), 한쪽만 Awake면 Sleeping인 쪽을
    // 바닥처럼 고정된 것으로 취급(ResolveRigidCollision)한다.
    void ResolveBlockPairCollision(Block* block, Block* other);
    // 존재하는 모든 블럭 쌍(중복 없이 한 번씩)에 대해 ResolveBlockPairCollision을 돌림
    void ResolveBlockCollisions();

    // cellPosition(월드 좌표, TILE_SIZE 정사각형 기준)이 바닥이나 self가 아닌 다른 블럭 위에 "안정적으로"
    // 얹혀 있는지 판정한다(칸 중심이 실제로 그 위에 있어야 함 — 엄격). Sleep 가능 여부/붕괴 판정처럼
    // "여기서 잠들어도/버텨도 되는가"를 결정하는 곳에서만 쓴다. GetCellSupportRange의 지지 여부만 쓰는
    // 얇은 래퍼 — 지지 범위(겹치는 폭)까지 필요하면 GetCellSupportRange를 직접 쓸 것.
    bool IsCellSupported(Block* block, int cellIndex) const;

    // [경계값 버그 방지 — 실전 확인됨] IsCellSupported와 같은 기준으로 지지 여부를 판정하되, 지지된다면
    // 그 칸의 전체 폭이 아니라 "바닥/아래 블록과 실제로 겹치는 부분"만 outSupportMinX/MaxX로 돌려준다.
    // 칸이 발판이나 아래 블록의 가장자리에 절반쯤만 걸쳤을 때(칸 중심은 지지 쪽에 있지만 칸 자체는 절반이
    // 허공 위) 칸의 전체 폭을 지지 범위에 다 넣으면, 실제보다 지지 기반이 넓게(허공 쪽으로) 잡혀서 무게중심이
    // 그 안에 들어와버려 명백히 넘어져야 할 상황에서도 안 넘어지는 원인이 된다 — 4칸 중 3칸이 기둥으로
    // 곧게 서 있고 발만 옆으로 삐져나온 모양이 착지 위치에 따라 이렇게 잘못 안정 판정되는 걸 확인했다.
    // ComputeSupportDebugInfo(진짜 지지 범위/무게중심 비교가 필요한 곳)만 이 버전을 쓴다.
    bool GetCellSupportRange(Block* block, int cellIndex, float& outSupportMinX, float& outSupportMaxX) const;

    // [마찰 사각지대 방지] IsCellSupported보다 느슨하게, 칸이 바닥/다른 블럭과 조금이라도 겹치면 true를
    // 반환한다. 회전한 칸이 모서리로만 걸친 경우(IsCellSupported는 false) 이 함수는 true가 되는데, 이게
    // 필요한 이유: 마찰 감쇠(Step 2.5)를 IsCellSupported 기준으로만 걸면, "모서리로만 걸쳐서 중심은 안
    // 겹치는" 블록은 마찰도 안 받고 안정 판정도 못 받는 사각지대에 빠져 중력에 살짝 밀렸다가 충돌에 다시
    // 튕기는 게 감쇠 없이 반복되며 영원히 떨린다. 마찰은 이 느슨한 기준으로 걸어서 떨림 자체를 죽이고,
    // "진짜 안정적인지/넘어질지"는 여전히 IsCellSupported(엄격)로 따로 판단한다.
    bool IsCellTouchingAnySupport(Block* block, int cellIndex) const;
    // [무게중심 실제 토크] block의 무게중심(x)이 지지된 칸들의 가로 범위를 벗어났으면
    // Block::ApplyGravityTorque로 pivot(지지 가장자리) 기준 진짜 중력 토크를 매 프레임 걸어준다.
    // 기울어진 각도가 MAX_TOPPLE_ANGLE을 넘어서야 실제로 BeginToppling() 상태 전환이 일어나고,
    // 균형 잡힌 경우엔 아무것도 안 한다.
    // [성능] childrenOf는 이번 물리 스텝에서 BuildRestingChildrenMap()으로 한 번만 계산해서 매 블럭 호출에
    // 재사용하는 "누가 누구 위에 얹혀 있는지" 맵이다 — 매번 새로 스캔하지 않기 위한 것.
    // [지지대 우선 붕괴] 반환값은 "이번 프레임에 이 블록이 불균형으로 판정돼 처리(토크 적용/포기 등)됐는지"
    // — Step()이 true를 받으면 이 블록 위에 얹힌 모든 블록(재귀)을 이번 프레임엔 각자 따로 불균형 검사를
    // 안 하도록 건너뛴다. "지지대가 먼저 무너지는 게 맞다"는 설계 결정에 따른 것 — 아래(더 근본적인 지지대)가
    // 이미 불안정을 처리 중이면, 그 위에 얹힌 것들은 자기 좁은 접촉면만 보고 따로 넘어지지 않고 아래의
    // 붕괴에 묻어간다. 아래가 실제로 넘어지면(Toppling) 다음 프레임에 지지를 잃었다고 자연히 감지해서 반응한다.
    bool ResolveBalance(Block* block, float deltaTime, const std::unordered_map<Block*, std::vector<Block*>>& childrenOf);

    // Toppling 상태인 블럭 중 화면 아래로 완전히 벗어난 것들을 BlockManager에서 제거
    void RemoveToppledBlocks();

    // Toppling 상태인 블럭 중 화면 밖으로 떨어지지 않고, 결합 무게중심이 실제 지지 범위 안으로
    // 들어온 것들만 Awake로 전환한다("칸 하나라도 닿음"만 보고 돌려보내면, 회전하며 넘어지는 도중
    // 다른 모서리가 잠깐 스치기만 해도 매번 Awake로 튕겨나갔다가 다음 스텝에 ResolveBalance가 다시
    // 넘어뜨리는 걸 반복해서 — 진짜 넘어지는 중인 블록의 낙하가 계속 끊기고, 결국 재넘어짐 제한에
    // 걸려 넘어져야 할 자세 그대로 얼어붙었다). childrenOf는 이 스텝에서 이미 계산된 것을 그대로 받아
    // 재사용한다(성능, ResolveBalance와 같은 이유). Sleeping으로 곧바로 재우진 않는다 — 그건 TrySleepAll의
    // rest timer가 판단한다.
    void SettleToppledBlocks(const std::unordered_map<Block*, std::vector<Block*>>& childrenOf);

    // 개별 블럭이 아니라 탑 전체가 안정적인지 판단 (전체 판단은 매니저의 책임)
    bool CheckGlobalStability() const;

    void TrySleepAll(float deltaTime);
	

    // [디버그 시각화] block 자신의 지지 범위(outMinX/outMaxX)와, block이 떠받치는 전체 덩어리의 결합
    // 무게중심(outCombinedComX)을 계산해서 그대로 돌려준다 — ResolveBalance와 완전히 같은 계산이라,
    // F1 디버그 오버레이가 "지금 이 블럭이 안정적이라고 판단된 이유"를 화면에 그대로 그릴 수 있게 해준다.
    // 지지대가 없으면 false를 반환(이 경우 outMinX/outMaxX/outCombinedComX는 의미 없음)
    bool ComputeSupportDebugInfo(Block* block, float& outMinX, float& outMaxX, float& outCombinedComX) const;

    // [디버그 시각화 성능] 위 ComputeSupportDebugInfo를 여러 블럭에 대해 연달아 부를 거라면(예: F1
    // 오버레이가 화면의 모든 블럭을 순회), 이 버전으로 childrenOf를 한 번만 만들어 넘겨 재사용해야 한다.
    // 4-인자 버전을 블럭마다 부르면 그때마다 BuildRestingChildrenMap()을 처음부터 다시 계산해서
    // 물리 스텝에서 고쳤던 것과 같은 n³ 비용이 디버그 오버레이 경로에 그대로 남는다.
    bool ComputeSupportDebugInfo(Block* block, const std::unordered_map<Block*, std::vector<Block*>>& childrenOf, float& outMinX, float& outMaxX, float& outCombinedComX) const;

    // [성능] "누가 누구 위에 얹혀 있는지"를 전체 블럭 쌍을 훑어서 한 번에 계산해둔다. 물리 스텝(Step)뿐
    // 아니라 디버그 오버레이(RenderManager::DrawSupportDebug)도 이걸 한 번만 만들어서 재사용해야 한다.
    std::unordered_map<Block*, std::vector<Block*>> BuildRestingChildrenMap() const;

private:
    PhysicsManager() = default;
    ~PhysicsManager();
    PhysicsManager(const PhysicsManager&) = delete;
    PhysicsManager& operator=(const PhysicsManager&) = delete;
    // [지지 판정 통합] cellIndex번 칸의 회전된 네 꼭짓점(GetCellRotatedCorners)에서 "가장 아래 Y"와
    // "가로 범위(min~max X)"를 뽑아준다. 회전이 없으면 기존 cellBottomY/cellCenterX 계산과 똑같이 나오고,
    // 기울어진 블럭이면 실제 기운 모양대로 나온다 — IsCellSupported/RestsOnBlock/ComputeSupportDebugInfo가
    // 전부 이 함수 하나로 지지 판정용 좌표를 얻게 해서, 세 곳이 서로 다른 좌표계를 보는 문제를 없앤다.
    void GetCellSupportBounds(Block* block, int cellIndex, float& outTopY, float& outBottomY, float& outMinX, float& outMaxX) const;
    // upper의 칸 중 하나라도 lower 위에 얹혀 있는지(IsCellSupported와 같은 판정을, 특정 블록 한 쌍으로 좁혀서) 검사
    bool RestsOnBlock(Block* upper, Block* lower) const;

    // [고정 timestep] 예전 Update()의 본문 전체 — 항상 PHYSICS_FIXED_TIMESTEP 크기로만 호출된다.
    // Update()가 렌더 프레임의 가변 deltaTime을 여기로 몇 번 나눠서 넘길지 결정한다.
    void Step(float deltaTime);

    // [고정 timestep] 프레임 간 남은 시간을 누적해뒀다가, PHYSICS_FIXED_TIMESTEP만큼씩 Step()에 넘긴다.
    float m_accumulator = 0.0f;

    // [연쇄 붕괴] base 위에 (직접 또는 다른 블럭을 거쳐 간접적으로) 얹힌 모든 블럭을 재귀로 훑어서,
    // base를 포함한 전체의 질량 합과 무게중심(x) 가중합을 누적한다. ResolveBalance가 "이 블럭 자신"이 아니라
    // "이 블럭이 떠받치고 있는 전체 무더기"의 무게중심으로 판정하게 하려고 필요하다 —
    // 안 그러면 위에 삐딱하게 얹힌 블럭만 넘어지고 밑을 받치는 블럭은 자기 발판만 보고 멀쩡하다고 착각한다.
    void AccumulateSupportedMass(Block* base, std::vector<Block*>& visited, const std::unordered_map<Block*, std::vector<Block*>>& childrenOf, float& outTotalMass, float& outWeightedX) const;

    // [지지대 우선 붕괴] base가 이번 프레임에 불균형으로 처리됐을 때(ResolveBalance가 true를 반환),
    // base 위에 (직접/간접으로) 얹힌 모든 블록을 재귀로 찾아서 outDeferred에 추가한다. Step()이 이
    // 집합에 있는 블록은 이번 프레임 자기 몫의 개별 ResolveBalance 호출을 건너뛴다 — "지지대가 먼저
    // 무너지는 게 맞다"는 설계 결정에 따라, 아래(더 근본적인 지지대)가 이미 불안정을 처리 중이면 위에
    // 얹힌 것들은 자기 좁은 접촉면만 보고 따로 넘어지지 않고 아래의 붕괴에 묻어가게 한다.
    void MarkDescendantsDeferred(Block* base, const std::unordered_map<Block*, std::vector<Block*>>& childrenOf, std::unordered_set<Block*>& outDeferred) const;
};
