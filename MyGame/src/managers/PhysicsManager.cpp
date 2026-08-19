#include "pch.h"

#include "PhysicsManager.h"
#include "../objects/Block.h"
#include "../managers/BlockManager.h"
#include "../managers/CollisionManager.h"
#include "../managers/SoundManager.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <sstream>

namespace
{
	// [경사면 미끄러짐 허용] 이 블럭의 각도가 90도 배수(0/90/180/270...)에서 SUPPORT_SURFACE_MAX_TILT_DEGREES
	// 이내인지 — 즉 윗면이 실제로 평평한 가로 선반 역할을 할 수 있는 각도인지 본다. Constants.h의
	// SUPPORT_SURFACE_MAX_TILT_DEGREES 주석 참고.
	bool IsNearAxisAlignedAngle(float angleDegrees)
	{
		float normalized = std::fmod(angleDegrees, 90.0f);
		if (normalized < 0.0f)
		{
			normalized += 90.0f;
		}
		float distanceFromAxis = (std::min)(normalized, 90.0f - normalized);
		return distanceFromAxis <= Constants::SUPPORT_SURFACE_MAX_TILT_DEGREES;
	}

	// [임시 디버그 로깅 — 지지 없이 얼어붙는 버그 재발 추적용] Sleep/Wake/ForceStabilize 전환이 일어날
	// 때마다 어느 블록이 왜 그렇게 됐는지 physics_debug.log에 남긴다. 실행마다 이전 로그를 지우고 새로
	// 시작한다(디스크에 남아있던 예전 로그와 섞이지 않도록). 버그가 재현 안 되는 채로 며칠 확인되면
	// 이 로깅 블록(g_stepCounter/LogFreezeDebug/DescribeBlock 및 호출부)은 지워도 된다.
	int g_stepCounter = 0;

	void LogFreezeDebug(const std::string& message)
	{
		static bool isFirstWrite = true;
		std::ofstream log("physics_debug.log", isFirstWrite ? std::ios::trunc : std::ios::app);
		isFirstWrite = false;
		if (log.is_open())
		{
			log << "[step " << g_stepCounter << "] " << message << "\n";
		}
	}

	std::string DescribeBlock(Block* block)
	{
		Vector2 pos = block->GetRenderPosition();
		std::ostringstream oss;
		oss << "block=" << block
			<< " pos=(" << pos.x << "," << pos.y << ")"
			<< " v=" << std::sqrt(block->GetSpeedSquared())
			<< " w=" << block->GetAngularVelocity()
			<< " angle=" << block->GetAngle();
		return oss.str();
	}
}

PhysicsManager& PhysicsManager::GetInstance()
{
    static PhysicsManager instance;
    return instance;
}

void PhysicsManager::LogDebug(const std::string& message)
{
    LogFreezeDebug(message);
}

PhysicsManager::~PhysicsManager() = default;

void PhysicsManager::Update(float deltaTime)
{
    // [고정 timestep] 렌더 프레임이 밀려서 deltaTime이 순간적으로 커져도(디버거로 멈췄다 풀거나 랙),
    // 그걸 그대로 누적하면 다음 프레임에 수십 스텝을 몰아서 처리하려다 오히려 더 느려진다
    // ("Spiral of Death") — 그래서 누적 전에 한 프레임 최대치를 잘라낸다.
    float clampedDeltaTime = std::min(deltaTime, Constants::PHYSICS_FIXED_TIMESTEP * Constants::PHYSICS_MAX_SUBSTEPS);
    m_accumulator += clampedDeltaTime;

    int substeps = 0;
    while (m_accumulator >= Constants::PHYSICS_FIXED_TIMESTEP && substeps < Constants::PHYSICS_MAX_SUBSTEPS)
    {
        Step(Constants::PHYSICS_FIXED_TIMESTEP);
        m_accumulator -= Constants::PHYSICS_FIXED_TIMESTEP;
        ++substeps;
    }
}

