#pragma once

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include <array>

struct frustum {
	std::array<glm::vec3, 8> vertices;
	std::array<glm::vec3, 5> face_normals;
	std::array<glm::vec3, 6> edge_directions;

	frustum(glm::mat4 const &view_projection);
};
