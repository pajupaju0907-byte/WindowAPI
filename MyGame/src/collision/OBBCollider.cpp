#include "pch.h"

#include "OBBCollider.h"
#include "../util/MathUtil.h"
#include <algorithm>
#include <cmath>

namespace
{
    // corners를 axis(단위벡터) 위에 투영했을 때의 최소/최대값을 구하는 SAT 보조 함수
    void ProjectCornersOntoAxis(const Vector2 corners[4], Vector2 axis, float& outMin, float& outMax)
    {
        outMin = corners[0].x * axis.x + corners[0].y * axis.y;
        outMax = outMin;
        for (int i = 1; i < 4; ++i)
        {
            float projection = corners[i].x * axis.x + corners[i].y * axis.y;
            if (projection < outMin) outMin = projection;
            if (projection > outMax) outMax = projection;
        }
    }

    // corners 네 점의 중심(단순 평균) — 정사각형이라 이게 곧 기하학적 중심이다
    Vector2 ComputeQuadCenter(const Vector2 corners[4])
    {
        Vector2 sum = { 0.0f, 0.0f };
        for (int i = 0; i < 4; ++i)
        {
            sum += corners[i];
        }
        return sum * 0.25f;
    }
}

// [책임 분리] 원래 PhysicsManager::TestCellCollision에 있던 SAT 구현을 그대로 옮겨왔다.
// SAT(분리축 정리): 두 볼록 도형이 안 겹친다면, 두 도형의 변 중 적어도 하나에 수직인 방향(축)으로
// 투영했을 때 두 도형의 투영 구간이 겹치지 않는 축이 반드시 존재한다.
// 반대로 모든 후보 축에서 투영이 겹친다면 두 도형은 실제로 겹쳐 있다는 뜻이다.
// 사각형은 마주보는 변끼리 평행이라 축은 실질적으로 2개씩(총 4개)이면 충분하지만,
// 코드를 단순하게 유지하려고 그냥 8개(양쪽 다 4변씩) 다 검사한다.
OBBCollisionResult TestOBBCollision(const Vector2 cornersA[4], const Vector2 cornersB[4])
{
    OBBCollisionResult result;
    float smallestOverlap = 999999.0f;
    Vector2 smallestAxis = { 0.0f, 0.0f };

    for (int shapeIndex = 0; shapeIndex < 2; ++shapeIndex)
    {
        const Vector2* corners = (shapeIndex == 0) ? cornersA : cornersB;

        for (int edgeIndex = 0; edgeIndex < 4; ++edgeIndex)
        {
            Vector2 edgeStart = corners[edgeIndex];
            Vector2 edgeEnd = corners[(edgeIndex + 1) % 4];
            Vector2 edge = edgeEnd - edgeStart;

            // 변을 90도 돌리면 그 변에 수직인 방향(분리축 후보)이 된다
            Vector2 axis = { edge.y, -edge.x };
            float axisLength = std::sqrt(axis.x * axis.x + axis.y * axis.y);
            if (axisLength < 0.0001f)
            {
                continue;
            }
            axis = axis * (1.0f / axisLength);

            float minA, maxA, minB, maxB;
            ProjectCornersOntoAxis(cornersA, axis, minA, maxA);
            ProjectCornersOntoAxis(cornersB, axis, minB, maxB);

            float overlapEnd = std::min(maxA, maxB);
            float overlapStart = std::max(minA, minB);
            float overlap = overlapEnd - overlapStart;

            if (overlap <= 0.0f)
            {
                // 이 축에서 안 겹친다 = 분리축 발견 = 두 사각형은 충돌하지 않는다 (result.collided는 기본값 false)
                return result;
            }

            if (overlap < smallestOverlap)
            {
                smallestOverlap = overlap;
                smallestAxis = axis;
            }
        }
    }

    // 모든 축에서 겹쳤다 = 충돌. 겹침이 가장 작은 축이 "밀어내야 할 방향"(최소 이동 벡터)이 된다
    result.collided = true;
    result.penetration = smallestOverlap;

    // normal은 항상 "B에서 A를 향하는 방향"으로 통일한다
    Vector2 centerA = ComputeQuadCenter(cornersA);
    Vector2 centerB = ComputeQuadCenter(cornersB);
    Vector2 aFromB = centerA - centerB;
    float dot = aFromB.x * smallestAxis.x + aFromB.y * smallestAxis.y;
    result.normal = (dot < 0.0f) ? smallestAxis * -1.0f : smallestAxis;

    // [접촉 다각화] B의 꼭짓점 4개를 normal 방향(=A가 있는 쪽)으로 얼마나 깊이 들어갔는지 기준으로 정렬해서,
    // 가장 깊은 2개를 접촉점으로 삼는다.
    float bestProjection = -999999.0f;
    float secondProjection = -999999.0f;
    int bestIndex = 0;
    int secondIndex = 0;

    for (int i = 0; i < 4; ++i)
    {
        float projection = cornersB[i].x * result.normal.x + cornersB[i].y * result.normal.y;
        if (projection > bestProjection)
        {
            secondProjection = bestProjection;
            secondIndex = bestIndex;
            bestProjection = projection;
            bestIndex = i;
        }
        else if (projection > secondProjection)
        {
            secondProjection = projection;
            secondIndex = i;
        }
    }

    result.contactPoints[0] = cornersB[bestIndex];
    result.contactPoints[1] = cornersB[secondIndex];

    return result;
}