void PhysicsManager::Step(float deltaTime)
{
    ++g_stepCounter;

    // 1. 상태 업데이트 및 중력 적용
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        bool isAwake = block->GetPhysicsState() == PhysicsState::Awake;
        bool isToppling = block->GetPhysicsState() == PhysicsState::Toppling;

        if (!isAwake && !isToppling) continue;

        block->AdvanceActiveTimer(deltaTime);
        block->AdvanceWakeTimer(deltaTime);
        ApplyGravity(block);
        block->Integrate(deltaTime);
    }

    // 2. 충돌 해결 (Solver Iterations)
    // [좌우 편향 방지] 접촉점 처리 순서(왼쪽 먼저/오른쪽 먼저)를 이번 프레임의 4회 반복 전체에 걸쳐
    // 통일하고, 그 방향을 프레임(Step 호출)마다 뒤집는다 — 이유는 m_swapContactOrderThisStep 선언부
    // (PhysicsManager.h) 주석 참고. 반복 안에서 매번 뒤집으면 뒤쪽 반복의 작은 편향이 앞쪽 반복의 큰
    // 편향을 다 못 지운다.
    m_swapContactOrderThisStep = !m_swapContactOrderThisStep;
    m_collisionSfxPlayedThisStep = false;
    for (int iteration = 0; iteration < Constants::COLLISION_SOLVER_ITERATIONS; ++iteration)
    {
        for (Block* block : BlockManager::GetInstance().GetAllBlocks())
        {
            if (block->GetPhysicsState() == PhysicsState::Awake || block->GetPhysicsState() == PhysicsState::Toppling)
            {
                ResolveFloorCollision(block, m_swapContactOrderThisStep);
            }
        }
        ResolveBlockCollisions(m_swapContactOrderThisStep);
    }

    // 2.5. [바닥 마찰 근사] 지지대가 있는 Awake 블럭은 매 프레임 속도를 감쇠시켜서 미끄러짐을 잡는다.
    // ResolveRigidCollision의 임펄스 마찰은 얹혀서 미끄러지는 동안(수직 속도 거의 0)엔 안 걸리기 때문에 필요하다.
    // [넘어지는 중엔 제외] Toppling은 일부러 뺐다 — 넘어지는 도중엔 피벗 모서리가 거의 계속 바닥/다른
    // 블럭에 닿아 있어서(hasSupport=true) 이 감쇠가 매초 60번씩 각속도를 깎았다. "가만히 얹힌 블록이
    // 안 미끄러지게" 하려고 만든 감쇠인데, 지금 활발히 회전하며 넘어지는 블록에도 똑같이 걸려서 초기
    // 회전력이 거의 즉시 죽어버리고, 재넘어짐마다 그 죽은 속도에서 다시 시작해 넘어짐이 부자연스럽게
    // 느려 보이는 원인이었다. 넘어지는 중엔 실제 중력+충돌 임펄스만으로 가속되게 둔다.
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        bool isActive = block->GetPhysicsState() == PhysicsState::Awake;
        if (!isActive)
        {
            continue;
        }

        // [마찰 사각지대 방지] 여기는 일부러 엄격한 IsCellSupported 대신 느슨한 IsCellTouchingAnySupport를
        // 쓴다 — 모서리로만 살짝 걸친 상태도 마찰을 받아야, 중력에 살짝 밀렸다가 충돌에 다시 튕기는 게
        // 감쇠 없이 반복되며 영원히 떠는 사각지대가 안 생긴다. "진짜 안정적인지"는 아래 ResolveBalance가
        // IsCellSupported(엄격)로 따로 판단하니, 여기서 느슨하게 봐준다고 넘어져야 할 게 안 넘어지진 않는다.
        bool isTouchingAnySupport = false;
        for (int i = 0; i < block->GetCellCount() && !isTouchingAnySupport; ++i)
        {
            if (IsCellTouchingAnySupport(block, i))
            {
                isTouchingAnySupport = true;
            }
        }

        if (!isTouchingAnySupport)
        {
            continue;
        }

        // [불균형 중에도 선속도 감쇠는 유지 — "무게중심이 불안정하면 미끄러진다" 버그 수정] 충돌 임펄스
        // 기반 마찰(ResolveRigidCollision)은 그 순간의 수직 임펄스 크기에 비례해서 clamp되는데, 가만히
        // 얹힌 채 기우는 중엔 수직 임펄스가 거의 0이라 마찰도 거의 안 걸린다. 예전엔 이 선속도 감쇠까지
        // 각속도 감쇠와 함께 불균형 중엔 통째로 건너뛰어서, 무게중심이 지지 범위를 벗어난 순간 옆으로
        // 잡아줄 게 없어 미끄러지는 원인이 됐다. 선속도 감쇠는 회전(넘어짐)과 무관하므로 늘 걸어둔다.
        block->DampVelocity(Constants::GROUNDED_VELOCITY_DAMPING);

        // [넘어지기 직전 단계는 각속도 감쇠만 제외 — 실전 로그로 확인된 "40도를 절대 못 넘고 그 자리에서
        // 멈춤" 버그 수정] ResolveBalance가 불균형을 감지해서 ApplyGravityTorque로 막 각속도를 키우는
        // 중인데, 토크가 붙여주는 각속도를 매 스텝 GROUNDED_ANGULAR_DAMPING(0.75)만큼 도로 깎아버리면
        // 지렛대가 아직 짧아 토크 자체가 작은 초반에는 절대 MAX_TOPPLE_ANGLE까지 못 크고
        // TOPPLE_STUCK_TIMEOUT에 걸려 그 자리에서 ForceStabilize된다(physics_debug.log에서 각도 1~2도대로
        // 멈추는 것으로 실전 확인됨). TrySleepAll이 같은 이유로 이미 "불균형 진행 중이면 재우지 않는다"는
        // 예외를 두고 있는 것과 같은 기준으로, 각속도 감쇠만 불균형 에피소드 중엔 건너뛴다.
        if (block->GetImbalanceTimer() <= 0.0f)
        {
            block->DampAngularVelocity(Constants::GROUNDED_ANGULAR_DAMPING);
        }
    }

    // 2.6. [Sleep->Awake 완충] 이유는 Constants.h의 WAKE_DAMPING_DURATION 주석 참고. 위 마찰 감쇠와
    // 달리 뭔가에 닿아있는지와 무관하게, "갓 깨어났는지"만 본다 — 실제 있었던 폭주(두 블록이 서로
    // 부딪히며 계속 속도가 붙던 경우)의 피해자들은 부딪히는 그 순간엔 아직 뭔가에 안정적으로 닿아있지
    // 않았을 수 있어서다. Toppling이거나 불균형을 풀어가는 중이면 건드리지 않는다(GROUNDED_ANGULAR_DAMPING과
    // 같은 이유 — 진짜 넘어지는/기우는 자연스러운 가속을 죽이면 안 된다).
    // [연쇄 전파 감쇠 — "탄탄히 쌓인 탑에 착지 순간 먼 끝 블록이 갑자기 튀는" 버그 수정] 예전엔 모든
    // 블록에 깊이와 무관하게 같은 세기(WAKE_DAMPING_FACTOR)로 한 번만 걸었다 — 그런데 Sleeping 블록이
    // 촘촘히 맞닿아 있으면, 새로 떨어진 블록의 충격이 ResolveBlockCollisions의 솔버 반복(한 Step 안에
    // COLLISION_SOLVER_ITERATIONS번) 동안 인접한 Sleeping 블록을 순서대로 깨우며 여러 칸을 연쇄로 타고
    // 넘어갈 수 있는데, 접촉점 순차 처리 자체의 부정확성(PhysicsManager round 8/11)이 매 전달마다 조금씩
    // 남긴 잔여 힘이 안 죽고 그대로 이어지면 사슬 맨 끝 블록에서 갑자기 눈에 띄게 "툭" 튀어나오는 것처럼
    // 보인다. Block::GetWakeChainDepth()(원본 충격이면 0, 전달받아 깨울 때마다 +1)로 완충 세기를
    // WAKE_DAMPING_FACTOR의 (깊이+1)제곱으로 걸어서, 연쇄가 한 단계씩 이어질 때마다 남는 힘이 기하급수적으로
    // 줄게 한다 — 깊이 0(원본 충격)은 예전과 완전히 같은 세기 그대로다.
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        bool isRecentlyAwake = block->GetPhysicsState() == PhysicsState::Awake &&
            block->GetWakeTimer() < Constants::WAKE_DAMPING_DURATION &&
            block->GetImbalanceTimer() <= 0.0f;
        if (!isRecentlyAwake)
        {
            continue;
        }

        float chainDampingFactor = std::pow(Constants::WAKE_DAMPING_FACTOR, 1.0f + block->GetWakeChainDepth());
        block->DampVelocity(chainDampingFactor);
        block->DampAngularVelocity(chainDampingFactor);
    }

    // 2.7. [지속 접촉 에너지 증가 완화] Constants::SUSTAINED_COLLISION_DAMPING_THRESHOLD_STEPS 주석
    // 참고 — 이번 스텝에 실제로 충돌이 있었는지를 먼저 확정(AdvanceSustainedCollisionTracking)한 뒤,
    // 그게 임계 스텝 수를 넘도록 끊기지 않고 이어진 블록에만 감쇠를 건다. WAKE_DAMPING(2.6단계)과
    // 달리 "갓 깨어났는지"와 무관하게 "얼마나 오래 계속 부딪히고 있는지"만 본다.
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        bool isActive = block->GetPhysicsState() == PhysicsState::Awake || block->GetPhysicsState() == PhysicsState::Toppling;
        if (!isActive)
        {
            continue;
        }

        block->AdvanceSustainedCollisionTracking();

        int sustainedSteps = block->GetSustainedCollisionSteps();
        if (sustainedSteps <= Constants::SUSTAINED_COLLISION_DAMPING_THRESHOLD_STEPS)
        {
            continue;
        }

        float overThreshold = static_cast<float>(sustainedSteps - Constants::SUSTAINED_COLLISION_DAMPING_THRESHOLD_STEPS);
        float sustainedDampingFactor = std::pow(Constants::SUSTAINED_COLLISION_DAMPING_FACTOR, overThreshold);
        block->DampVelocity(sustainedDampingFactor);
        block->DampAngularVelocity(sustainedDampingFactor);
    }

    // [댕글링 포인터 방지] RemoveToppledBlocks()는 반드시 BuildRestingChildrenMap()보다 먼저 실행돼야
    // 한다. 화면 밖으로 나간 Toppling 블록을 여기서 먼저 지워야, 그 블록이 (지워지기 직전 순간의 낡은
    // 스냅샷으로) restingChildren에 "누군가의 위에 얹힌 자식"으로 남아있다가 SettleToppledBlocks의
    // AccumulateSupportedMass가 이미 해제된 포인터를 그대로 역참조하는 use-after-free를 막을 수 있다
    // (예전엔 이 호출이 아래 ResolveBalance 루프 뒤, SettleToppledBlocks 바로 앞에 있어서 restingChildren이
    // 삭제 전 스냅샷인 채로 재사용됐다).
    RemoveToppledBlocks();

    // 3. 밸런스 체크 (Awake + Sleeping 블록 전부)
    // [연쇄 붕괴 사각지대] Sleeping 블록은 충돌 접촉이 있어야만 다시 깨어난다.
    // 그 사이 주변 블록이 밀리거나 무게가 옮겨가면서 뒤늦게 불안정해질 수 있는데, 그런 경우 아무도
    // 다시 깨워주지 않으면 실제로는 무게중심이 지지 범위를 벗어났는데도 영원히 잠든 채로 방치된다.
    // [성능] "누가 누구 위에 얹혀 있는지"를 이 스텝에서 한 번만 계산해서 모든 블록의 ResolveBalance가
    // 재사용한다 — 예전엔 블록마다(그리고 그 안에서 재귀할 때마다) 전체 블록을 매번 다시 스캔해서,
    // 탑이 커질수록 비용이 세제곱에 가깝게 늘어났다.
    RestingChildrenMap restingChildren = BuildRestingChildrenMap();

    // [지지대 우선 붕괴 — 실제로 빠져있던 로직을 뒤늦게 구현] ResolveBalance의 반환값(true=이번 프레임에
    // 불균형으로 처리됨)은 "이 블록 위에 얹힌 모든 블록은 이번 프레임엔 따로 검사하지 않고 건너뛴다"는
    // 뜻으로 설계되어 있었는데(ResolveBalance 선언부 주석 참고), 예전 코드는 이 반환값을 그냥 버려서
    // 실제로는 탑의 모든 블록이 base가 이미 불안정 처리됐는지와 무관하게 매 프레임 각자 독립적으로
    // 균형 판정을 받고 있었다 — "아래가 먼저 무너지고 위는 그 결과에 묻어간다"는 의도와 달리 여러 층이
    // 같은 프레임에 동시에 제각각 넘어지기 시작할 수 있었다.
    //
    // [순서 문제] GetAllBlocks()가 주는 순서는 생성(스폰) 순서라 실제로 쌓인 순서와 무관하다 — 그대로
    // 훑으면 자식이 부모(base)보다 먼저 처리되는 경우가 생겨서, base가 나중에 불안정 판정을 받아도 이미
    // 처리해버린 자식에게는 스킵이 소급 적용되지 않는다. 그래서 restingChildren(base -> 자식)을 따라
    // "누구 위에도 안 얹힌(=root)" 블록부터 시작해 자식 쪽으로 재귀하는 순서로 방문한다 — 그래야 base가
    // 항상 자식보다 먼저 처리된다.
    std::vector<Block*> hasBaseBelow;
    for (const std::pair<Block* const, std::vector<std::pair<Block*, float>>>& baseEntry : restingChildren)
    {
        for (const std::pair<Block*, float>& childEntry : baseEntry.second)
        {
            if (std::find(hasBaseBelow.begin(), hasBaseBelow.end(), childEntry.first) == hasBaseBelow.end())
            {
                hasBaseBelow.push_back(childEntry.first);
            }
        }
    }

    std::vector<Block*> balanceVisited;

    // current부터 시작해서 restingChildren을 따라 자식 쪽으로 재귀한다. forceSkip이 true면(이미 이번
    // 프레임에 자신의 base 어딘가가 불안정 처리됐다는 뜻) ResolveBalance 자체를 호출하지 않고, 그 결과를
    // 그대로 자식에게도 물려준다. Toppling 상태인 블록(current 자신이 pass-through 중일 수 있음)은
    // ResolveBalance/안전장치 대상이 아니지만, 그 위에 얹힌 자식들에게 도달하려면 재귀 자체는 계속돼야
    // 하므로 자식 순회는 항상 한다. Step()의 절대각 안전장치는 스킵 여부와 무관하게 원래처럼 모든
    // Awake/Sleeping 블록에 그대로 적용한다 — ResolveBalance 캐스케이드와는 별개의 안전판이라서다.
    // [자식이 base의 전체 불균형 기간 내내 얼어붙는 문제 수정] childForceSkip 계산부 주석 참고 — base가
    // "새로" 불균형을 시작한 그 한 프레임만 자식에게 스킵을 물려주고, base가 계속 기울어지는(느리게
    // 넘어가는) 나머지 기간에는 자식도 매 프레임 독립적으로 재평가된다.
    std::function<void(Block*, bool)> resolveBalanceCascade = [&](Block* current, bool forceSkip)
    {
        if (std::find(balanceVisited.begin(), balanceVisited.end(), current) != balanceVisited.end())
        {
            return;
        }
        balanceVisited.push_back(current);

        PhysicsState currentState = current->GetPhysicsState();
        bool isEligible = currentState == PhysicsState::Awake || currentState == PhysicsState::Sleeping;
        bool wasProcessed = false;

        // [임시 디버그] forceSkip으로 ResolveBalance 자체가 스킵된 프레임을 남긴다 — 불균형 타이머가
        // 멈춘 채(끝없이 skip)로 남는 케이스를 확인하기 위함.
        if (isEligible && forceSkip && current->GetImbalanceTimer() > 0.0f)
        {
            LogFreezeDebug("[CASCADE-SKIP] " + DescribeBlock(current));
        }

        // [지지대가 느리게 넘어가는 동안 자식이 무한정 얼어붙는 문제 수정 — 실전 로그로 확인됨] 자식을
        // forceSkip하는 원래 의도는 "같은 프레임에 여러 층이 동시에 넘어지기 시작하는 것"만 막는 것이었는데
        // (childForceSkip 계산부 주석 참고), 실제로는 base가 완전히 넘어갈 때까지(TOPPLE_STUCK_TIMEOUT
        // 3초 또는 MAX_TOPPLE_ANGLE 도달까지) "wasProcessed=true"가 계속 나와서 그 몇 초 내내 자식이
        // 한 번도 자기 지지 상태를 재평가 못 하고 완전히 얼어붙는다(physics_debug.log에서 자식 블록이
        // [CASCADE-SKIP]으로 1초 넘게 60번 이상 연속으로 찍히는 것으로 확인됨 — 명백히 무너져야 할 구조가
        // 안 무너지는 것처럼 보이는 원인). base가 이번 프레임에 "새로" 불균형을 시작한 경우(같은 프레임
        // 동시 시작 방지가 실제로 필요한 그 순간)만 스킵을 전파하고, 그 이후 프레임(base가 계속 기울어지는
        // 중)에는 자식도 매 프레임 독립적으로 다시 평가하게 한다.
        bool wasNewEpisodeBeforeResolve = current->GetImbalanceTimer() <= 0.0f;

        if (isEligible)
        {
            if (!forceSkip)
            {
                wasProcessed = ResolveBalance(current, deltaTime, restingChildren);
            }

            // ResolveBalance 외부적인 요인(충돌 등)으로 각도가 꺾인 경우의 안전장치.
            // [절대각이 아니라 기준각 대비 — 실전에서 발견된 무한 진동 버그 수정] GetAngle()의 절대값이
            // 아니라 GetRestAngleReference()(마지막으로 안정 확인된 각도)에서 얼마나 더 돌았는지를 봐야
            // 한다 — 안 그러면 90/135도로 완전히 넘어져서 옆으로 누운 채 정상적으로 안정된 블록도 절대각이
            // 여전히 40도를 넘는다는 이유만으로 여기 계속 걸려서, SettleToppledBlocks가 Awake로 돌려보내자
            // 마자 바로 이 줄이 다시 Toppling으로 되돌리는 걸 매 프레임 무한 반복하며 제자리에서 떤다.
            float angleFromRest = std::fabs(current->GetAngle() - current->GetRestAngleReference());
            if (current->GetPhysicsState() != PhysicsState::Toppling && angleFromRest >= Constants::MAX_TOPPLE_ANGLE)
            {
                // [연쇄 붕괴 추적] 이 블록이 왜 넘어지기 시작했는지(첫 시작점)를 남긴다 — WAKE/SLEEP만으로는
                // "이미 넘어지는 중인 블록이 뭔가를 침"만 보이고, 애초에 그 블록이 왜 넘어지기 시작했는지는
                // 안 남아서 연쇄의 진짜 출발점을 못 찾는 문제가 있었다(실전 확인).
                LogFreezeDebug("[TOPPLING] safety-net-angle " + DescribeBlock(current));
                current->BeginToppling();
            }
        }

        bool childForceSkip = forceSkip || (wasProcessed && wasNewEpisodeBeforeResolve);
        auto childrenIt = restingChildren.find(current);
        if (childrenIt != restingChildren.end())
        {
            for (const std::pair<Block*, float>& childEntry : childrenIt->second)
            {
                resolveBalanceCascade(childEntry.first, childForceSkip);
            }
        }
    };

    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        // [순서 문제] 재귀의 시작점(root)은 "누구 위에도 얹혀있지 않은" 블록이면 되고, 그 자신의 상태는
        // Awake/Sleeping/Toppling 어느 쪽이든 상관없다 — Toppling인 root라도 그 위에 얹힌 진짜 대상
        // (Awake/Sleeping 자식)에 닿으려면 pass-through로라도 재귀를 시작해야 한다. Airborne만 애초에
        // restingChildren 구성에서 제외되므로 여기서도 제외한다.
        if (block->GetPhysicsState() == PhysicsState::Airborne)
        {
            continue;
        }
        bool isRoot = std::find(hasBaseBelow.begin(), hasBaseBelow.end(), block) == hasBaseBelow.end();
        if (isRoot)
        {
            resolveBalanceCascade(block, false);
        }
    }

    // [속도 폭주 방지 안전판] 이유는 Constants.h의 MAX_LINEAR_VELOCITY/MAX_ANGULAR_VELOCITY 주석 참고.
    // 중력/충돌 임펄스/토크(위 1~3단계) 전부가 끝난 뒤, 이번 스텝의 최종 결과값에 한 번만 상한을 건다 —
    // SettleToppledBlocks/TrySleepAll이 속도로 판단을 내리기 전에 걸려야 그쪽도 정상 범위를 본다.
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        PhysicsState state = block->GetPhysicsState();
        if (state != PhysicsState::Awake && state != PhysicsState::Toppling) continue;

        float speed = std::sqrt(block->GetSpeedSquared());
        if (speed > Constants::MAX_LINEAR_VELOCITY)
        {
            block->DampVelocity(Constants::MAX_LINEAR_VELOCITY / speed);
        }

        float angularSpeed = std::fabs(block->GetAngularVelocity());
        if (angularSpeed > Constants::MAX_ANGULAR_VELOCITY)
        {
            block->DampAngularVelocity(Constants::MAX_ANGULAR_VELOCITY / angularSpeed);
        }
    }

    SettleToppledBlocks(restingChildren);
    TrySleepAll(deltaTime);
}
void PhysicsManager::ApplyGravity(Block* block)
{
    // 질량과 무관하게 항상 같은 가속도(GRAVITY)로 떨어지게 하려고, 힘 = 질량 * 중력가속도로 건다
    // (Integrate가 힘을 다시 질량으로 나누므로 결국 가속도는 GRAVITY로 통일됨)
    block->ApplyForce({ 0.0f, block->GetMass() * Constants::GRAVITY });
}

