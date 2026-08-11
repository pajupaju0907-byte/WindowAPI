#pragma once

#include <string>
#include <memory>
#include <d2d1.h>
#include <wrl/client.h>

// 여러 클래스가 공용으로 쓰는 기본 타입 모음.
// 다른 헤더보다 먼저 include되는 경우가 많으므로 이 파일 자체는 다른 게임 헤더를 include하지 않는다.

// 위치/속도 등에 공용으로 쓰이는 2차원 벡터
struct Vector2
{
    float x = 0.0f;
    float y = 0.0f;

    Vector2 operator+(const Vector2& other) const { return { x + other.x, y + other.y }; }
    Vector2 operator-(const Vector2& other) const { return { x - other.x, y - other.y }; }
    Vector2 operator*(float scalar) const { return { x * scalar, y * scalar }; }

    Vector2& operator+=(const Vector2& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vector2& operator-=(const Vector2& other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }
};

// 사각형 충돌 판정에 쓰이는 AABB (Axis-Aligned Bounding Box)
struct AABB
{
    Vector2 min;
    Vector2 max;
};

// 블럭의 물리 상태
enum class PhysicsState
{
    Airborne,  // 공중 상태: 그리드(서브셀) 기반 낙하 중, 물리 미적용
    Awake,     // 바닥 상태: 착지 직후, 물리 시뮬레이션(흔들림) 활성
    Sleeping,  // 바닥 상태: 안정화되어 물리 계산 스킵
    Toppling   // 무게중심이 무너져서 회전+낙하하며 이탈 중. 충돌 없이 화면 밖으로 나가면 제거된다
};

// TODO: 어떤 블럭 종류를 스폰할지 구분하는 용도.
// 실제 테트로미노 모양(I/O/T/S/Z/J/L 등) 구성은 게임 디자인 영역이므로 직접 정의할 것.
enum class BlockType
{
    Tetromino,
    GiantTetromino,
    Heavy
};
// 테트리스 블럭 모양 이넘
enum class TetrominoShape
{
    I, O, T, S, Z, J, L
};
// ResourceManager가 로드하고 RenderManager가 그리는 스프라이트 정보.
// TODO: 실제 표현 방식(비트맵 핸들, UV 좌표 등)은 렌더링 방식에 맞게 직접 설계할 것
struct SpriteInfo
{
    std::string id;
    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
};
