#pragma once

#include <string>
#include <unordered_map>
#include "../util/Types.h"

// 스프라이트/JSON 등 외부 리소스 로딩과 캐싱을 담당하는 싱글톤
class ResourceManager
{
public:
    static ResourceManager& GetInstance();

    void LoadSprite(const std::string& path);
    void LoadJson(const std::string& path);
    const SpriteInfo& GetSpriteInfo(const std::string& id) const;
    // 애플리케이션 종료 또는 리소스 재로딩 시 모든 로드된 리소스를 해제합니다.
    void Shutdown();

private:
    ResourceManager() = default;
    ~ResourceManager() = default;
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    std::unordered_map<std::string, SpriteInfo> m_sprites;
};