void PhysicsManager::ResolveFloorCollision(Block* block, bool swapContactOrder)
{
    // [바닥 SAT 통합] 바닥을 하나의 정적 OBB로 취급해서, 블럭-블럭과 똑같이 CollisionManager가
    // 칸 단위 SAT + 접촉 매니폴드(접촉점 2개) 판정을 전담한다. 예전엔 여기서 직접 칸의 네 모서리를
    // 스캔하며 법선을 항상 (0,-1)로 고정하고 왼쪽/오른쪽 끝 모서리를 손으로 골랐는데, 그 판정 로직이
    // 이제 CollisionManager::DetectFloorCollision 하나로 모였다 — 여기서는 결과를 받아 반응만 한다.
    CollisionPair pair;
    if (!CollisionManager::GetInstance().DetectFloorCollision(block, pair))
    {
        return;
    }

    // [접촉 다각화] 먼저 처리되는 점만 위치 보정을 하고(penetration 그대로), 나중 점은 이미 밀어낸
    // 뒤라 penetration을 0으로 넘겨서 속도/토크 반응만 추가로 준다 — 면과 면이 맞닿았을 때 양 끝을
    // 동시에 붙잡아서 미세 회전(떨림)이 사라진다. (한때 이 두 호출을 하나로 합쳐 "동시 처리"했다가, 진짜
    // 충돌에서 임펄스가 과도하게 커지는 폭주 버그로 되돌렸다 — Block::ResolveRigidCollision 주석 참고.)
    // [좌우 편향 방지] contactPoints[0]/[1]은 BuildManifold가 항상 "왼쪽/오른쪽" 순서로 결정론적으로
    // 고정해서 주기 때문에, 항상 같은 순서로만 순차 처리하면 완벽히 대칭인 충돌에서도 매번 같은 부호의
    // 잔여 각속도가 남는다(PhysicsManager.h의 ResolveBlockPairCollision 주석 참고). swapContactOrder로
    // 어느 점이 먼저 처리될지를 뒤집어서 그 편향을 상쇄시킨다 — 이 값은 프레임(Step) 단위로 뒤집힌다.
    if (swapContactOrder)
    {
        block->ResolveRigidCollision(pair.contactPoints[1], pair.normal, pair.penetration);
        block->ResolveRigidCollision(pair.contactPoints[0], pair.normal, 0.0f);
    }
    else
    {
        block->ResolveRigidCollision(pair.contactPoints[0], pair.normal, pair.penetration);
        block->ResolveRigidCollision(pair.contactPoints[1], pair.normal, 0.0f);
    }
}

void PhysicsManager::ResolveBlockPairCollision(Block* block, Block* other, bool swapContactOrder)
{
    // [책임 분리] "겹쳤는지/어디를/얼마나 겹쳤는지"(SAT, 접촉 매니폴드)는 CollisionManager가 전담한다.
    // 여기서는 그 판정 결과를 가지고 "그래서 어떻게 반응할지"(WakeUp 여부, 강체 충돌 임펄스)만 담당한다.
    CollisionPair pair;
    if (!CollisionManager::GetInstance().DetectPairCollision(block, other, pair))
    {
        return;
    }

    bool blockMovable = block->GetPhysicsState() == PhysicsState::Awake || block->GetPhysicsState() == PhysicsState::Toppling;
    bool otherMovable = other->GetPhysicsState() == PhysicsState::Awake || other->GetPhysicsState() == PhysicsState::Toppling;

    bool blockIsRealImpact = block->GetSpeedSquared() > Constants::WAKE_IMPACT_SPEED_THRESHOLD * Constants::WAKE_IMPACT_SPEED_THRESHOLD;
    bool otherIsRealImpact = other->GetSpeedSquared() > Constants::WAKE_IMPACT_SPEED_THRESHOLD * Constants::WAKE_IMPACT_SPEED_THRESHOLD;

    if (blockMovable && blockIsRealImpact && other->GetPhysicsState() == PhysicsState::Sleeping)
    {
        // [연쇄 붕괴 추적] 깨어나는 쪽(other)뿐 아니라 때린 쪽(block, impactor)도 같이 남긴다 —
        // 안 그러면 "누가 이 연쇄를 시작했는지" 로그만으로는 추적이 안 된다(실전에서 필요성 확인).
        LogFreezeDebug("[WAKE] impact victim=(" + DescribeBlock(other) + ") impactor=(" + DescribeBlock(block) + ")");
        other->WakeUp();
        // [Sleep->Awake 완충 — 연쇄 전파 감쇠, Block::GetWakeChainDepth 주석 참고] 때린 쪽(block)의
        // 연쇄 깊이를 그대로 물려받아 +1 한다 — block 자신도 이미 연쇄로 충격을 전달받은 상태였다면
        // (GetWakeChainDepth() > 0), other는 그보다 한 단계 더 깊은 곳에서 깨어난 것이므로 Step() 2.6
        // 단계에서 더 강한 완충을 받는다. block이 진짜 원본 충격원(깊이 0)이면 other는 깊이 1이 되어
        // 예전과 동일한(WAKE_DAMPING_FACTOR 그대로) 완충을 받는다 — 즉 연쇄가 없던 기존 동작과 호환된다.
        other->SetWakeChainDepth(block->GetWakeChainDepth() + 1);
        otherMovable = true; // 이제 other도 움직일 수 있게 되었으므로 true로 바꿔줌
    }
    else if (otherMovable && otherIsRealImpact && block->GetPhysicsState() == PhysicsState::Sleeping)
    {
        LogFreezeDebug("[WAKE] impact victim=(" + DescribeBlock(block) + ") impactor=(" + DescribeBlock(other) + ")");
        block->WakeUp();
        block->SetWakeChainDepth(other->GetWakeChainDepth() + 1);
        blockMovable = true; // 이제 block도 움직일 수 있게 되었으므로 true로 바꿔줌
    }
    else if (blockMovable && otherMovable && (blockIsRealImpact || otherIsRealImpact))
    {
        // [이미 Awake끼리의 고속 충돌 추적 — 실전에서 필요성 확인] 위 두 분기는 "Sleeping 쪽을 깨우는"
        // 경우만 남긴다. 그런데 둘 다 이미 Awake/Toppling인 상태에서 격렬하게 부딪히는 경우(예: 넘어지는
        // 블록이 이미 움직이던 다른 블록을 침)는 깨울 필요가 없어서 여태 전혀 로그가 안 남았다 — 그래서
        // 갑자기 v=600대의 큰 값이 로그에 나타나도 누가/왜 쳤는지 추적할 방법이 없었다.
        LogFreezeDebug("[COLLISION] fast-awake a=(" + DescribeBlock(block) + ") b=(" + DescribeBlock(other) + ")");
    }

    // [지속 접촉 에너지 증가 완화 — Block::GetSustainedCollisionSteps 선언부 주석 참고] "진짜 충돌"
    // 판정을 받은 쪽에 이번 스텝에 접촉이 있었다고 표시해둔다. Step()이 매 스텝 끝에서 이걸 읽어
    // 몇 스텝 연속으로 이어지는지 센다.
    if (blockIsRealImpact)
    {
        block->MarkRealImpactThisStep();
    }
    if (otherIsRealImpact)
    {
        other->MarkRealImpactThisStep();
    }

    // [붕괴 충돌음] 둘 중 하나가 무게중심이 무너져 회전+낙하 이탈 중인 상태(Toppling)이고, 그 블록이
    // 실제로 빠르게 부딪히는 중일 때만 재생한다 — 가만히 얹혀있거나 미세하게 떠는 접촉은 blockIsRealImpact/
    // otherIsRealImpact가 false라 자동으로 걸러진다. m_collisionSfxPlayedThisStep으로 한 스텝(4회 반복 x
    // 여러 쌍)에 최대 한 번만 울리게 제한한다.
    bool blockIsTopplingImpact = block->GetPhysicsState() == PhysicsState::Toppling && blockIsRealImpact;
    bool otherIsTopplingImpact = other->GetPhysicsState() == PhysicsState::Toppling && otherIsRealImpact;
    if (!m_collisionSfxPlayedThisStep && (blockIsTopplingImpact || otherIsTopplingImpact))
    {
        SoundManager::GetInstance().PlaySfx("assets/Sound/Pong.mp3");
        m_collisionSfxPlayedThisStep = true;
    }

    // [접촉 다각화] CollisionManager가 넘겨준 접촉점 2개에 대해서만 반응을 준다. 먼저 처리되는 점만
    // 위치 보정을 하고(penetration 그대로), 나중 점은 이미 밀어낸 뒤라 penetration을 0으로 넘겨서
    // 속도/토크 반응만 추가로 준다 — 면과 면이 맞닿았을 때 양 끝을 동시에 붙잡아서 미세 회전(떨림)이 사라진다.
    // [좌우 편향 방지] swapContactOrder로 두 점 중 어느 쪽이 먼저 처리될지를 뒤집는다(프레임 단위) —
    // 이유는 PhysicsManager.h의 ResolveBlockPairCollision 선언부 주석 참고.
    Vector2 firstPoint = swapContactOrder ? pair.contactPoints[1] : pair.contactPoints[0];
    Vector2 secondPoint = swapContactOrder ? pair.contactPoints[0] : pair.contactPoints[1];

    if (blockMovable && otherMovable)
    {
        // 둘 다 움직일 수 있으면 진짜 쌍방향 충돌
        block->ResolveRigidCollisionWithBlock(other, firstPoint, pair.normal, pair.penetration);
        block->ResolveRigidCollisionWithBlock(other, secondPoint, pair.normal, 0.0f);
    }
    else if (blockMovable)
    {
        // other는 Sleeping(고정) — block만 밀려남. normal이 이미 "other->block" 방향이라 그대로 씀
        block->ResolveRigidCollision(firstPoint, pair.normal, pair.penetration);
        block->ResolveRigidCollision(secondPoint, pair.normal, 0.0f);
    }
    else if (otherMovable)
    {
        // block이 Sleeping(고정) — other만 밀려남. other 입장에선 반대(block->other) 방향이 필요해서 뒤집는다
        other->ResolveRigidCollision(firstPoint, pair.normal * -1.0f, pair.penetration);
        other->ResolveRigidCollision(secondPoint, pair.normal * -1.0f, 0.0f);
    }
}

void PhysicsManager::ResolveBlockCollisions(bool swapContactOrder)
{
    std::vector<Block*> allBlocks = BlockManager::GetInstance().GetAllBlocks();

    // [강체물리 3단계] 이제 충돌 반응이 양쪽 다 반응하는 진짜 쌍방향이라, 같은 쌍을 두 번 풀면
    // ((X,Y)와 (Y,X) 양쪽에서 각각) 임펄스를 두 번 주는 꼴이 되어버린다. 그래서 i<j로만 순회해서
    // 정확히 한 쌍당 한 번만 처리한다.
    for (size_t i = 0; i < allBlocks.size(); ++i)
    {
        Block* blockA = allBlocks[i];
        bool aIsSolid = blockA->GetPhysicsState() == PhysicsState::Awake ||
            blockA->GetPhysicsState() == PhysicsState::Sleeping ||
            blockA->GetPhysicsState() == PhysicsState::Toppling;
        if (!aIsSolid)
        {
            continue;
        }

        for (size_t j = i + 1; j < allBlocks.size(); ++j)
        {
            Block* blockB = allBlocks[j];
            bool bIsSolid = blockB->GetPhysicsState() == PhysicsState::Awake ||
                blockB->GetPhysicsState() == PhysicsState::Sleeping ||
                blockB->GetPhysicsState() == PhysicsState::Toppling;
            bool blockAMovable = blockA->GetPhysicsState() == PhysicsState::Awake || blockA->GetPhysicsState() == PhysicsState::Toppling;
            bool blockBMovable = blockB->GetPhysicsState() == PhysicsState::Awake || blockB->GetPhysicsState() == PhysicsState::Toppling;
            bool eitherMovable = blockAMovable || blockBMovable;

            // 둘 다 안 움직이는 상태(Sleeping끼리)면 부딪혀도 반응할 게 없으니 건너뜀
            if (!bIsSolid || !eitherMovable)
            {
                continue;
            }

            ResolveBlockPairCollision(blockA, blockB, swapContactOrder);
        }
    }
}

float PhysicsManager::GetClampedImbalanceDeadzone(float supportMinX, float supportMaxX) const
{
    // [얇은 지지 폭 방지] 이유는 이 함수 선언부(PhysicsManager.h) 주석 참고 — 지지 폭의 절반을
    // 넘지 않도록 clamp해서, 양쪽 데드존이 겹쳐 "균형" 판정이 아예 불가능해지는 걸 막는다.
    float supportWidth = supportMaxX - supportMinX;
    return (std::min)(Constants::IMBALANCE_DEADZONE, supportWidth * 0.5f);
}

