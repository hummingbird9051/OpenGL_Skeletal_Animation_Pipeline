#include "Model.h"
#include<glm/gtc/type_ptr.hpp>
#include "Texture.h"
#include<iostream>

Model::Model() {
	material.shininess = 32.f;
	material.useDiffuseMap = false;
	material.useSpecularMap = false;

	material.defaultAmbient = glm::vec3(1.0f, 1.0f, 1.0f);
	material.defaultDiffuse = glm::vec3(1.0f, 1.0f, 1.0f);
	material.defaultSpecular = glm::vec3(0.0f, 0.0f, 0.0f);
}

Model::~Model() {
	glDeleteTextures(1, &diffuseMap);
	glDeleteTextures(1, &specularMap);

	glDeleteBuffers(1, &EBO);
	glDeleteBuffers(1, &VBO_positions);
	glDeleteBuffers(1, &VBO_texCoords);
	glDeleteBuffers(1, &VBO_normals);
	glDeleteVertexArrays(1, &VAO);
}

void Model::init() {
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO_positions);
	glGenBuffers(1, &VBO_texCoords);
	glGenBuffers(1, &VBO_normals);
	glGenBuffers(1, &EBO);

	glGenTextures(1, &diffuseMap);
	glGenTextures(1, &specularMap);
}

void Model::setupMesh(const std::vector<GLfloat>& _vPositions, const std::vector<GLfloat>& _vTexCoords, const std::vector<GLfloat>& _vNormals) {
	if (_vPositions.size() % 3 != 0) {
		std::cerr << "Position Array error" << std::endl;
		return;
	}
	if (_vTexCoords.size() % 2 != 0) {
		std::cerr << "Texture Coordinate Array error" << std::endl;
		return;
	}
	if (_vNormals.size() % 3 != 0) {
		std::cerr << "Normal Array error" << std::endl;
		return;
	}
	// 1. copy data from arrays
	int numVertices = static_cast<int>(_vPositions.size()) / 3;
	for (int i = 0; i < numVertices; i++) {
		glm::vec3 position;
		position[0] = _vPositions[i * 3 + 0];
		position[1] = _vPositions[i * 3 + 1];
		position[2] = _vPositions[i * 3 + 2];
		this->vPositions.push_back(position);
	}

	if (!_vTexCoords.empty()) {
		for (int i = 0; i < numVertices; i++) {
			glm::vec2 texCoords;
			texCoords[0] = _vTexCoords[i * 2 + 0];
			texCoords[1] = _vTexCoords[i * 2 + 1];
			this->vTexCoords.push_back(texCoords);
		}
	}

	if (!_vNormals.empty()) {
		for (int i = 0; i < numVertices; i++) {
			glm::vec3 normal;
			normal[0] = _vNormals[i * 3 + 0];
			normal[1] = _vNormals[i * 3 + 1];
			normal[2] = _vNormals[i * 3 + 2];
			this->vNormals.push_back(normal);
		}
	}

	// 2. create buffers
	prepareBuffers();

}

void Model::setupIndices(const std::vector<GLuint>& _indices) {
	int numIndices = static_cast<int>(_indices.size());
	for (int i = 0; i < numIndices; i++) {
		vIndices.push_back(_indices[i]);
	}

	glBindVertexArray(VAO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * sizeof(GLuint), &vIndices[0], GL_STATIC_DRAW);
	glBindVertexArray(0);
}

bool Model::loadDiffuseMap(const std::string& _filepath) {
	if (loadTextureFile(diffuseMap, _filepath.c_str())) {
		material.useDiffuseMap = true;
		return true;
	}

	material.useDiffuseMap = false;
	return false;
}

bool Model::loadSpecularMap(const std::string& _filepath) {
	if (loadTextureFile(specularMap, _filepath.c_str())) {
		material.useSpecularMap = true;
		return true;
	}

	material.useSpecularMap = false;
	return false;
}

void Model::draw(GLuint _shaderID, double currentTime) {
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glUniform3fv(glGetUniformLocation(_shaderID, "material.defaultAmbient"), 1, glm::value_ptr(material.defaultAmbient));
	glUniform3fv(glGetUniformLocation(_shaderID, "material.defaultDiffuse"), 1, glm::value_ptr(material.defaultDiffuse));
	glUniform3fv(glGetUniformLocation(_shaderID, "material.defaultSpecular"), 1, glm::value_ptr(material.defaultSpecular));
	glUniform1i(glGetUniformLocation(_shaderID, "material.useDiffuseMap"), static_cast<int>(material.useDiffuseMap));
	glUniform1i(glGetUniformLocation(_shaderID, "material.useSpecularMap"), static_cast<int>(material.useSpecularMap));

	glUniform1i(glGetUniformLocation(_shaderID, "useNormal"), static_cast<int>(vNormals.size()));
	glUniform1f(glGetUniformLocation(_shaderID, "material.shininess"), material.shininess);
	//Solve CPU bottle-neck.


	if (material.useDiffuseMap) {
		glUniform1i(glGetUniformLocation(_shaderID, "material.diffuse"), 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, diffuseMap);
	}

	if (material.useSpecularMap) {
		glUniform1i(glGetUniformLocation(_shaderID, "material.specular"), 1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, specularMap);
	}

	glBindVertexArray(VAO);
	if (vIndices.empty())
		glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vPositions.size()));
	else
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(vIndices.size()), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);


}

void Model::prepareBuffers() {
	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO_positions);
	glBufferData(GL_ARRAY_BUFFER, vPositions.size() * sizeof(glm::vec3), &vPositions[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));

	if (!vTexCoords.empty()) {
		glBindBuffer(GL_ARRAY_BUFFER, VBO_texCoords);
		glBufferData(GL_ARRAY_BUFFER, vTexCoords.size() * sizeof(glm::vec2), &vTexCoords[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
	}

	if (!vNormals.empty()) {
		glBindBuffer(GL_ARRAY_BUFFER, VBO_normals);
		glBufferData(GL_ARRAY_BUFFER, vNormals.size() * sizeof(glm::vec3), &vNormals[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}