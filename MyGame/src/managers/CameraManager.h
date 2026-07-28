#pragma once

#include <vector>
#include "../util/Types.h"

class Block;

// 카메라 위치와 월드-스크린 좌표 변환을 담당하는 싱글톤
class CameraManager
{
public:
    static CameraManager& GetInstance();

    // 블럭 목록을 보고 카메라가 따라갈 목표 y값을 갱신
    void UpdateTarget(const std::vector<Block*>& blocks);

    void Update(float deltaTime);
    Vector2 WorldToScreen(Vector2 worldPos) const;

private:
    CameraManager() = default;
    ~CameraManager() = default;
    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    Vector2 m_position;
    float m_targetY = 0.0f;
};