void PhysicsManager::GetCellSupportBounds(Block* block, int cellIndex, float& outTopY, float& outBottomY, float& outMinX, float& outMaxX) const
{
    Vector2 corners[4];
    block->GetCellRotatedCorners(cellIndex, corners);

    outTopY = corners[0].y;
    outBottomY = corners[0].y;
    outMinX = corners[0].x;
    outMaxX = corners[0].x;

    for (int i = 1; i < 4; ++i)
    {
        if (corners[i].y < outTopY) outTopY = corners[i].y;
        if (corners[i].y > outBottomY) outBottomY = corners[i].y;
        if (corners[i].x < outMinX) outMinX = corners[i].x;
        if (corners[i].x > outMaxX) outMaxX = corners[i].x;
    }
}
bool PhysicsManager::IsCellSupported(Block* block, int cellIndex) const
{
    float cellTopY = 0.0f, cellBottomY = 0.0f, cellMinX = 0.0f, cellMaxX = 0.0f;
    GetCellSupportBounds(block, cellIndex, cellTopY, cellBottomY, cellMinX, cellMaxX);
    float cellCenterX = (cellMinX + cellMaxX) * 0.5f;

    // [왼쪽 끝/가운데/오른쪽 끝, 세 지점 레이캐스트] 서로 떨어진 두 지지대(예: 양쪽에 블럭 두 개, 가운데는
    // 완전한 허공) 위에 이 칸이 걸쳐 있으면, 좌우 끝 레이는 각각 지지대에 맞지만 가운데 레이는 허공을
    // 그대로 지나가 맞는 게 없다 — 세 지점 중 하나라도 맞으면 "지지는 있다"고 본다(가운데가 비어 있어도
    // 좌우 어느 한쪽에 진짜로 얹혀 있으면 완전한 자유낙하는 아니므로).
    float unusedHitDistance = 0.0f;
    return RaycastDownForSupport({ cellMinX, cellBottomY }, block, unusedHitDistance) ||
        RaycastDownForSupport({ cellCenterX, cellBottomY }, block, unusedHitDistance) ||
        RaycastDownForSupport({ cellMaxX, cellBottomY }, block, unusedHitDistance);
}

bool PhysicsManager::RaycastDownForSupport(Vector2 origin, Block* selfBlock, float& outHitDistance) const
{
    bool hasHit = false;
    outHitDistance = 0.0f;

    bool isOverFloorPlatform = origin.x >= Constants::FLOOR_LEFT_X && origin.x <= Constants::FLOOR_RIGHT_X;
    if (isOverFloorPlatform)
    {
        float distanceToFloor = Constants::FLOOR_TOP_Y - origin.y;
        bool isWithinTolerance = distanceToFloor >= -Constants::SUPPORT_CHECK_TOLERANCE &&
            distanceToFloor <= Constants::SUPPORT_CHECK_TOLERANCE;
        if (isWithinTolerance)
        {
            outHitDistance = distanceToFloor;
            hasHit = true;
        }
    }

    for (Block* other : BlockManager::GetInstance().GetAllBlocks())
    {
        // [경사면 미끄러짐 허용] GetCellSupportRange와 같은 이유로, 회전을 무시하고 "평평한 선반"으로
        // 근사하는 이 판정에서 축 정렬에서 크게 벗어난 블럭은 지지 제공자 후보에서 뺀다.
        bool otherCanSupport = other != selfBlock &&
            other->GetPhysicsState() != PhysicsState::Airborne &&
            other->GetPhysicsState() != PhysicsState::Toppling &&
            IsNearAxisAlignedAngle(other->GetAngle());
        if (!otherCanSupport)
        {
            continue;
        }

        // [성능] 실제 모서리 계산 전에 블록 원점 Y거리로 먼저 걸러낸다(GetCellSupportRange와 같은 이유)
        if (std::fabs(other->GetRenderPosition().y - origin.y) > Constants::SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT)
        {
            continue;
        }

        for (int j = 0; j < other->GetCellCount(); ++j)
        {
            float otherTopY = 0.0f, otherBottomY = 0.0f, otherMinX = 0.0f, otherMaxX = 0.0f;
            GetCellSupportBounds(other, j, otherTopY, otherBottomY, otherMinX, otherMaxX);

            // 레이가 이 칸의 X범위를 지나가지 않으면(가로로 빗나가면) 애초에 맞을 수 없다
            if (origin.x < otherMinX || origin.x > otherMaxX)
            {
                continue;
            }

            // 내 바닥이 맞닿아야 하는 건 상대 칸의 "윗면"이다 — 반대로 비교하면 다른 블록 위에 쌓인
            // 블록은 지지 판정이 늘 실패한다
            float distanceToOther = otherTopY - origin.y;
            bool isWithinTolerance = distanceToOther >= -Constants::SUPPORT_CHECK_TOLERANCE &&
                distanceToOther <= Constants::SUPPORT_CHECK_TOLERANCE;
            if (!isWithinTolerance)
            {
                continue;
            }

            // 진짜 레이캐스트처럼, 이미 찾은 것보다 더 가까운(레이 시작점에 더 가까운) 지지면만 갱신한다
            if (!hasHit || std::fabs(distanceToOther) < std::fabs(outHitDistance))
            {
                outHitDistance = distanceToOther;
                hasHit = true;
            }
        }
    }

    return hasHit;
}

bool PhysicsManager::GetCellSupportRange(Block* block, int cellIndex, float& outSupportMinX, float& outSupportMaxX) const
{
    float cellTopY = 0.0f;
    float cellBottomY = 0.0f;
    float cellMinX = 0.0f;
    float cellMaxX = 0.0f;
    GetCellSupportBounds(block, cellIndex, cellTopY, cellBottomY, cellMinX, cellMaxX);

    bool isNearFloorHeight = cellBottomY >= Constants::FLOOR_TOP_Y - Constants::SUPPORT_CHECK_TOLERANCE &&
        cellBottomY <= Constants::FLOOR_TOP_Y + Constants::SUPPORT_CHECK_TOLERANCE;
    // [근본 수정 — 서브셀 걸침이 부자연스럽던 원인] 예전엔 칸 중심이 발판 위에 있어야만(=절반 넘게
    // 걸쳐야) 지지로 인정했다. 그런데 실제로 반환하는 지지 범위(아래 outSupportMinX/MaxX)는 이미 겹친
    // 부분만 std::max/min으로 잘라서 주므로, 절반 미만 걸친 칸을 포함시켜도 그 칸이 만드는 좁은 접촉
    // 폭 그대로만 반영될 뿐 범위가 부풀지 않는다 — 즉 중심 기준 이분법은 이제 안전장치로서 의미가 없다.
    // 반면 서브셀(반칸=SUBCELL_SIZE=24px) 단위 착지는 "정확히 50%" 걸침이 흔한데, 중심 기준 이분법이면
    // 49% 걸침은 지지 0, 51% 걸침은 그 칸 전체가 지지 후보로 등판하는 것처럼 취급돼 지지 범위가 프레임
    // 사이에 없다가 갑자기 넓어지며 튀어 보였다. 걸친 만큼(조금이라도 겹치면 그만큼만) 그대로 지지로
    // 반영해야 무게중심이 걸친 비율에 비례해 시소처럼 서서히 기운다.
    bool overlapsFloorPlatform = cellMaxX > Constants::FLOOR_LEFT_X && cellMinX < Constants::FLOOR_RIGHT_X;
    if (isNearFloorHeight && overlapsFloorPlatform)
    {
        // [경계값 버그 2 — 실전 확인됨] 칸 중심이 지지 쪽에 있다고 해서 칸 "전체 폭"을 지지 범위에 다
        // 넣으면 안 된다 — 칸이 발판 가장자리에 절반쯤 걸친 경우(중심은 발판 위, 나머지 절반은 허공),
        // 칸의 전체 폭을 지지 범위로 치면 실제보다 넓게(허공 쪽으로) 잡혀서 무게중심이 그 안에 들어와
        // 버린다. 4칸 중 3칸이 기둥으로 곧게 서고 발 하나만 옆으로 삐져나온 모양이 착지 위치에 따라
        // 이렇게 "명백히 넘어져야 하는데 안 넘어지는" 것으로 실전 확인됐다. 그래서 칸의 폭이 아니라
        // 발판과 실제로 겹치는 부분(교집합)만 돌려준다.
        outSupportMinX = (std::max)(cellMinX, Constants::FLOOR_LEFT_X);
        outSupportMaxX = (std::min)(cellMaxX, Constants::FLOOR_RIGHT_X);
        return true;
    }

    // [분리된 두 지지대 유니온 — IsCellSupported/RaycastDownForSupport와 기준 통일] 이 칸이 서로 다른
    // 두 블록 위에 나뉘어 걸쳐 있을 수 있다(예: 가운데는 허공이고 양 끝만 각각 다른 블록에 걸친 경우) —
    // IsCellSupported/RaycastDownForSupport는 왼쪽/가운데/오른쪽 3점 레이캐스트로 이런 경우까지 "지지
    // 있음"으로 정확히 잡아내는데, 여기는 예전에 "처음 찾은 지지 제공자 하나"만 보고 그 즉시 반환해서
    // 반대쪽에 실제로 있는 두 번째 지지대를 무시했다 — 그러면 실제보다 지지 범위가 좁게 잡혀서 무게중심이
    // (진짜로는 양쪽 다 지지되는데도) 범위 밖으로 판정되는 오탐 불균형이 생길 수 있었다. 이제 겹치는
    // 지지 제공자를 전부 찾아서 그 클리핑된 범위들의 합집합(가장 왼쪽 min ~ 가장 오른쪽 max)을 돌려준다.
    bool hasBlockSupport = false;
    float blockSupportMinX = 0.0f;
    float blockSupportMaxX = 0.0f;

    for (Block* other : BlockManager::GetInstance().GetAllBlocks())
    {
        // Toppling(무너져서 낙하 중)인 블럭은 그 자체가 안 안정적인 상태라, 다른 블럭이 그 위에
        // 안정적으로 얹혀 있다고 볼 수 없다 — 지지 제공자로 인정하지 않는다
        // [경사면 미끄러짐 허용] 이 지지 판정 자체가 회전을 무시하고 "평평한 선반"으로 근사하는
        // 방식이라, 많이 기울어진 블럭까지 이걸로 지지 제공자 취급하면 실제로는 모서리 하나에 걸친
        // 것도 안정적으로 얼어붙는다(Constants::SUPPORT_SURFACE_MAX_TILT_DEGREES 주석 참고). 축
        // 정렬에서 크게 벗어난 블럭은 후보에서 빼고, 실제 회전 인식 충돌/마찰 물리로 넘긴다.
        bool otherCanSupport = other != block &&
            other->GetPhysicsState() != PhysicsState::Airborne &&
            other->GetPhysicsState() != PhysicsState::Toppling &&
            IsNearAxisAlignedAngle(other->GetAngle());
        if (!otherCanSupport)
        {
            continue;
        }

        // [성능] 실제 모서리(회전 반영, sin/cos 포함) 계산은 비싸다. 블록 원점 Y가 이 칸의 바닥과
        // SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT보다 멀리 떨어져 있으면, 그 블록의 어떤 칸도 이 칸에
        // 닿을 수 없다는 뜻이니 모서리 계산 없이 곧바로 건너뛴다.
        if (std::fabs(other->GetRenderPosition().y - cellBottomY) > Constants::SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT)
        {
            continue;
        }

        for (int j = 0; j < other->GetCellCount(); ++j)
        {
            float otherTopY = 0.0f, otherBottomY = 0.0f, otherMinX = 0.0f, otherMaxX = 0.0f;
            GetCellSupportBounds(other, j, otherTopY, otherBottomY, otherMinX, otherMaxX);

            // 내 칸의 바닥이 맞닿아야 하는 건 상대 칸의 "바닥"이 아니라 "윗면"이다 — 반대로 비교하면
            // 다른 블록 위에 쌓인 블록은 지지 판정이 늘 실패해서 절대 Sleep에 못 들어간다
            // [근본 수정 — 위 FLOOR 케이스와 같은 이유] 칸 중심이 상대 칸 위에 있어야만(절반 초과) 지지로
            // 인정하던 이분법을 걷어냈다. 아래에서 지지 범위를 실제 겹친 폭만큼만 클리핑해서 돌려주므로,
            // 살짝만 걸친 칸도 그 좁은 폭만큼만 지지로 반영될 뿐 범위가 부풀지 않는다. 서브셀(반칸) 단위
            // 착지에서 흔한 "정확히 50%" 걸침 때 49%/51% 사이에서 지지 유무가 통째로 뒤집히던 것도 같이
            // 없어진다 — 걸친 만큼 비례해서 지지가 잡혀야 시소처럼 자연스럽게 기운다.
            bool overlapsOther = cellMaxX > otherMinX && cellMinX < otherMaxX;
            bool isRestingOnOther = cellBottomY >= otherTopY - Constants::SUPPORT_CHECK_TOLERANCE &&
                cellBottomY <= otherTopY + Constants::SUPPORT_CHECK_TOLERANCE;

            if (overlapsOther && isRestingOnOther)
            {
                // [경계값 버그 2] 위 바닥 케이스와 같은 이유로, 아래 블록과 실제로 겹치는 부분만 반영한다.
                float clippedMinX = (std::max)(cellMinX, otherMinX);
                float clippedMaxX = (std::min)(cellMaxX, otherMaxX);
                if (!hasBlockSupport)
                {
                    blockSupportMinX = clippedMinX;
                    blockSupportMaxX = clippedMaxX;
                    hasBlockSupport = true;
                }
                else
                {
                    if (clippedMinX < blockSupportMinX) blockSupportMinX = clippedMinX;
                    if (clippedMaxX > blockSupportMaxX) blockSupportMaxX = clippedMaxX;
                }
            }
        }
    }

    if (hasBlockSupport)
    {
        outSupportMinX = blockSupportMinX;
        outSupportMaxX = blockSupportMaxX;
        return true;
    }

    return false;
}

