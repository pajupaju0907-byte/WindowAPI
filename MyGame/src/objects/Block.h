#pragma once

#include <memory>
#include <string>
#include "../util/Types.h"

class Collider;

// 모든 블럭 종류(테트로미노/원형/거대/무거운 블럭)의 공통 베이스.
// 물리 적분(Integrate)과 힘 적용(ApplyForce)은 블럭 종류와 무관하게 동일하므로 여기서 구현하고,
// 모양/크기 등 데이터만 다른 부분은 파생 클래스의 필드로 처리한다.
class Block
{
public:
    virtual ~Block();

    void ApplyForce(Vector2 force);
    void Integrate(float deltaTime);
    void WakeUp();

protected:
    // unique_ptr<Collider>가 불완전 타입을 가리키므로, 기본 생성자는 반드시 .cpp(Collider가 완전한 타입인 곳)에서 정의한다.
    Block();

    int m_id = 0;
    Vector2 m_position;
    Vector2 m_velocity;
    float m_angle = 0.0f;
    float m_angularVelocity = 0.0f;
    float m_mass = 1.0f;
    PhysicsState m_physicsState = PhysicsState::Falling;
    std::unique_ptr<Collider> m_collider;
    std::string m_spriteId;
};
