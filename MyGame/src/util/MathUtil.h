#pragma once

#include <algorithm>

// 도메인 로직과 무관한 범용 수치 계산 함수 모음
namespace MathUtil
{
    // value를 [minValue, maxValue] 범위로 제한
    inline float Clamp(float value, float minValue, float maxValue)
    {
        return std::max(minValue, std::min(value, maxValue));
    }

    // start와 end 사이를 t(0~1) 비율로 선형 보간
    inline float Lerp(float start, float end, float t)
    {
        return start + (end - start) * t;
    }

    // 도(degree)를 라디안으로 변환 (삼각함수는 라디안 단위 인자를 받으므로 필요)
    inline float DegreesToRadians(float degrees)
    {
        constexpr float PI = 3.14159265f;
        return degrees * (PI / 180.0f);
    }

    // 라디안을 도(degree)로 변환 (DegreesToRadians의 역변환).
    // 각속도 관련 물리 공식은 라디안 단위여야 정확해서, 계산은 라디안으로 하고 저장할 때만 도로 되돌리는 데 씀
    inline float RadiansToDegrees(float radians)
    {
        constexpr float PI = 3.14159265f;
        return radians * (180.0f / PI);
    }
}
