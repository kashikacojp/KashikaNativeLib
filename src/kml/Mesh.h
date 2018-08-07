#pragma once
#ifndef _KML_MESH_H_
#define _KML_MESH_H_

#include <vector>
#include <memory>
#include "Compatibility.h"
#include "SkinWeights.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace kml
{
	class Mesh
	{
	public:
		Mesh();
	public:
		std::string name;
		std::vector<unsigned char>			facenums;
		std::vector<int>					pos_indices;
		std::vector<int>					nor_indices;
		std::vector<int>					tex_indices;
		std::vector<glm::vec3>				positions;
		std::vector<glm::vec3>				normals;
		std::vector<glm::vec2>				texcoords;
		std::vector<int>			        materials;
		std::shared_ptr<SkinWeights>        skin_weights;
	};
}

#endif