bool PhysicsManager::IsCellTouchingAnySupport(Block* block, int cellIndex) const
{
    float cellTopY = 0.0f;
    float cellBottomY = 0.0f;
    float cellMinX = 0.0f;
    float cellMaxX = 0.0f;
    GetCellSupportBounds(block, cellIndex, cellTopY, cellBottomY, cellMinX, cellMaxX);

    // [마찰 사각지대 방지] IsCellSupported와 달리 "중심이 그 위에 있는지"가 아니라 "칸의 X범위가
    // 조금이라도 겹치는지"만 본다 — 모서리로만 걸친 접촉에도 마찰이 걸리게 하려는 것.
    bool isNearFloorHeight = cellBottomY >= Constants::FLOOR_TOP_Y - Constants::SUPPORT_CHECK_TOLERANCE &&
        cellBottomY <= Constants::FLOOR_TOP_Y + Constants::SUPPORT_CHECK_TOLERANCE;
    bool overlapsFloorPlatform = cellMaxX > Constants::FLOOR_LEFT_X && cellMinX < Constants::FLOOR_RIGHT_X;
    if (isNearFloorHeight && overlapsFloorPlatform)
    {
        return true;
    }

    for (Block* other : BlockManager::GetInstance().GetAllBlocks())
    {
        // [지지 없이 얼어붙는 버그 수정 — GetCellSupportRange와 기준 통일] 여기 IsNearAxisAlignedAngle
        // 조건이 빠져있으면, 5도 넘게 기운(예: 넘어지다 만) 블럭에 살짝 닿기만 해도 "마찰상으로는 닿음"
        // 판정을 받는다. 그런데 GetCellSupportRange(공식 지지 판정, Sleep/Wake을 결정)는 이미 이 조건으로
        // 그런 블럭을 지지 제공자에서 제외하므로, 위에 얹힌 블럭은 "공식적으로는 지지 없음"이라 매 스텝
        // ResolveBalance가 깨우려고 하는데, 동시에 이 느슨한 판정만으로 걸리는 마찰 감쇠(DampVelocity)와
        // 충돌 반응이 매 프레임 속도를 죽여서 실제로는 안 떨어지고 그 자리에 붙박인다 — "지지가 없다고
        // 판정되는데도 얼어붙는" 증상의 원인이었다. 두 판정이 같은 기준(축 정렬 5도 이내)을 써야 한다.
        bool otherCanSupport = other != block &&
            other->GetPhysicsState() != PhysicsState::Airborne &&
            other->GetPhysicsState() != PhysicsState::Toppling &&
            IsNearAxisAlignedAngle(other->GetAngle());
        if (!otherCanSupport)
        {
            continue;
        }

        if (std::fabs(other->GetRenderPosition().y - cellBottomY) > Constants::SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT)
        {
            continue;
        }

        for (int j = 0; j < other->GetCellCount(); ++j)
        {
            float otherTopY = 0.0f, otherBottomY = 0.0f, otherMinX = 0.0f, otherMaxX = 0.0f;
            GetCellSupportBounds(other, j, otherTopY, otherBottomY, otherMinX, otherMaxX);

            bool overlapsOther = cellMaxX > otherMinX && cellMinX < otherMaxX;
            bool isTouchingOther = cellBottomY >= otherTopY - Constants::SUPPORT_CHECK_TOLERANCE &&
                cellBottomY <= otherTopY + Constants::SUPPORT_CHECK_TOLERANCE;

            if (overlapsOther && isTouchingOther)
            {
                return true;
            }
        }
    }

    return false;
}

float PhysicsManager::CountCellsRestingOnBlock(Block* upper, Block* lower) const
{
    // [성능] BuildRestingChildrenMap이 모든 블록 쌍(n²)에 대해 이 함수를 부르므로, 명백히 멀리 떨어진
    // 쌍은 칸 단위 모서리 계산 없이 블록 원점 Y거리만으로 먼저 걸러낸다 (IsCellSupported와 같은 이유)
    if (std::fabs(upper->GetRenderPosition().y - lower->GetRenderPosition().y) > Constants::SUPPORT_BROADPHASE_MAX_BLOCK_EXTENT)
    {
        return 0.0f;
    }

    float restingOverlapWidth = 0.0f;

    for (int i = 0; i < upper->GetCellCount(); ++i)
    {
        float cellTopY = 0.0f, cellBottomY = 0.0f, cellMinX = 0.0f, cellMaxX = 0.0f;
        GetCellSupportBounds(upper, i, cellTopY, cellBottomY, cellMinX, cellMaxX);

        for (int j = 0; j < lower->GetCellCount(); ++j)
        {
            float lowerTopY = 0.0f, lowerBottomY = 0.0f, lowerMinX = 0.0f, lowerMaxX = 0.0f;
            GetCellSupportBounds(lower, j, lowerTopY, lowerBottomY, lowerMinX, lowerMaxX);

            // 여기도 마찬가지로 upper의 바닥은 lower의 "윗면"과 비교해야 한다
            // [근본 수정 — GetCellSupportRange와 같은 이유] 칸 중심이 아래 블록 위에 있어야만(절반 초과)
            // "얹혀 있다"고 인정하던 이분법을 걷어내고, 두 칸의 X 범위가 조금이라도 겹치면 얹힌 것으로
            // 본다. 실제 물리에서는 살짝만 걸쳐도(칼날 위 접촉이라도) 그 무게가 고스란히 아래로
            // 전달돼야 한다. 서브셀(반칸) 단위 착지의 "정확히 50%" 걸침에서 등록 여부가 뒤집히던 것도
            // 같이 없어진다.
            bool overlapsLower = cellMaxX > lowerMinX && cellMinX < lowerMaxX;
            bool isRestingOnLower = cellBottomY >= lowerTopY - Constants::SUPPORT_CHECK_TOLERANCE &&
                cellBottomY <= lowerTopY + Constants::SUPPORT_CHECK_TOLERANCE;

            if (overlapsLower && isRestingOnLower)
            {
                // [분배 정밀화 — 칸 개수 대신 겹친 폭] 예전엔 여기서 개수만 1 늘렸는데, 그러면 살짝
                // 걸친 칸과 꽉 찬 칸이 무게 분배에서 똑같이 취급됐다. 실제로 겹친 X폭만큼만 더해서
                // 걸친 정도에 비례해 무게가 나뉘게 한다.
                restingOverlapWidth += (std::min)(cellMaxX, lowerMaxX) - (std::max)(cellMinX, lowerMinX);
                break; // 이 upper 칸은 이미 lower 위에 얹힌 걸로 셌으니, lower의 다른 칸과 또 겹쳐도 중복 세지 않는다
            }
        }
    }

    return restingOverlapWidth;
}

PhysicsManager::RestingChildrenMap PhysicsManager::BuildRestingChildrenMap() const
{
    RestingChildrenMap childrenOf;
    std::vector<Block*> allBlocks = BlockManager::GetInstance().GetAllBlocks();

    for (Block* child : allBlocks)
    {
        // Airborne 블럭은 애초에 아무 위에도 "얹혀" 있는 상태가 아니라 제외한다.
        // Toppling은 여기서 안 거른다 — 재귀 도중 BeginToppling으로 상태가 바뀔 수 있어서,
        // 그 필터는 AccumulateSupportedMass가 순회할 때 그 시점의 상태로 판단해야 한다.
        if (child->GetPhysicsState() == PhysicsState::Airborne)
        {
            continue;
        }

        // [무게 분배 — 여러 블록에 걸쳐 얹힌 경우의 중복 계산 방지, 실전에서 발견된 버그 수정] child가
        // 서로 다른 여러 base 위에 한 칸씩 나눠 걸쳐 있을 수 있다(예: L자가 서로 다른 두 블록 위에
        // 한 칸씩). 예전엔 "얹혀 있는지"만 boolean으로 봐서, 걸친 base 각각에 child의 무게 전체를
        // 그대로 등록했다 — 그러면 각 base가 "내가 child 전체 무게를 다 받친다"고 중복 계산해서,
        // 실제로는 두 base가 나눠 받쳐 충분히 안정적인 구조도 양쪽 다 불안정으로 오판했다. 여기서
        // child가 각 base와 얼마나 겹쳐 얹혀 있는지(겹친 폭) 먼저 다 재서, 총 겹친 폭 대비 비율만큼만
        // 그 base에 무게를 배분한다.
        // [분배 정밀화] 칸 개수 대신 폭을 쓰는 이유는 CountCellsRestingOnBlock 주석 참고 — 살짝
        // 걸친 base가 넓게 걸친 base와 똑같이 취급되던 문제를 없앤다.
        std::vector<std::pair<Block*, float>> basesWithOverlapWidth;
        float totalRestingWidth = 0.0f;
        for (Block* base : allBlocks)
        {
            if (base == child)
            {
                continue;
            }

            float overlapWidth = CountCellsRestingOnBlock(child, base);
            if (overlapWidth > 0.0f)
            {
                basesWithOverlapWidth.push_back({ base, overlapWidth });
                totalRestingWidth += overlapWidth;
            }
        }

        for (const std::pair<Block*, float>& entry : basesWithOverlapWidth)
        {
            float weightFraction = entry.second / totalRestingWidth;
            childrenOf[entry.first].push_back({ child, weightFraction });
        }
    }

    return childrenOf;
}

void PhysicsManager::AccumulateSupportedMass(Block* base, float weightFraction, std::vector<Block*>& visited,
    const RestingChildrenMap& childrenOf,
    float& outTotalMass, float& outWeightedX) const
{
    outTotalMass += base->GetMass() * weightFraction;
    // [무게 전달 — 콜리전과 안 싸우는 버전] 실제 m_position 대신 GetEffectiveWeightPositionWorld()를
    // 쓴다 — 기울어지는 중인 블록은 순수 회전만으로는 실제 위치가 거의 안 바뀌어서, 실제 위치만 보면
    // 위쪽이 아무리 기울어도 아래 블록이 전혀 못 느낀다. 이 함수는 m_position/충돌/렌더링은 전혀
    // 안 건드리고 "pivot을 축으로 지금 각도까지 진짜 swing했다면 어디 있을지"만 계산해서 돌려주므로,
    // 여기 무게 합산 용도로만 안전하게 쓸 수 있다(Block.h의 설명 참고).
    outWeightedX += base->GetMass() * weightFraction * base->GetEffectiveWeightPositionWorld().x;

    auto it = childrenOf.find(base);
    if (it == childrenOf.end())
    {
        return;
    }

    for (const std::pair<Block*, float>& entry : it->second)
    {
        Block* child = entry.first;
        float childWeightFraction = entry.second;
        bool alreadyVisited = std::find(visited.begin(), visited.end(), child) != visited.end();
        // Toppling(이미 무너지는 중)인 블럭은 더 이상 자기 무게를 아래로 전달하지 않는다고 취급 —
        // 넘어지고 있는 조각까지 계속 하중으로 잡으면, 그 조각이 떨어져 나가는 동안 아래쪽이 계속 불안정하다고 오판한다
        bool canRestOnBase = !alreadyVisited && child->GetPhysicsState() != PhysicsState::Toppling;

        if (canRestOnBase)
        {
            visited.push_back(child);
            AccumulateSupportedMass(child, weightFraction * childWeightFraction, visited, childrenOf, outTotalMass, outWeightedX);
        }
    }
}

