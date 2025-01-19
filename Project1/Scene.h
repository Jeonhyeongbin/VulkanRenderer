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
				Primitives.emplace(typeName, primitive);
			}
			else
			{
				AddInstanceToModel(typeName);
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

		std::shared_ptr<Model> loadGLTFFile(const std::string& filename);

	private:
		std::unordered_map<std::string, Model> Primitives;
	};
}