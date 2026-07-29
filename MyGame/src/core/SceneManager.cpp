#include "pch.h"

#include "SceneManager.h"
#include "Scene.h"

SceneManager& SceneManager::GetInstance()
{
    static SceneManager instance;
    return instance;
}

SceneManager::~SceneManager() = default;

void SceneManager::ChangeScene(SceneType type)
{
    // TODO: type에 맞는 TitleScene/PlayScene/GameOverScene 생성 및 Exit/Enter 호출 직접 구현
    (void)type;
}

void SceneManager::Update(float deltaTime)
{
    if (m_currentScene)
    {
        m_currentScene->Update(deltaTime);
    }
}

void SceneManager::Render()
{
    if (m_currentScene)
    {
        m_currentScene->Render();
    }
}
