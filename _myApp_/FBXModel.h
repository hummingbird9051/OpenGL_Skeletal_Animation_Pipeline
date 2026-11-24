#pragma once
#include<glad/glad.h>
#include<string>
#include<vector>
#include<unordered_map>
#define GLM_ENABLE_EXPERIMENTAL
#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include "Material.h"

struct aiScene;
struct aiNode;

struct VertexJointWeights
{
	std::vector<float> weight;
	std::vector<int> jointIndex;
};

class FBXModel
{
private:
	struct Animation {
		struct Channel {
			struct VectorKey {
				double time;
				glm::vec3 value;
			};
			struct QuaternionKey {
				double time;
				glm::quat value;
			};

			std::string name;
			std::vector<VectorKey> positionKeys;
			std::vector<QuaternionKey> rotationKeys;
			std::vector<VectorKey> scalingKeys;
		};
		double duration = 0.0;
		double tickperSec = 0.0;
		std::vector<Channel> channels;
		std::unordered_map<std::string, int> channelIndexByName;
		std::vector<int> nodeToChannel;
	};

	struct Node {
		std::string name;
		glm::vec3 translate = glm::vec3(0.0f);
		glm::quat rotate = glm::quat(1, 0, 0, 0);
		glm::vec3 scale = glm::vec3(1.0f);
		int parent = -1;
		std::vector<int> children;
	};

	struct Skin {
		std::vector<int> jointNodeIds;
		std::vector<glm::mat4> inverseBindMatrices;
	};

	struct ShaderInfo {
		GLuint shaderId = 0;

		GLint ulDefaultAmbient = 0;
		GLint ulDefaultDiffuse = 0;
		GLint ulDefaultSpecular = 0;
		GLint ulUseDiffuseMap = 0;
		GLint ulUseSpecularMap = 0;
		GLint ulUseNormal = 0;
		GLint ulShininess = 0;
		GLint ulDiffuse = 0;
	};

	ShaderInfo shaderInfo;

	std::vector<Animation> animations;
	std::unordered_map<std::string, int> findAnimation;

	int currentAnimationId = 1;
	int lastAnimationId = 1;

	double currentAnimStartedTime = 0.0;
	double lastAnimStartedTime = 0.0;
	double transitionLength = 0.5;

	std::vector<Node> nodes;

	glm::mat4 globalInverseRootTransform = glm::mat4(1.0f);
	
	Material material;

	std::vector<GLuint> meshesVAO;
	std::vector<GLuint> meshesVBOPosition;
	std::vector<GLuint> meshesVBONormal;
	std::vector<GLuint> meshesVBOTexCoord;
	std::vector<GLuint> meshesVBOJointIndices;
	std::vector<GLuint> meshesVBOJointWeights;
	std::vector<GLuint> meshesEBO;
	GLuint skinningUBO = 0;
	GLuint SSBOId = 0;

	std::vector<GLuint> meshesTexture;

	std::vector<Skin> meshSkins;
	std::vector<std::vector<glm::mat4>> meshSkinningMatrices; // Skinning value with each meshes joints

	std::vector<std::vector<glm::vec3>> positionMesh;
	std::vector<std::vector<glm::vec2>> texCoordMesh;
	std::vector<std::vector<glm::vec3>> normalMesh;
	std::vector<std::vector<GLuint>> vIndicesMesh;
	std::vector<std::vector<VertexJointWeights>> jointWeightsMesh;					//relationship with vertex and joint

	std::vector<glm::mat4> nodeGlobalTransforms; //global transform per node (indexed by node), used by updatePose()

	int modelAmount = 0;

	int loadAnimation(const aiScene* scene, const std::string& originalFileName);

public:
	FBXModel();

	~FBXModel();

	void init();

	// TODO: Texture paths are hardcoded per mesh. Read them from the FBX materials.
	bool loadDiffuseMap(const char* texFile);

	void loadFBX(const std::string& objectFileName, const std::vector<std::string>& animationFileName);

	void updateAnimation(double currentTime);

	void setModelMatrices(const std::vector<glm::mat4>& matrices);

	void draw(GLuint _shaderID);

	void setCurrentAnimationId(const std::string& animationName, double currentTime);

private:
	void prepareBuffers();

	void setShaderInfo(GLuint _shaderId);

	void updatePose(double lastAnimTick, double currentAnimTick, double w);

	int saveNodes(const aiNode* cur, int parentInfo);
	
	glm::vec3 pickKey(const std::vector<Animation::Channel::VectorKey>& keys, double tickTime);

	glm::quat pickKey(const std::vector<Animation::Channel::QuaternionKey>& keys, double tickTime);
};