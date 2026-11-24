#pragma once
#include<glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include "Material.h"


class Model {
public:
	std::vector<glm::vec3> vPositions;
	std::vector<glm::vec2> vTexCoords;
	std::vector<glm::vec3> vNormals;
	std::vector<GLuint> vIndices;

	GLuint diffuseMap = 0, specularMap = 0;

	Material material;

private:
	GLuint VAO = 0;
	GLuint VBO_positions = 0, VBO_texCoords = 0, VBO_normals = 0;
	GLuint EBO = 0;



public:
	// TODO: Move init() into the constructor. Create the model only after the GL context exists.
	Model();

	~Model();

	void init();

	// TODO: Take std::vector instead of raw pointers and counts.
	void setupMesh(const std::vector<GLfloat>& _vPositions, const std::vector<GLfloat>& _vTexCoords = {}, const std::vector<GLfloat>& _vNormals = {});

	void setupIndices(const std::vector<GLuint>& _indices);

	bool loadDiffuseMap(const std::string& _filepath);

	bool loadSpecularMap(const std::string& _filepath);

	void draw(GLuint _shaderID, double currentTime);

private:
	void prepareBuffers();

};
 