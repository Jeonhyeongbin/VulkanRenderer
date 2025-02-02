#pragma once
#include "GameObject.h"

namespace jhb {
	class GameObjectManager
	{
	public:
		static GameObjectManager& GetSingleton()
		{
			if (!objectManager)
			{
				objectManager = new GameObjectManager();
			}
			return *objectManager;
		}

		void AddGameObject(const GameObject&);

		void GetGameObject(unsigned int);


	public:
		jhb::GameObject::Map gameObjects;
	private:
		static GameObjectManager* objectManager;
	};
}

