#pragma once

#include "../util/Types.h"

// 화면에 실제로 그리는 역할만 담당하는 싱글톤.
// 다른 매니저의 상태를 읽기만 하며, 게임 로직을 갖지 않는다.
class RenderManager
{
public:
    static RenderManager& GetInstance();

    void DrawSpriteRotated(const SpriteInfo& sprite, float angle);
    void Render();

private:
    RenderManager() = default;
    ~RenderManager() = default;
    RenderManager(const RenderManager&) = delete;
    RenderManager& operator=(const RenderManager&) = delete;
};
