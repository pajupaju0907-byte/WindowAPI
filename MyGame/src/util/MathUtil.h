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
}