bool PhysicsManager::ComputeSupportDebugInfo(Block* block, const RestingChildrenMap& childrenOf, float& outMinX, float& outMaxX, float& outCombinedComX, float& outCombinedMass) const
{
    bool hasSupport = false;

    for (int i = 0; i < block->GetCellCount(); ++i)
    {
        // [경계값 버그 방지] 칸의 전체 폭이 아니라 실제로 겹치는 지지 범위만 받는다 — IsCellSupported+
        // GetCellSupportBounds를 따로 쓰면 칸이 발판 가장자리에 절반만 걸쳐도 전체 폭이 지지 범위에
        // 들어가버려서 무게중심 비교가 실제보다 관대해진다(GetCellSupportRange 주석 참고).
        float cellSupportMinX = 0.0f, cellSupportMaxX = 0.0f;
        if (!GetCellSupportRange(block, i, cellSupportMinX, cellSupportMaxX))
        {
            continue;
        }

        if (!hasSupport)
        {
            outMinX = cellSupportMinX;
            outMaxX = cellSupportMaxX;
            hasSupport = true;
        }
        else
        {
            if (cellSupportMinX < outMinX) outMinX = cellSupportMinX;
            if (cellSupportMaxX > outMaxX) outMaxX = cellSupportMaxX;
        }
    }

    if (!hasSupport)
    {
        return false;
    }

    // [연쇄 붕괴] block 자신의 무게중심이 아니라, block이 떠받치고 있는 전체(자신 + 위에 얹힌 모든 블럭)의
    // 결합 무게중심을 block 자신의 지지 범위와 비교한다. 이래야 위에 삐딱하게 얹힌 블럭 하나 때문에
    // 아래 블럭까지 실제로 넘어져야 하는 상황이 정확히 반영된다.
    std::vector<Block*> visited = { block };
    float totalMass = 0.0f;
    float weightedX = 0.0f;
    AccumulateSupportedMass(block, 1.0f, visited, childrenOf, totalMass, weightedX);
    outCombinedComX = weightedX / totalMass;
    outCombinedMass = totalMass;
    return true;
}

