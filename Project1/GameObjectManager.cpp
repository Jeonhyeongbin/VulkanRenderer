#include "GameObjectManager.h"

jhb::GameObjectManager* jhb::GameObjectManager::objectManager = nullptr;

void jhb::GameObjectManager::AddGameObject(jhb::GameObject&& gameObject)
{
	gameObjects.emplace(gameObject.getId(), std::move(gameObject));
}

void jhb::GameObjectManager::GetGameObject(unsigned int id)
{

}


