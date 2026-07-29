#include "pch.h"

#include "ResourceManager.h"

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