bool PhysicsManager::ResolveBalance(Block* block, float deltaTime, const RestingChildrenMap& childrenOf)
{
    float supportMinX = 0.0f;
    float supportMaxX = 0.0f;
    float centerOfMassX = 0.0f;
    float combinedMass = 0.0f;

    if (!ComputeSupportDebugInfo(block, childrenOf, supportMinX, supportMaxX, centerOfMassX, combinedMass))
    {
		if (block->GetPhysicsState() == PhysicsState::Sleeping)
		{
			LogFreezeDebug("[WAKE] no-support " + DescribeBlock(block));
			block->WakeUp();
		}
		else if (block->GetSpeedSquared() < Constants::SLEEP_LINEAR_THRESHOLD * Constants::SLEEP_LINEAR_THRESHOLD
			&& g_stepCounter % 60 == 0)
		{
			// [안전망] Awake인데 공식 지지는 없고 속도까지 낮은 상태가 계속되면(대략 1초에 한 번씩만 기록,
			// 스팸 방지) — 자유낙하라면 곧 속도가 붙어야 정상이라, 이게 반복해서 찍히면 뭔가(예: 아직
			// 못 잡은 다른 마찰/충돌 경로)가 계속 붙잡고 있다는 뜻이다.
			LogFreezeDebug("[STUCK?] awake-no-support-low-speed " + DescribeBlock(block));
		}

        // [공식 지지 없이 몇 초씩 "떠있는" 채로 얼어붙는 버그 수정 — 실전 로그로 확인됨] 이 블록이
        // BuildRestingChildrenMap의 느슨한 겹침 판정(CountCellsRestingOnBlock, 각도 무관)으로는 여전히
        // 누군가의 "자식"으로 등록돼 있어서(=진짜로 뭔가에 물리적으로 닿아 있어서) Step() 2.5단계의
        // GROUNDED_VELOCITY_DAMPING이 계속 걸리는데, 그 받침(예: 40도 넘게 기울어 무너지는 중인 이웃)이
        // IsNearAxisAlignedAngle(5도) 기준을 넘어서 "공식적으로 못 믿을 지지면"이 되면 여기(!ComputeSupportDebugInfo)
        // 로 빠져서 ResolveBalance가 이 블록에 대해선 아무 것도 안 하고 매번 조용히 빠져나간다 — 그 결과
        // 실제로는 닿아있는 채로 감쇠만 계속 받아서 중력을 거의 못 이기고, 수 초씩 거의 제자리에서 아주
        // 느리게 미끄러지기만 하는(physics_debug.log에서 5초 넘게 Y좌표가 8px밖에 안 바뀌는 것으로 확인)
        // "떠있는 정지" 상태가 된다. m_activeTimer(Awake로 전환된 뒤 안 끊기고 누적되는 시간)가
        // NO_SUPPORT_TOPPLE_TIMEOUT을 넘으면, 자유낙하로 이어지길 기다리는 대신 그냥 BeginToppling()으로
        // 직접 넘겨서 순수 중력+충돌(감쇠 없음)로 실제 떨어지게 한다 — Toppling 상태는 이미 Step() 2.5단계
        // 감쇠에서 제외되어 있으므로(위 주석 참고) 이 damping 함정 자체에서 완전히 벗어난다.
        if (block->GetPhysicsState() == PhysicsState::Awake &&
            block->GetActiveTimer() >= Constants::NO_SUPPORT_TOPPLE_TIMEOUT)
        {
            LogFreezeDebug("[TOPPLING] no-support-timeout " + DescribeBlock(block));
            block->BeginToppling();
            return true;
        }

        // [지지 flicker로 영원히 안 풀리는 불균형 버그 수정 — 실전 로그로 확인됨] 여기서 예전처럼
        // ResetImbalanceTimer()로 즉시 0을 만들면 안 된다 — 세게 부딪혀 각속도가 큰 블록은 흔들리는
        // 도중 회전된 모서리가 SUPPORT_CHECK_TOLERANCE 판정 창을 한두 프레임만 살짝 벗어나도 여기로
        // 들어오는데, 매번 타이머를 통째로 날리면 TOPPLE_STUCK_TIMEOUT(3초)까지 절대 못 쌓여서
        // [IMBALANCE-START]만 몇 초 넘게 계속 재시작하며 제자리에서 계속 흔들리기만 한다(physics_debug.log에서
        // L자 블록이 9초 넘게 이 패턴으로 멈추지도 넘어지지도 않는 것으로 확인됨). DecayImbalanceTimer와
        // 같은 이유(선언부 주석 참고)로 여기도 이 프레임 분(deltaTime)만 깎는다 — 진짜 자유낙하처럼 여러
        // 프레임 연속으로 지지가 없으면 어차피 쌓인 시간만큼 서서히 깎여 결국 0에 도달해 똑같이 리셋되고,
        // 한두 프레임짜리 순간 flicker는 누적 시간을 거의 잃지 않는다.
        block->DecayImbalanceTimer(deltaTime);

        return false;
    }

    // [임시 디버그 — combinedMass가 프레임마다 뒤집히는 문제 추적] 이미 불균형 진행 중인 블록에 한해,
    // 이번 프레임에 "내 위에 얹혀있다"고 카운트된 직계 자식 목록을 그대로 남긴다. combinedMass가
    // 프레임마다 널뛰면 여기서 자식이 프레임마다 나타났다 사라졌다 하는 걸 바로 볼 수 있다.
    if (block->GetImbalanceTimer() > 0.0f)
    {
        auto childrenIt = childrenOf.find(block);
        std::ostringstream oss;
        oss << "children=[";
        if (childrenIt != childrenOf.end())
        {
            for (const std::pair<Block*, float>& entry : childrenIt->second)
            {
                oss << entry.first << "(w=" << entry.second << ") ";
            }
        }
        oss << "]";
        LogFreezeDebug("[CHILDREN-TRACE] " + oss.str() + " " + DescribeBlock(block));
    }

    // [무게 계산 완충 — Block::SmoothCombinedMass/SmoothCombinedComX 선언부 주석 참고] raw로 계산된
    // combinedMass/centerOfMassX를 곧바로 쓰지 않고, 지수 이동평균으로 부드럽게 이은 값으로 바꿔치기
    // 한다. 이 아래로는(불균형 판정/pivot 방향/토크 계산 전부) 전부 이 완충된 값을 그대로 쓰게 되므로,
    // 판정 기준이 항상 서로 일치한다. GetImbalanceTimer()<=0(아직 이번 프레임에 AdvanceImbalanceTimer가
    // 안 불린 시점 기준)이면 "새 에피소드 시작"으로 보고 raw 값 그대로 스냅한다.
    bool isNewEpisodeForSmoothing = block->GetImbalanceTimer() <= 0.0f;
    combinedMass = block->SmoothCombinedMass(combinedMass, isNewEpisodeForSmoothing, deltaTime);
    centerOfMassX = block->SmoothCombinedComX(centerOfMassX, isNewEpisodeForSmoothing, deltaTime);

    // [진짜 중력 토크] 순간적으로 각속도를 꽂아넣던 예전 방식(TUMBLE_ANGULAR_VELOCITY 킥) 대신, 무게중심이
    // 지지 범위를 벗어난 채로 있는 한 매 프레임 실제 중력 토크(ApplyGravityTorque)를 계속 걸어준다.
    // 처음엔 지렛대(pivot~무게중심 거리)가 짧아 천천히 기울다가, 기울수록 지렛대가 길어지며 점점 빨리
    // 넘어가는 진짜 물리가 된다. 완전히 못 버티는 각도(MAX_TOPPLE_ANGLE)를 넘어서야 Toppling으로 확정한다.
    // [칼날 위 균형 방지] 가장자리를 넘어야(구식) 불안정으로 보는 대신, 가장자리에서 안쪽으로
    // IMBALANCE_DEADZONE만큼도 못 들어와 있으면(가장자리에 걸치기만 해도) 불안정으로 본다 —
    // 지지 칸이 하나뿐이고 무게중심이 그 가장자리에 정확히 걸리는 칼날 위 균형(예: S자를 세로로 세워
    // 모서리 하나로만 받치는 경우)까지 "가장자리를 안 넘었으니 안정"으로 영원히 통과하는 걸 막는다.
    // [얇은 지지 폭 방지] GetClampedImbalanceDeadzone 주석 참고 — 지지 폭이 좁으면 데드존도 줄여서,
    // 무게중심이 정중앙이어도 절대 균형으로 판정될 수 없던 버그를 막는다.
    float imbalanceDeadzone = GetClampedImbalanceDeadzone(supportMinX, supportMaxX);
    bool isImbalanced = centerOfMassX < supportMinX + imbalanceDeadzone ||
        centerOfMassX > supportMaxX - imbalanceDeadzone;

    if (!isImbalanced)
    {
        // [불균형 깜빡임 방지] 여기서 곧바로 ResetImbalanceTimer()로 완전히 0을 만들면 안 된다 — 경계선
        // 근처에서 무게중심이 한두 프레임만 안쪽으로 들어왔다 나가는 "깜빡임"에도 매번 완전히 리셋되면,
        // 실제로는 몇 초째 제자리인 블록도 TOPPLE_STUCK_TIMEOUT이 연속 3초를 못 채워서 끼임을 영원히
        // 못 잡는다(DecayImbalanceTimer 선언부 주석 참고, 실전 확인). 진짜로 오래 안정되면 자연히
        // 0 밑으로 내려가 결국 똑같이 리셋된다.
        block->DecayImbalanceTimer(deltaTime);
        return false;
    }

    // [무한 재시도 방지 — 실전에서 발견된 버그] 지난번에 같은(안 풀린) 불균형 때문에 이미 한 번
    // TOPPLE_STUCK_TIMEOUT을 다 채우고 ForceStabilize된 적이 있으면, 지지 상황이 안 바뀐 채로 그대로
    // 둔다 — 다시 깨워봤자 또 같은 시간을 들여 같은 결론(끼임)에 도달할 뿐이고, 매 프레임 재시도하면
    // Sleep<->WakeUp이 무한 반복된다(physics_debug.log에서 확인됨). 콜리전 등 다른 이유로 WakeUp되면
    // 이 판정과 무관하게 다음 프레임에 지지 상황이 새로 평가된다.
    if (block->IsWedged())
    {
        // 여전히 안 풀린 불균형 상태이므로 true — 위에 얹힌 것들은 계속 이 블록의 (멈춘) 붕괴에 묻어간다.
        return true;
    }

    // 이 지속시간(m_imbalanceTimer)은 TrySleepAll이 "지금 한창 넘어지는 중"인 블록을 각속도 임계값
    // 때문에 섣불리 재우거나 강제 정지시키지 않도록 참고한다 — 안 그러면 명백히 넘어져야 할 블록이
    // 조금씩만 진행하다 번번이 멈추고 끝내 못 넘어간다(실전 확인됨).
    // [pivot 고정 — 실전에서 발견된 방향 오류 버그] 이번 불균형이 "새로 시작"되는 프레임(각도가 아직
    // 거의 0에 가까울 때)인지를 먼저 판단해둔다. 매 프레임 그 순간의 회전된 발판(지지된 칸)에서 pivot을
    // 다시 뽑으면, 회전이 진행될수록 무게중심에서 먼 발판(특히 세로로 긴 I자처럼)이 무게중심을 축으로
    // 옆으로 크게 휩쓸리면서 pivot 선택 자체가 뒤집힐 수 있다 — 우연한 미세한 초기 흔들림 방향이
    // "이 방향이 맞다"고 스스로를 강화하는 피드백 루프가 되어, 실제로는 반대쪽으로 넘어가야 할 블록이
    // 그 흔들림 방향 그대로 가속돼버린다(I자 블록이 반대로 넘어가는 것으로 확인됨). 그래서 pivot은
    // 에피소드가 시작되는 이 한 프레임에만 새로 계산하고, 이후로는 각도가 얼마나 돌든 같은 pivot을 쓴다.
    bool isNewImbalanceEpisode = block->GetImbalanceTimer() <= 0.0f;
    if (isNewImbalanceEpisode)
    {
        // [불균형 시작점 추적 — 실전에서 필요성 확인] "지지는 있는데 무게중심이 범위를 벗어남"이 처음
        // 감지된 이 프레임을 남긴다. 이게 없으면, MAX_TOPPLE_ANGLE을 넘거나 다른 사건(충돌 등)에 우연히
        // 걸릴 때까지 매 프레임 조용히 토크만 쌓여서, 나중에 로그를 봐도 "왜 이미 이만큼(예: 18도)
        // 기울어진 채로 나타났는지" 그 시작 원인을 설명할 수가 없었다.
        std::ostringstream oss;
        oss << "supportRange=[" << supportMinX << "," << supportMaxX << "] centerOfMassX=" << centerOfMassX
            << " combinedMass=" << combinedMass;
        LogFreezeDebug("[IMBALANCE-START] " + oss.str() + " " + DescribeBlock(block));
    }
    block->AdvanceImbalanceTimer(deltaTime);

    // [잠든 채로 토크만 쌓이는 것 방지] Sleeping 블럭은 Step() 1단계(Integrate)를 안 타서 m_angle이
    // 갱신되지 않는다 — 여기서 WakeUp() 없이 ApplyGravityTorque만 걸면 m_angularVelocity가 화면엔
    // 안 보이는 채로 계속 쌓이다가, 나중에 뭔가(충돌 등) 딴 이유로 깨어나는 순간 그 쌓인 각속도가
    // 한꺼번에 반영되며 또 "휙" 튀어버린다. 불균형을 감지한 그 즉시 깨워서, 다음 프레임부터 Integrate가
    // 실제로 돌게 한다.
    bool wasSleeping = block->GetPhysicsState() == PhysicsState::Sleeping;

    // [Sleep 직후 즉시 재불안정 반복 방지 — 실전에서 발견된 무한 루프 버그] 결합 무게중심이 지지 범위
    // 경계선에 거의 정확히 걸친 "칼날 위 균형"에서는, 매번 정상적으로 Sleep까지 갔다가 Sleep()의 각도/
    // 위치 스냅이 그 경계를 살짝 다시 넘겨서 곧바로 새 에피소드가 시작되는 게 반복될 수 있다(m_imbalanceTimer
    // decay는 "같은 에피소드 안의 깜빡임"만 막아서 이 경우엔 무력함 — 매번 진짜로 Sleep을 완주하기
    // 때문). Sleeping에서 막 나온 블록이 새 에피소드를 이만큼 연속으로 시작하면, 그 미세한 잔여 기울기를
    // 그대로 받아들이고 더 재시도하지 않는다.
    if (isNewImbalanceEpisode && wasSleeping)
    {
        block->AdvanceSleepReawakenCount();
        if (block->GetSleepReawakenCount() >= Constants::MAX_SLEEP_REAWAKEN_COUNT)
        {
            LogFreezeDebug("[FORCESTABILIZE] sleep-reawaken-loop " + DescribeBlock(block));
            block->ForceStabilize();
            return true;
        }
    }

    if (wasSleeping)
    {
        block->WakeUp();
    }

    Vector2 pivot;
    if (isNewImbalanceEpisode)
    {
        // [화면 좌표계(Y 아래로 증가) 기준] 무게중심이 벗어난 방향(넘어가는 방향)의 반대쪽, 즉 지지
        // 범위에서 그 방향의 가장자리를 pivot으로 삼는다. 그 가장자리를 실제로 담당하는 지지 칸을 찾아서
        // 그 칸의 바닥 Y를 피벗의 Y로 쓴다.
        // [반칸 걸침 블록이 자꾸 오른쪽으로만 넘어가던 버그 수정] 예전엔 여기가 `centerOfMassX >=
        // supportMinX`였는데, 이건 위 isImbalanced 판정의 왼쪽 조건(`< supportMinX + DEADZONE`)과
        // 기준이 다르다 — 무게중심이 DEADZONE 안쪽(예: supportMinX보다 1px만 큰 값)에 있어서 "왼쪽으로
        // 불안정"으로 막 판정됐어도, `>= supportMinX` 자체는 참이라 tippingRight가 true가 돼버려서
        // 실제로는 왼쪽으로 넘어가야 할 블록이 오른쪽으로 넘어갔다. 반칸(서브셀) 단위로 착지한 블록은
        // 지지 범위 경계에 딱 걸치는 경우가 많아 이 DEADZONE 안쪽 구간에 자주 들어가고, 그때마다 전부
        // 오른쪽으로만 쏠렸던 것 — 실전에서 관찰된 "반칸 걸치면 오른쪽으로 도는 경향"과 정확히 일치한다.
        // isImbalanced의 오른쪽 조건과 완전히 같은 기준으로 판정해야 방향이 일관된다.
        // [레이캐스트로 방향 확정 — "반칸 걸치면 항상 오른쪽으로 넘어간다"는 실전 재현 버그 수정]
        // 예전엔 무게중심(centerOfMassX)이 supportMaxX - DEADZONE 경계의 어느 쪽에 있는지로만 방향을
        // 정했다. 반칸(정확히 50%) 단위로 걸친 경우 무게중심이 그 경계에 거의 딱 붙어버리는데, 여러
        // 단계를 거쳐 계산된 값(칸 회전→AABB→클리핑→가중평균)이라 부동소수점 오차가 어느 쪽으로
        // 튈지 예측하기 어렵고, 실전에서 "항상 오른쪽"처럼 한쪽으로 쏠려 보이는 원인이 됐다.
        // 진짜 물리적으로 맞는 기준은 "내 칸이 지지 범위보다 더 튀어나간(=바닥이 없는) 쪽"이므로,
        // 지지 중인 칸들의 raw 왼쪽/오른쪽 모서리에서 직접 레이캐스트를 쏴서 그 모서리 바로 너머에
        // 진짜로 바닥이 없는지 확인한다 — 이건 "겹침 몇 %"가 아니라 "거기 뭔가 있냐 없냐"라는 이분법
        // 질문이라 정확히 50%여도 흔들리지 않는다.
        bool leftEdgeUnsupported = false;
        bool rightEdgeUnsupported = false;
        for (int i = 0; i < block->GetCellCount(); ++i)
        {
            if (!IsCellSupported(block, i))
            {
                continue;
            }
            float cellTopY = 0.0f, cellBottomY = 0.0f, cellMinX = 0.0f, cellMaxX = 0.0f;
            GetCellSupportBounds(block, i, cellTopY, cellBottomY, cellMinX, cellMaxX);

            float unusedHitDistance = 0.0f;
            if (!RaycastDownForSupport({ cellMinX, cellBottomY }, block, unusedHitDistance))
            {
                leftEdgeUnsupported = true;
            }
            if (!RaycastDownForSupport({ cellMaxX, cellBottomY }, block, unusedHitDistance))
            {
                rightEdgeUnsupported = true;
            }
        }

        // 정확히 한쪽 모서리에서만"바닥 없음"이 확인되면 그쪽으로 확정한다. 양쪽 다 지지되거나
        // (다른 칸이 만드는 불균형처럼, 이 칸 자체는 안 넘어가는 경우) 양쪽 다 안 되는(드문) 경우엔
        // 판단할 근거가 없으므로 기존 무게중심 비교로 되돌아간다.
        bool tippingRight = (leftEdgeUnsupported != rightEdgeUnsupported)
            ? rightEdgeUnsupported
            : (centerOfMassX > supportMaxX - Constants::IMBALANCE_DEADZONE);
        float pivotX = tippingRight ? supportMaxX : supportMinX;

        // [레이캐스트로 pivot Y 확정 — 실전에서 발견된 버그 수정] 예전엔 "내 칸의 raw 모서리(cellMaxX/
        // cellMinX)가 pivotX와 오차 0.01px 이내로 정확히 같은 칸"을 찾아서 그 칸의 바닥 Y를 썼다. 그런데
        // pivotX는 "지지대 쪽" 모서리(예: 아래 블록의 실제 폭)라서, 내 칸 폭이 지지대와 정확히 안 맞으면
        // (칸이 지지대보다 넓거나 위치가 살짝 어긋나면) 못 찾고 pivotY가 기본값(FLOOR_TOP_Y)에 그대로
        // 남았다 — 그러면 ApplyGravityTorque의 평행축 정리(r.x²+r.y²)가 실제보다 훨씬 먼 거리로 계산돼
        // 회전 저항(관성모멘트)이 폭증한다. 토크 "방향"은 맞아서 넘어가야 할 쪽으로 밀리긴 하는데, 저항이
        // 너무 커서 거의 못 느낄 정도로 느리게 돈다 — 탑 위쪽 블록이 넘어가야 할 방향으로 안 넘어가는
        // 것처럼 보이던 원인. pivotX가 속한 내 칸을 찾아 그 칸 바닥에서 직접 레이캐스트를 쏴서 "진짜로
        // 맞은 지지면의 Y"를 쓰면, 폭이 어긋나든 말든 항상 정확한 pivotY를 얻는다.
        float pivotY = Constants::FLOOR_TOP_Y;
        for (int i = 0; i < block->GetCellCount(); ++i)
        {
            float cellTopY = 0.0f, cellBottomY = 0.0f, cellMinX = 0.0f, cellMaxX = 0.0f;
            GetCellSupportBounds(block, i, cellTopY, cellBottomY, cellMinX, cellMaxX);
            // pivotX가 이 칸의 가로 범위 안에 있어야(=이 칸이 그 지지 지점을 담당해야) 레이캐스트 후보로 삼는다
            if (pivotX < cellMinX - 0.01f || pivotX > cellMaxX + 0.01f)
            {
                continue;
            }

            float hitDistance = 0.0f;
            if (RaycastDownForSupport({ pivotX, cellBottomY }, block, hitDistance))
            {
                pivotY = cellBottomY + hitDistance;
                break;
            }
        }
        pivot = { pivotX, pivotY };
        block->SetImbalancePivot(pivot);
    }
    else
    {
        pivot = block->GetImbalancePivot();
    }

    block->ApplyGravityTorque(pivot, centerOfMassX, combinedMass, deltaTime);

    // [임시 디버그] 왜 토크가 걸려도 각속도가 안 붙는지 프레임별로 추적하기 위한 임시 로그.
    // 원인 확정되면 지운다.
    {
        std::ostringstream traceOss;
        traceOss << "pivot=(" << pivot.x << "," << pivot.y << ") combinedComX=" << centerOfMassX
            << " combinedMass=" << combinedMass << " angle=" << block->GetAngle()
            << " angularVel=" << block->GetAngularVelocity() << " v=" << std::sqrt(block->GetSpeedSquared());
        LogFreezeDebug("[IMBALANCE-TRACE] " + traceOss.str() + " " + DescribeBlock(block));
    }

    // [연쇄 붕괴 전파] block 하나만 넘어뜨린다. block이 떠받치던 덩어리 전체에 같은 각속도를 강제로
    // 주지 않는다 — 블록마다 무게중심/관성모멘트가 달라서 같은 각속도를 받아도 다르게 움직여야
    // 정상이다. 위에 얹힌 블록들은 직접 건드리지 않는다: IsCellSupported가 Toppling 블록을 지지
    // 제공자로 인정하지 않으므로, 다음 물리 스텝에 그 블록들이 각자 자기 지지 판정에서 "지지를
    // 잃었다"를 스스로 감지해 개별적으로 깨어나고(Sleeping->Awake) 자연스러운 충돌로 무너진다.
    // [절대각이 아니라 기준각 대비] Step()의 안전장치와 같은 이유로, 이미 기울어져 있던 기준각에서
    // 얼마나 더 돌았는지를 본다 — 한 번 크게 넘어져서 옆으로 누운 블록이 나중에 또 살짝 불균형해질
    // 때도 절대각이 아니라 그 눕혀진 자세를 기준으로 40도를 새로 재도록.
    if (std::fabs(block->GetAngle() - block->GetRestAngleReference()) >= Constants::MAX_TOPPLE_ANGLE)
    {
        // [무한 재넘어짐 방지] 옆 블록 등에 막혀서 실제로는 못 넘어가는 블록은, SettleToppledBlocks가
        // Awake로 돌려보내자마자 다시 토크를 받아 곧 같은 각도로 되돌아가길 반복할 수 있다. Sleep
        // 없이 이 재확정이 MAX_CONSECUTIVE_TOPPLE_COUNT번을 넘으면, 물리적으로 완전히 안정된 상태가
        // 아니어도 그 자리에서 강제로 멈춰서 무한 반복을 끊는다.
        if (block->GetConsecutiveToppleCount() >= Constants::MAX_CONSECUTIVE_TOPPLE_COUNT)
        {
            LogFreezeDebug("[FORCESTABILIZE] max-consecutive-topple " + DescribeBlock(block));
            block->ForceStabilize();
        }
        else
        {
            LogFreezeDebug("[TOPPLING] imbalance " + DescribeBlock(block));
            block->BeginToppling();
        }
    }
    else if (block->GetImbalanceTimer() >= Constants::TOPPLE_STUCK_TIMEOUT)
    {
        // [끼임 판정] 계속 불균형인데도 이만큼(3초) 시간 동안 MAX_TOPPLE_ANGLE을 못 넘었으면, 옆
        // 블록이나 바닥 모서리에 진짜로 끼여서 더는 못 넘어가는 것으로 보고 강제로 멈춘다. "포기" 표시는
        // ForceStabilize() 내부에서 일괄 처리한다(어느 호출 경로에서 왔든 동일하게 적용되도록).
        // [오뚜기처럼 도로 일어서는 버그 수정 — SettleToppledBlocks와 같은 안전장치 필요] 이 아래
        // SettleToppledBlocks의 같은 목적 체크(끼임 판정)는 "3초가 지났다"만으로 판단하지 않고
        // hasStoppedMoving(실제로 지금 거의 안 움직이는지)까지 같이 봐야 한다는 걸 이미 겪어서 알고 있다
        // (해당 주석 참고) — 그런데 여기(넘어지기 *전* 단계)엔 그 가드가 빠져있었다. 지렛대가 아직 짧아
        // 토크가 작은 초반엔 무게중심이 살짝만(예: 9도) 기운 채로 아주 천천히 계속 가속 중일 수 있는데,
        // 그것도 "3초 동안 40도를 못 넘었다"는 이유만으로 여기 걸려 강제로 멈춰버렸다. 그 상태로
        // Sleep()이 각도 스냅(90도 배수 12도 이내면 반올림)까지 적용해버리면, 아직 진행 중이던 미세한
        // 기울기가 한 프레임 만에 0도로 확 되돌아가 "오뚜기처럼 벌떡 일어나는" 것처럼 보인다. 실제로
        // 각속도가 이미 거의 0(=진짜로 못 넘어가고 멈춘 상태)일 때만 끼임으로 확정한다.
        bool hasStoppedMoving = block->GetSpeedSquared() < Constants::SLEEP_LINEAR_THRESHOLD * Constants::SLEEP_LINEAR_THRESHOLD &&
            std::fabs(block->GetAngularVelocity()) < Constants::SLEEP_ANGULAR_THRESHOLD;
        if (hasStoppedMoving)
        {
            LogFreezeDebug("[FORCESTABILIZE] topple-stuck-timeout " + DescribeBlock(block));
            block->ForceStabilize();
        }
    }

    return true;
}

