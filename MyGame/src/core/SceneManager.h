#pragma once

#include <memory>

class Scene;

// 씬 전환과 현재 씬의 Update/Render 위임을 담당하는 싱글톤
enum class SceneType
{
    Title,
    Play,
    GameOver
};

class SceneManager
{
public:
    static SceneManager& GetInstance();

    // 현재 씬을 Exit하고 새 씬으로 교체 후 Enter
    void ChangeScene(SceneType type);

    void Update(float deltaTime);
    void Render(HDC hdc);

private:
    SceneManager() = default;
    ~SceneManager();
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    std::unique_ptr<Scene> m_currentScene;
};
