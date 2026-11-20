#ifndef MODEL_H
#define MODEL_H

#include<glad/glad.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>

#include <glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include<assimp/Importer.hpp>
#include<assimp/scene.h>
#include<assimp/postprocess.h>
#include<assimp/mesh.h>
#include<assimp/material.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


struct BoneInfo
{
	glm::mat4 BoneOffset;
	glm::mat4 FinalTransformation;
};

class Model {
public:
	std::vector<glm::vec3> vPositions;
	std::vector<glm::vec2> vTexCoords;
	std::vector<glm::vec3> vNormals;
	std::vector<GLuint> vIndices;

	GLuint diffuseMap = 0, specularMap = 0;
	float shininess = 0.0f;
	glm::vec3 defaultAmbient = glm::vec3(1.0f), defaultDiffuse = glm::vec3(1.0f), defaultSpecular = glm::vec3(1.0f);

private:
	GLuint VAO = 0;
	GLuint VBO_positions = 0, VBO_texCoords = 0, VBO_normals = 0;
	GLuint EBO = 0;

protected:
	bool useDiffuseMap = false, useSpecularMap = false;


public:
	// 생성자
	// TODO: [Architecture] Anti-pattern: Two-step initialization (Constructor + init()).
	// Refactoring plan: Merge init() into the constructor to apply RAII pattern.
	// IMPORTANT: Ensure the model object is instantiated ONLY AFTER the GLFW OpenGL context is fully created.
	Model() {
		shininess = 32.f;
		useDiffuseMap = false;
		useSpecularMap = false;

		defaultAmbient = glm::vec3(1.0f, 1.0f, 1.0f);
		defaultDiffuse = glm::vec3(1.0f, 1.0f, 1.0f);
		defaultSpecular = glm::vec3(0.0f, 0.0f, 0.0f);
		// TODO: [Dependency] Remove sb7 legacy math library (vmath::vec3).
		// Replace all vmath occurrences with industry-standard GLM (glm::vec3).
	}
	// 소멸자
	virtual ~Model() {
		glDeleteTextures(1, &diffuseMap);
		glDeleteTextures(1, &specularMap);

		glDeleteBuffers(1, &EBO);
		glDeleteBuffers(1, &VBO_positions);
		glDeleteBuffers(1, &VBO_texCoords);
		glDeleteBuffers(1, &VBO_normals);
		glDeleteVertexArrays(1, &VAO);
	}

	virtual void init() {
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO_positions);
		glGenBuffers(1, &VBO_texCoords);
		glGenBuffers(1, &VBO_normals);
		glGenBuffers(1, &EBO);

		glGenTextures(1, &diffuseMap);
		glGenTextures(1, &specularMap);
	}

	//change into smart pointer(prevent dangling memories, dangerous leaking)
	void setupMesh(int _numVertices, GLfloat* _vPositions, GLfloat* _vTexCoords = NULL, GLfloat* _vNormals = NULL) {
		// 1. copy data from arrays
		for (int i = 0; i < _numVertices; i++) {
			glm::vec3 position;
			position[0] = _vPositions[i * 3 + 0];
			position[1] = _vPositions[i * 3 + 1];
			position[2] = _vPositions[i * 3 + 2];
			this->vPositions.push_back(position);
		}

		if (_vTexCoords) {
			for (int i = 0; i < _numVertices; i++) {
				glm::vec2 texCoords;
				texCoords[0] = _vTexCoords[i * 2 + 0];
				texCoords[1] = _vTexCoords[i * 2 + 1];
				this->vTexCoords.push_back(texCoords);
			}
		}

		if (_vNormals) {
			for (int i = 0; i < _numVertices; i++) {
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

	void setupIndices(int _numIndices, GLuint* _indices) {
		for (int i = 0; i < _numIndices; i++) {
			vIndices.push_back(_indices[i]);
		}

		glBindVertexArray(VAO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, _numIndices * sizeof(int), &vIndices[0], GL_STATIC_DRAW);
		glBindVertexArray(0);
	}

	virtual bool loadDiffuseMap(const char* _filepath) {
		if (loadTextureFile(diffuseMap, _filepath)) {
			useDiffuseMap = true;
			return true;
		}

		useDiffuseMap = false;
		return false;
	}

	bool loadSpecularMap(const char* _filepath) {
		if (loadTextureFile(specularMap, _filepath)) {
			useSpecularMap = true;
			return true;
		}

		useSpecularMap = false;
		return false;
	}

	virtual void draw(GLuint _shaderID, double currentTime) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glUniform3fv(glGetUniformLocation(_shaderID, "material.defaultAmbient"), 1, glm::value_ptr(defaultAmbient));
		glUniform3fv(glGetUniformLocation(_shaderID, "material.defaultDiffuse"), 1, glm::value_ptr(defaultDiffuse));
		glUniform3fv(glGetUniformLocation(_shaderID, "material.defaultSpecular"), 1, glm::value_ptr(defaultSpecular));
		glUniform1i(glGetUniformLocation(_shaderID, "material.useDiffuseMap"), (int)useDiffuseMap);
		glUniform1i(glGetUniformLocation(_shaderID, "material.useSpecularMap"), (int)useSpecularMap);

		glUniform1i(glGetUniformLocation(_shaderID, "useNormal"), (int)vNormals.size());
		glUniform1f(glGetUniformLocation(_shaderID, "material.shininess"), shininess);
		//Solve CPU bottle-neck.


		if (useDiffuseMap) {
			glUniform1i(glGetUniformLocation(_shaderID, "material.diffuse"), 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, diffuseMap);
		}

		if (useSpecularMap) {
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
	// TODO: [Modern C++] Legacy C-style string parameter (const char*).
	// Refactoring plan: Upgrade to C++17 std::string_view (or const std::string&) 
	// for better memory safety and performance. Use .c_str() only when 
	// interfacing with C-API boundaries like fopen() or Assimp.
	virtual void loadFBX(std::string objectFileName, std::vector<std::string> animationFileName) {}

	virtual void setCurrentAnimationId(std::string animationName) {}

protected:
	virtual void prepareBuffers() {
		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, VBO_positions);
		glBufferData(GL_ARRAY_BUFFER, vPositions.size() * sizeof(glm::vec3), &vPositions[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

		if (!vTexCoords.empty()) {
			glBindBuffer(GL_ARRAY_BUFFER, VBO_texCoords);
			glBufferData(GL_ARRAY_BUFFER, vTexCoords.size() * sizeof(glm::vec2), &vTexCoords[0], GL_STATIC_DRAW);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
		}

		if (!vNormals.empty()) {
			glBindBuffer(GL_ARRAY_BUFFER, VBO_normals);
			glBufferData(GL_ARRAY_BUFFER, vNormals.size() * sizeof(glm::vec3), &vNormals[0], GL_STATIC_DRAW);
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	bool loadTextureFile(GLuint textureID, const char* filepath) {
		int width, height, nrChannels;
		unsigned char* data = stbi_load(filepath, &width, &height, &nrChannels, 0);

		if (data) {
			GLenum format = 0;
			if (nrChannels == 1)
				format = GL_RED;
			else if (nrChannels == 3) // R, G, B (ex. jpg)
				format = GL_RGB;
			else if (nrChannels == 4) // PNG
				format = GL_RGBA;

			glBindTexture(GL_TEXTURE_2D, textureID);

			glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			stbi_image_free(data);
			return true;
		}

		stbi_image_free(data);
		return false;
	}

};

#endif 