OBBCollider::OBBCollider(Vector2 center, Vector2 halfExtents, float angleDegrees)
    : m_center(center)
    , m_halfExtents(halfExtents)
    , m_angleDegrees(angleDegrees)
{
}

void OBBCollider::GetCorners(Vector2 outCorners[4]) const
{
    // 로컬 좌표계(회전 전) 기준 네 꼭짓점 — 중심에서 halfExtents만큼 떨어진 사각형, 시계방향
    Vector2 localCorners[4] =
    {
        { -m_halfExtents.x, -m_halfExtents.y },
        {  m_halfExtents.x, -m_halfExtents.y },
        {  m_halfExtents.x,  m_halfExtents.y },
        { -m_halfExtents.x,  m_halfExtents.y },
    };

    float radians = MathUtil::DegreesToRadians(m_angleDegrees);
    float cosAngle = static_cast<float>(std::cos(radians));
    float sinAngle = static_cast<float>(std::sin(radians));

    for (int i = 0; i < 4; ++i)
    {
        Vector2 local = localCorners[i];
        Vector2 rotated
        {
            local.x * cosAngle - local.y * sinAngle,
            local.x * sinAngle + local.y * cosAngle
        };
        outCorners[i] = m_center + rotated;
    }
}

AABB OBBCollider::GetBounds() const
{
    Vector2 corners[4];
    GetCorners(corners);

    Vector2 minPos = corners[0];
    Vector2 maxPos = corners[0];
    for (int i = 1; i < 4; ++i)
    {
        if (corners[i].x < minPos.x) minPos.x = corners[i].x;
        if (corners[i].y < minPos.y) minPos.y = corners[i].y;
        if (corners[i].x > maxPos.x) maxPos.x = corners[i].x;
        if (corners[i].y > maxPos.y) maxPos.y = corners[i].y;
    }

    return { minPos, maxPos };
}

OBBCollisionResult OBBCollider::Intersect(const OBBCollider& other) const
{
    Vector2 cornersA[4];
    Vector2 cornersB[4];
    GetCorners(cornersA);
    other.GetCorners(cornersB);
    return TestOBBCollision(cornersA, cornersB);
}

bool OBBCollider::CheckCollision(const Collider& other) const
{
    // Collider 인터페이스는 도형 종류를 모르므로, OBB끼리인지 런타임에 확인해야 한다.
    // 지금은 블럭 칸이 전부 OBBCollider라 다른 타입이 들어올 일이 없지만, 인터페이스 계약상 방어적으로 처리한다.
    const OBBCollider* otherObb = dynamic_cast<const OBBCollider*>(&other);
    if (otherObb == nullptr)
    {
        return false;
    }
    return Intersect(*otherObb).collided;
}
