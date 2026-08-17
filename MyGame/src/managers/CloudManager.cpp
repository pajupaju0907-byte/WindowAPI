#include "pch.h"

#include "CloudManager.h"
#include "CameraManager.h"
#include "../util/Constants.h"
#include "../util/MathUtil.h"
#include <cstdlib>
#include <string>

CloudManager& CloudManager::GetInstance()
{
    static CloudManager instance;
    return instance;
}

void CloudManager::Update(float deltaTime, float currentHeightMeters)
{
    // 기존 구름 이동 + 화면을 완전히 벗어난 구름 제거
    for (auto it = m_clouds.begin(); it != m_clouds.end(); )
    {
        it->position.x += it->velocityX * deltaTime;

        float halfSize = (Constants::CLOUD_BASE_SIZE * it->scale) / 2.0f;
        bool offScreen = (it->position.x + halfSize < 0.0f) ||
            (it->position.x - halfSize > static_cast<float>(Constants::WINDOW_WIDTH));

        if (offScreen)
        {
            it = m_clouds.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // 아직 스폰 조건(최소 높이)에 못 미치면 스폰 시도 자체를 하지 않는다
    if (currentHeightMeters < Constants::CLOUD_MIN_HEIGHT_METERS)
    {
        return;
    }

    m_spawnCheckTimer += deltaTime;
    if (m_spawnCheckTimer >= Constants::CLOUD_SPAWN_CHECK_INTERVAL)
    {
        m_spawnCheckTimer = 0.0f;
        TrySpawnCloud(currentHeightMeters);
    }
}

// currentHeightMeters를 [CLOUD_MIN_HEIGHT_METERS, CLOUD_HEIGHT_FOR_MAX_CHANCE] 구간에서
// [CLOUD_BASE_SPAWN_CHANCE, CLOUD_MAX_SPAWN_CHANCE]로 선형 보간한다
float CloudManager::CalculateSpawnChance(float currentHeightMeters) const
{
    float t = (currentHeightMeters - Constants::CLOUD_MIN_HEIGHT_METERS) /
        (Constants::CLOUD_HEIGHT_FOR_MAX_CHANCE - Constants::CLOUD_MIN_HEIGHT_METERS);
    t = MathUtil::Clamp(t, 0.0f, 1.0f);
    return MathUtil::Lerp(Constants::CLOUD_BASE_SPAWN_CHANCE, Constants::CLOUD_MAX_SPAWN_CHANCE, t);
}

void CloudManager::TrySpawnCloud(float currentHeightMeters)
{
    if (static_cast<int>(m_clouds.size()) >= Constants::CLOUD_MAX_COUNT)
    {
        return;
    }

    float roll = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    if (roll > CalculateSpawnChance(currentHeightMeters))
    {
        return;
    }

    Cloud cloud;

    // [중요] 스폰 여백은 반드시 이 구름 "자신의" scale로 계산해야 한다 — Update()의 화면 밖 제거
    // 판정도 각 구름의 실제 scale로 halfSize를 구하는데, 여기서 다른 기준(예: 항상 최대 크기)을
    // 쓰면 작게 뽑힌 구름은 자기 제거 경계보다 더 바깥에서 태어나 다음 프레임에 바로 삭제돼버린다.
    float scaleRange = Constants::CLOUD_MAX_SCALE - Constants::CLOUD_MIN_SCALE;
    cloud.scale = Constants::CLOUD_MIN_SCALE + static_cast<float>(rand()) / RAND_MAX * scaleRange;
    float halfSize = Constants::CLOUD_BASE_SIZE * cloud.scale / 2.0f;

    // 왼쪽/오른쪽 중 랜덤으로 한쪽 화면 밖에서 등장해 반대쪽으로 흘러간다
    bool fromLeft = (rand() % 2) == 0;
    cloud.position.x = fromLeft ? -halfSize : static_cast<float>(Constants::WINDOW_WIDTH) + halfSize;

    // 지금 카메라가 보여주고 있는 화면 세로 범위 안에서 랜덤한 높이에 등장시킨다
    cloud.position.y = CameraManager::GetInstance().GetPosition().y +
        static_cast<float>(rand() % Constants::WINDOW_HEIGHT);

    float speedRange = Constants::CLOUD_MAX_SPEED - Constants::CLOUD_MIN_SPEED;
    float speed = Constants::CLOUD_MIN_SPEED + static_cast<float>(rand()) / RAND_MAX * speedRange;
    cloud.velocityX = fromLeft ? speed : -speed;

    int spriteIndex = (rand() % 3) + 1; // cloud1~cloud3
    cloud.spriteId = "assets/cloud" + std::to_string(spriteIndex) + ".png";

    m_clouds.push_back(cloud);
}

const std::vector<Cloud>& CloudManager::GetClouds() const
{
    return m_clouds;
}

void CloudManager::Reset()
{
    m_clouds.clear();
    m_spawnCheckTimer = 0.0f;
}
