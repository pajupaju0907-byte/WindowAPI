#include "pch.h"

#include "ResourceManager.h"

#include <gdiplus.h>

ResourceManager& ResourceManager::GetInstance()
{
    static ResourceManager instance;
    return instance;
}

void ResourceManager::LoadSprite(const std::string& path)
{
	wstring widPath(path.begin(), path.end());
	auto bitmap = std::make_shared<Gdiplus::Bitmap>(widPath.c_str());
    
    SpriteInfo info;
	info.id = path;
	info.bitmap = bitmap;
	
    m_sprites[path] = info;


}

void ResourceManager::LoadJson(const std::string& path)
{
    // TODO: 실제 JSON 파싱 로직 직접 구현
    (void)path;
}

const SpriteInfo& ResourceManager::GetSpriteInfo(const std::string& id) const
{
    // TODO: m_sprites에서 id로 조회하는 로직 직접 구현
    static const SpriteInfo emptySpriteInfo;
    auto it = m_sprites.find(id);
    return it != m_sprites.end() ? it->second : emptySpriteInfo;
}

void ResourceManager::Shutdown()
{
    // 명시적으로 모든 SpriteInfo의 shared_ptr을 해제하여
    // GDI+가 활성화된 상태에서 비트맵이 파괴되도록 보장합니다.
    for (auto& p : m_sprites) {
        p.second.bitmap.reset();
    }
    m_sprites.clear();
}
