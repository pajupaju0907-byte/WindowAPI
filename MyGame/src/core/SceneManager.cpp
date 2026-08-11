#include "pch.h"

#include "SceneManager.h"
#include "Scene.h"
#include "../scenes/TitleScene.h"
#include "../scenes/PlayScene.h"
#include "../scenes/GameOverScene.h"
SceneManager& SceneManager::GetInstance()
{
    static SceneManager instance;
    return instance;
}

SceneManager::~SceneManager() = default;

void SceneManager::ChangeScene(SceneType type)
{
    if (m_currentScene) m_currentScene->Exit();

    switch (type) 
    {
    case SceneType::Title:
        m_currentScene = make_unique<TitleScene>();
        break;
    case SceneType::Play:
        m_currentScene = make_unique<PlayScene>();
        break;
    case SceneType::GameOver:
        m_currentScene = make_unique<GameOverScene>();
        break;
    }
    if (m_currentScene) m_currentScene->Enter();
}

void SceneManager::Update(float deltaTime)
{
    if (m_currentScene)
    {
        m_currentScene->Update(deltaTime);
    }
}

void SceneManager::Render(ID2D1RenderTarget* renderTarget)
{
    if (m_currentScene)
    {
        m_currentScene->Render(renderTarget);
    }
}
