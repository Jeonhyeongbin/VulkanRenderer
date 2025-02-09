#include "GameObjectManager.h"

void jhb::GameObjectManager::AddGameObject(const jhb::GameObject& gameObject)
{
	gameObjects.emplace(gameObject.getId(), std::move(gameObject));
}

void jhb::GameObjectManager::GetGameObject(unsigned int id)
{

}


