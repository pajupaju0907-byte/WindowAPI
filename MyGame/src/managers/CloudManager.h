#pragma once

#include <string>
#include <vector>
#include "../util/Types.h"

// 화면을 가로지르며 시야를 방해하는 반투명 구름 하나의 상태
struct Cloud
{
    Vector2 position;        // 월드 좌표(x는 화면 좌표와 동일하게 취급, y는 카메라를 따라 스크롤됨)
    float velocityX = 0.0f;  // 초당 이동 속도(px/s). 음수면 왼쪽, 양수면 오른쪽으로 이동
    float scale = 1.0f;      // CLOUD_BASE_SIZE에 곱해지는 크기 배율
    std::string spriteId;    // "assets/cloud1.png" 등 ResourceManager에 등록된 경로
};

// 탑이 일정 높이 이상 쌓이면 화면을 가로지르는 구름을 스폰/이동시키는 싱글톤.
// RenderManager 원칙(그리는 역할만 담당)을 지키기 위해, 실제로 그리는 건 PlayScene::Render에서
// GetClouds()를 순회하며 RenderManager::DrawSpriteRotated를 호출하는 방식으로 처리한다.
class CloudManager
{
public:
    static CloudManager& GetInstance();

    // deltaTime만큼 기존 구름을 이동/제거하고, currentHeightMeters를 기준으로 새 구름 스폰을 시도한다.
    void Update(float deltaTime, float currentHeightMeters);

    const std::vector<Cloud>& GetClouds() const;

    // 씬 재시작(재도전) 시 이전 판의 구름을 모두 비운다
    void Reset();

private:
    CloudManager() = default;
    ~CloudManager() = default;
    CloudManager(const CloudManager&) = delete;
    CloudManager& operator=(const CloudManager&) = delete;

    void TrySpawnCloud(float currentHeightMeters);
    float CalculateSpawnChance(float currentHeightMeters) const;

    std::vector<Cloud> m_clouds;
    float m_spawnCheckTimer = 0.0f;
};
