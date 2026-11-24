#pragma once
#include<glm/glm.hpp>

struct Material {
	bool useDiffuseMap = false, useSpecularMap = false;
	float shininess = 0.0f;
	glm::vec3 defaultAmbient = glm::vec3(1.0f), defaultDiffuse = glm::vec3(1.0f), defaultSpecular = glm::vec3(1.0f);
};



