#pragma once
#include <unordered_map>
#include <string>
#include "Model.h"

namespace jhb {
	class Scene {
	public:
		
		void AddToScene(const std::string& typeName, const Model& primitive)
		{
			if (Primitives.find(typeName) == Primitives.end())
			{
				Primitives.insert(std::unordered_map<std::string, Model>::value_type(typeName, primitive));
			}
		}

		bool AddInstanceToModel(std::string ModelType)
		{
			if (Primitives.find(ModelType) == Primitives.end())
			{
				return false;
			}

			Model& primitive = Primitives[ModelType];

			primitive.instanceCount++;

		}

	private:
		std::unordered_map<std::string, Model> Primitives;
	};
}