void PhysicsManager::RemoveToppledBlocks()
{
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        if (block->GetPhysicsState() != PhysicsState::Toppling)
        {
            continue;
        }

        // 블록이 화면 아래(바닥)로 완전히 벗어났는지 체크
        bool isBelowWindow = block->GetRenderPosition().y > Constants::WINDOW_HEIGHT;

        // [원인 제거]
        // 순간적으로 마찰에 걸려 속도가 0이 되었을 때 삭제되는 일이 없도록
        // hasStoppedMoving 조건을 과감히 지웁니다.

        if (isBelowWindow)
        {
            BlockManager::GetInstance().RemoveBlock(block);
        }
    }
}
void PhysicsManager::SettleToppledBlocks(const RestingChildrenMap& childrenOf)
{
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        if (block->GetPhysicsState() != PhysicsState::Toppling)
        {
            continue;
        }

        // 화면 밖으로 떨어지는 중이면 계속 Toppling으로 둬야 RemoveToppledBlocks가 나중에 치운다
        bool isBelowWindow = block->GetRenderPosition().y > Constants::WINDOW_HEIGHT;
        if (isBelowWindow)
        {
            continue;
        }

        bool tooFast = block->GetSpeedSquared() > Constants::UNSTABLE_SPEED_THRESHOLD * Constants::UNSTABLE_SPEED_THRESHOLD;
        bool spinningTooFast = std::fabs(block->GetAngularVelocity()) > Constants::UNSTABLE_ANGULAR_SPEED_THRESHOLD;
        if (tooFast || spinningTooFast)
        {
            continue;
        }

        // 속도가 느려도 포물선 꼭대기(수직 속도가 잠깐 0에 가까워지는 순간)에 우연히 걸릴 수 있어서,
        // 실제로 바닥/다른 블럭에 닿아 있는지(지지 여부)까지 확인해야 공중에서 얼어붙는 걸 막을 수 있다
        bool hasSupport = false;
        for (int i = 0; i < block->GetCellCount() && !hasSupport; ++i)
        {
            if (IsCellSupported(block, i))
            {
                hasSupport = true;
            }
        }

        if (!hasSupport)
        {
            continue;
        }

        // [진짜 원인 수정] "칸 하나라도 지지됨"만으로 Awake로 돌려보내면, 회전하며 넘어지는 도중 다른
        // 모서리가 바닥에 잠깐 스치기만 해도 매번 여기 걸려서 Awake로 튕겨나갔다가, 바로 다음 스텝
        // ResolveBalance가 결합 무게중심을 보고 다시 넘어뜨리는 게 반복된다 — 진짜 넘어지는 중인 블록의
        // 낙하가 계속 끊기고(눈에는 "재넘어짐"으로 보임), 결국 MAX_CONSECUTIVE_TOPPLE_COUNT에 걸려
        // 넘어져야 할 자세 그대로 ForceStabilize에 얼어붙는 원인이었다. 그래서 ResolveBalance와 완전히
        // 같은 기준(결합 무게중심이 지지 범위 안쪽으로 IMBALANCE_DEADZONE만큼 여유가 있는지)까지 확인해서,
        // 진짜로 안정된 경우에만 Awake로 돌려보낸다 — 아직 불안정하면 Toppling 상태를 그대로 유지해서
        // 실제 물리(중력+충돌)로 자연스럽게 계속 넘어지게 둔다.
        float supportMinX = 0.0f;
        float supportMaxX = 0.0f;
        float centerOfMassX = 0.0f;
        float unusedCombinedMass = 0.0f;
        if (!ComputeSupportDebugInfo(block, childrenOf, supportMinX, supportMaxX, centerOfMassX, unusedCombinedMass))
        {
            continue;
        }

        // [얇은 지지 폭 방지] ResolveBalance와 반드시 같은 기준을 써야 한다 — GetClampedImbalanceDeadzone
        // 주석 참고. 안 그러면 얇게 걸친 착지에서 이 함수는 "절대 안정 아님"으로, ResolveBalance는
        // (클램프 덕분에) "안정"으로 서로 다르게 판단해 Toppling<->Awake를 오갈 수 있다.
        float imbalanceDeadzone = GetClampedImbalanceDeadzone(supportMinX, supportMaxX);
        bool isBalanced = centerOfMassX >= supportMinX + imbalanceDeadzone &&
            centerOfMassX <= supportMaxX - imbalanceDeadzone;
        if (isBalanced)
        {
            // [무한 진동 버그 수정] 지금 각도(완전히 넘어가서 90/135도처럼 누운 자세일 수 있음)를 새
            // 안정 기준각으로 확정해둔다 — 안 그러면 Awake로 돌아가자마자 Step()의 절대각 안전장치가
            // "여전히 40도 넘음"으로 보고 즉시 다시 BeginToppling()을 불러서 Toppling<->Awake를 매
            // 프레임 무한 반복하며 제자리에서 떤다(physics_debug.log로 실전 확인됨).
            block->ConfirmRestingAngle();
            block->WakeUp();
            continue;
        }

        // [끼임 방지] 균형을 못 찾았어도, 옆 블럭이나 바닥 모서리에 끼어서 실제로는 더 못 넘어가는
        // 채로 TOPPLE_STUCK_TIMEOUT을 넘겨 계속 Toppling에 머물러 있으면 강제로 멈춘다 — 안 그러면
        // 위 hasSupport/isBalanced 두 탈출 조건을 영원히 못 만족해서 그 자세 그대로 무한정 얼어붙는다.
        // [애매한 각도에서 멈추는 문제 방지 — 실전 확인됨] 다만 "3초가 지났다"만으로 판단하면, 울퉁불퉁한
        // 지형을 타고 느리지만 실제로 계속 굴러 내려가는 중인 블록까지 도중의 애매한 대각선 각도에서
        // 멈춰버린다. 그래서 지금도 속도가 낮아(진짜로 멈춘 상태) 있을 때만 끼임으로 본다 — 아직
        // 움직이고 있으면 3초가 지났어도 계속 진행하게 둔다.
        bool hasStoppedMoving = block->GetSpeedSquared() < Constants::SLEEP_LINEAR_THRESHOLD * Constants::SLEEP_LINEAR_THRESHOLD &&
            std::fabs(block->GetAngularVelocity()) < Constants::SLEEP_ANGULAR_THRESHOLD;
        if (block->GetActiveTimer() >= Constants::TOPPLE_STUCK_TIMEOUT && hasStoppedMoving)
        {
            LogFreezeDebug("[FORCESTABILIZE] settle-stuck-timeout " + DescribeBlock(block));
            block->ForceStabilize();
        }
    }
}

void PhysicsManager::TrySleepAll(float deltaTime)
{
    for (Block* block : BlockManager::GetInstance().GetAllBlocks())
    {
        if (block->GetPhysicsState() != PhysicsState::Awake)
        {
            continue;
        }

        // [진짜 넘어지는 중 보호] ResolveBalance가 이번 스텝에 무게중심 불균형을 감지해서 실제 중력
        // 토크를 걸고 있는 중이면(IsWedged가 아직 아니라면, 즉 아직 "포기"하지 않은 진행 중인 상태면)
        // 여기서 각속도 임계값 기준으로 재우거나 강제 정지시키지 않는다. 넘어지는 도중의 정상적인 회전
        // 가속을 "떨림"으로 착각해서 매번 끊어버리면, 명백히 넘어져야 할 블록이 조금씩만 진행하다
        // 번번이 멈춰서 결국 못 넘어가는 문제가 생긴다(실전 확인됨) — "진짜로 끼여서 못 넘어가는" 판정과
        // 강제 정지는 ResolveBalance의 m_imbalanceTimer(TOPPLE_STUCK_TIMEOUT)가 전담한다. 끼임으로
        // 판정돼 이미 포기한(IsWedged) 블록은 여기서 정상적으로 처리되도록 그대로 둔다.
        if (block->GetImbalanceTimer() > 0.0f && !block->IsWedged())
        {
            continue;
        }

        bool tooFast = block->GetSpeedSquared() > Constants::SLEEP_LINEAR_THRESHOLD * Constants::SLEEP_LINEAR_THRESHOLD;
        bool spinningTooFast = std::fabs(block->GetAngularVelocity()) > Constants::SLEEP_ANGULAR_THRESHOLD;

        bool hasSupport = false;
        for (int i = 0; i < block->GetCellCount() && !hasSupport; ++i)
        {
            if (IsCellSupported(block, i))
            {
                hasSupport = true;
            }
        }

        bool canRest = hasSupport && !tooFast && !spinningTooFast;

        if (!canRest)
        {
            block->ResetRestTimer();

            // [강제 취침 타임아웃] 지지대는 있는데 미세한 진동 때문에 속도가 SLEEP 임계값을 계속
            // 살짝살짝 넘나들면, 위의 정상 경로(rest timer가 SLEEP_DELAY만큼 안 끊기고 유지)로는
            // 영원히 잠들 수 없다 — 눈에는 거의 멈춘 것처럼 보이는데 계속 Awake로 남는 경우가 이거다.
            // m_activeTimer(Land()/BeginToppling() 이후 경과 시간, Sleep<->Wake 반복에도 안 끊김)가
            // FORCE_SLEEP_TIMEOUT을 넘으면 속도와 무관하게 강제로 안정화한다.
            if (hasSupport && block->GetActiveTimer() >= Constants::FORCE_SLEEP_TIMEOUT)
            {
                LogFreezeDebug("[FORCESTABILIZE] force-sleep-timeout " + DescribeBlock(block));
                block->ForceStabilize();
            }

            continue;
        }

        block->AdvanceRestTimer(deltaTime);

        if (block->GetRestTimer() >= Constants::SLEEP_DELAY)
        {
            // [무한 재넘어짐 방지] 여기가 "진짜로 안정돼서 스스로 잠들었다"고 확인된 유일한 경로다
            // (지지대 있음 + 저속 상태가 SLEEP_DELAY 동안 유지됨) — 재넘어짐 카운트를 여기서만 리셋한다.
            LogFreezeDebug("[SLEEP] rest-timer " + DescribeBlock(block));
            block->ResetConsecutiveToppleCount();
            block->Sleep();
        }
    }
}

