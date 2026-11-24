#pragma once
#include <iostream>
#include<map>
#include<filesystem>
#include "Model.h"
#include "CPUProfiler.h"
#include "GPUProfiler.h"
#define MAX_JOINTS 52 //Set the same as shader's MAX_JOINTS
using namespace std;


struct VertexJointWeights
{
	vector<float> weight;
	vector<int> jointIndex;
};

class FBXModel : public Model
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

			string name;
			vector<VectorKey> positionKeys;
			vector<QuaternionKey> rotationKeys;
			vector<VectorKey> scalingKeys;
		};
		double duration = 0.0;
		double tickperSec = 0.0;
		vector<Channel> channels;
		unordered_map<string, int> channelIndexByName;
		vector<int> nodeToChannel;
	};

	struct Node {
		string name;
		glm::vec3 translate = glm::vec3(1.0f);
		glm::quat rotate = glm::mat4(1.0f);
		glm::vec3 scale = glm::vec3(1.0f);
		int parent = -1;
		vector<int> children;
	};

	struct Skin {
		vector<int> jointNodeIds;
		vector<glm::mat4> inverseBindMatrices;
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

	vector<Animation> animations;
	unordered_map<string, int> findAnimation;

	int currentAnimationId = 1;
	int lastAnimationId = 1;

	double currentAnimStartedTime = 0.0;
	double lastAnimStartedTime = 0.0;
	double transitionLength = 0.5;

	vector<Node> nodes;

	glm::mat4 globalInverseRootTransform = glm::mat4(1.0f);

	vector<GLuint> meshesVAO;
	vector<GLuint> meshesVBOPosition;
	vector<GLuint> meshesVBONormal;
	vector<GLuint> meshesVBOTexCoord;
	vector<GLuint> meshesVBOBoneID;
	vector<GLuint> meshesVBOWeights;
	vector<GLuint> meshesEBO;
	GLuint skinningUBO = 0;
	GLuint SSBOId = 0;

	vector<GLuint> meshesTexture;

	vector<Skin> meshSkins;
	vector<vector<glm::mat4>> meshSkinningMatrices; // Skinning value with each meshes joints

	vector<vector<glm::vec3>> positionMesh;
	vector<vector<glm::vec2>> texCoordMesh;
	vector<vector<glm::vec3>> normalMesh;
	vector<vector<GLuint>> vIndicesMesh;
	vector<vector<VertexJointWeights>> jointWeightsMesh;					//relationship with vertex and bone

	vector<glm::mat4> nodeGlobalTransforms; //global transform per node (indexed by node), used by updatePose()

	int modelAmount = 0;

	int loadAnimation(const aiScene* scene, string originalFileName) {
		if (scene->mNumAnimations == 0) {
			return 0;
		}
		string filename = std::filesystem::path(originalFileName).stem().string();
		int putCount = 0;
		for (unsigned int animIdx = 0; animIdx < scene->mNumAnimations; animIdx++) {
			if (findAnimation.count(filename + "_" + to_string(animIdx)) != 0) {
				cerr << "There's " << filename << " animation already" << endl;
				continue;
			}
			const aiAnimation* anim = scene->mAnimations[animIdx];
			Animation animation;
			animation.duration = anim->mDuration;
			animation.tickperSec = anim->mTicksPerSecond > 0.0 ? anim->mTicksPerSecond : 25.0;
			animation.channels.resize(static_cast<size_t>(anim->mNumChannels));
			//collect by channel
			for (unsigned int ch = 0; ch < anim->mNumChannels; ch++) {
				aiNodeAnim* nodeAnim = anim->mChannels[ch];
				//collecting information by keys
				animation.channels[ch].name = nodeAnim->mNodeName.C_Str();
				animation.channelIndexByName[nodeAnim->mNodeName.C_Str()] = ch; // collect node name with channel number
				animation.channels[ch].positionKeys.resize(nodeAnim->mNumPositionKeys);
				animation.channels[ch].rotationKeys.resize(nodeAnim->mNumRotationKeys);
				animation.channels[ch].scalingKeys.resize(nodeAnim->mNumScalingKeys);
				for (unsigned int posKey = 0; posKey < nodeAnim->mNumPositionKeys; posKey++) {
					animation.channels[ch].positionKeys[posKey].value.x = nodeAnim->mPositionKeys[posKey].mValue.x;
					animation.channels[ch].positionKeys[posKey].value.y = nodeAnim->mPositionKeys[posKey].mValue.y;
					animation.channels[ch].positionKeys[posKey].value.z = nodeAnim->mPositionKeys[posKey].mValue.z;
					animation.channels[ch].positionKeys[posKey].time = nodeAnim->mPositionKeys[posKey].mTime;
				}
				for (unsigned int rotKey = 0; rotKey < nodeAnim->mNumRotationKeys; rotKey++) {
					animation.channels[ch].rotationKeys[rotKey].value.w = nodeAnim->mRotationKeys[rotKey].mValue.w;
					animation.channels[ch].rotationKeys[rotKey].value.x = nodeAnim->mRotationKeys[rotKey].mValue.x;
					animation.channels[ch].rotationKeys[rotKey].value.y = nodeAnim->mRotationKeys[rotKey].mValue.y;
					animation.channels[ch].rotationKeys[rotKey].value.z = nodeAnim->mRotationKeys[rotKey].mValue.z;
					animation.channels[ch].rotationKeys[rotKey].time = nodeAnim->mRotationKeys[rotKey].mTime;
				}
				for (unsigned int scaleKey = 0; scaleKey < nodeAnim->mNumScalingKeys; scaleKey++) {
					animation.channels[ch].scalingKeys[scaleKey].value.x = nodeAnim->mScalingKeys[scaleKey].mValue.x;
					animation.channels[ch].scalingKeys[scaleKey].value.y = nodeAnim->mScalingKeys[scaleKey].mValue.y;
					animation.channels[ch].scalingKeys[scaleKey].value.z = nodeAnim->mScalingKeys[scaleKey].mValue.z;
					animation.channels[ch].scalingKeys[scaleKey].time = nodeAnim->mScalingKeys[scaleKey].mTime;
				}
			}
			animation.nodeToChannel.assign(nodes.size(), -1);
			for (int i = 0; i < nodes.size(); i++) {
				auto channelIt = animation.channelIndexByName.find(nodes[i].name);
				if (channelIt != animation.channelIndexByName.end()) {
					animation.nodeToChannel[i] = channelIt->second;
				}
			}
			animations.push_back(std::move(animation));
			findAnimation[filename + "_" + to_string(animIdx)] = static_cast<int>(animations.size()) - 1;
			putCount++;
		}
		return putCount;
	}

public:
	FBXModel() : Model() {}

	~FBXModel()
	{
		for (int i = 0; i < meshesVAO.size(); i++)
		{
			glDeleteVertexArrays(1, &meshesVAO[i]);
			glDeleteBuffers(1, &meshesVBOPosition[i]);
			glDeleteBuffers(1, &meshesVBONormal[i]);
			glDeleteBuffers(1, &meshesVBOTexCoord[i]);
			glDeleteBuffers(1, &meshesVBOBoneID[i]);
			glDeleteBuffers(1, &meshesVBOWeights[i]);
			glDeleteTextures(1, &meshesTexture[i]);
			glDeleteBuffers(1, &meshesEBO[i]);
		}
		glDeleteBuffers(1, &skinningUBO);
		glDeleteBuffers(1, &SSBOId);
	}

	void init() override {
		for (int i = 0; i < meshesVAO.size(); i++)
		{
			glGenVertexArrays(1, &meshesVAO[i]);
			glGenBuffers(1, &meshesVBOPosition[i]);
			glGenBuffers(1, &meshesVBONormal[i]);
			glGenBuffers(1, &meshesVBOTexCoord[i]);
			glGenBuffers(1, &meshesVBOBoneID[i]);
			glGenBuffers(1, &meshesVBOWeights[i]);
			glGenTextures(1, &meshesTexture[i]);
			glGenBuffers(1, &meshesEBO[i]);
		}
		glGenBuffers(1, &skinningUBO);
		glGenBuffers(1, &SSBOId);
	}

	// TODO: change into Universal code.
	// Refactoring plan: Change to texfile const string& and save size also
	// for safety, need to be changed to for context
	bool loadDiffuseMap(const char* texFile) override
	{
		loadTextureFile(meshesTexture[0], "Subpe_Redhood__head_BaseColor.png");
		loadTextureFile(meshesTexture[1], "Subpe_Redhood__Cloths_BaseColor.png");
		loadTextureFile(meshesTexture[2], "Subpe_Redhood__head_BaseColor.png");
		loadTextureFile(meshesTexture[3], "Subpe_Redhood__Cloths_BaseColor.png");
		loadTextureFile(meshesTexture[4], "Subpe_Redhood__Cloths_BaseColor.png");
		loadTextureFile(meshesTexture[5], "Subpe_Redhood__head_BaseColor.png");

		useDiffuseMap = true;
		return true;
	}

	void loadFBX(string objectFileName, vector<string> animationFileName) override
	{
		Assimp::Importer objectImporter;
		Assimp::Importer animationImporter;
		unsigned int flags = aiProcess_GenSmoothNormals |
			aiProcess_CalcTangentSpace |
			aiProcess_SortByPType |
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_FindInvalidData |
			aiProcess_FindDegenerates |
			aiProcess_ImproveCacheLocality;
		objectImporter.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
		animationImporter.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
		const aiScene* scene = objectImporter.ReadFile(objectFileName.c_str(), flags);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			cerr << "FBX Object Import Error: " << objectImporter.GetErrorString() << std::endl;
			return;
		}

		std::unordered_map<string, int> nameToNodeId;

		//refactor: Bone data saving(from scene)
		nodes.clear();
		saveNodes(scene->mRootNode, -1);
		aiMatrix4x4 invRootTransformAssimp = scene->mRootNode->mTransformation;
		invRootTransformAssimp = invRootTransformAssimp.Inverse();
		globalInverseRootTransform = aiMatToGlmMat(invRootTransformAssimp);

		for (int nodesId = 0; nodesId < nodes.size(); nodesId++) {
			nameToNodeId[nodes[nodesId].name] = nodesId;
		}

		//Collect Mesh Information
		jointWeightsMesh.resize(scene->mNumMeshes);
		for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++)
		{
			const aiMesh* mesh = scene->mMeshes[meshIdx];
			vector<glm::vec3> tmpPositions;
			vector<glm::vec2> tmpTexCoords;
			vector<glm::vec3> tmpNormals;
			vector<GLuint> tmpVIndices;
			vector<VertexJointWeights> tmpBoneData;

			//cout << mesh->mName.C_Str() << std::endl;
			//collect vertex information
			tmpBoneData.resize(mesh->mNumVertices);
			for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
			{
				aiVector3D vPosition;
				aiVector3D vNormal;
				aiVector2D vTexCoord;
				vPosition.x = mesh->mVertices[i].x;
				vPosition.y = mesh->mVertices[i].y;
				vPosition.z = mesh->mVertices[i].z;
				tmpPositions.push_back(glm::vec3(vPosition[0], vPosition[1], vPosition[2]));
				vNormal.x = mesh->mNormals[i].x;
				vNormal.y = mesh->mNormals[i].y;
				vNormal.z = mesh->mNormals[i].z;
				tmpNormals.push_back(glm::vec3(vNormal[0], vNormal[1], vNormal[2]));
				if (mesh->HasTextureCoords(0))
				{
					vTexCoord.x = mesh->mTextureCoords[0][i].x;
					vTexCoord.y = mesh->mTextureCoords[0][i].y;
				}
				else
				{
					vTexCoord = aiVector2D(0.0f, 0.0f);
				}
				tmpTexCoords.push_back(glm::vec2(vTexCoord.x, vTexCoord.y));
			}
			for (unsigned i = 0; i < mesh->mNumFaces; i++) {
				aiFace face = mesh->mFaces[i];
				for (int j = 0; j < face.mNumIndices; j++) {
					tmpVIndices.push_back(face.mIndices[j]);
				}
			}
			positionMesh.push_back(tmpPositions);
			normalMesh.push_back(tmpNormals);
			texCoordMesh.push_back(tmpTexCoords);
			vIndicesMesh.push_back(tmpVIndices);



			meshesVAO.push_back(0);
			meshesVBOPosition.push_back(0);
			meshesVBONormal.push_back(0);
			meshesVBOTexCoord.push_back(0);
			meshesVBOBoneID.push_back(0);
			meshesVBOWeights.push_back(0);
			meshesEBO.push_back(0);
			meshesTexture.push_back(0);

			Skin tempJointFromMesh;

			tempJointFromMesh.jointNodeIds.assign(mesh->mNumBones, -1);
			tempJointFromMesh.inverseBindMatrices.assign(mesh->mNumBones, glm::mat4(1.0f));

			if (mesh->mNumBones > MAX_JOINTS) {
				std::cerr << "Over maximum joints amount. please adjust maxumum joints amount" << std::endl;
			}

			for (unsigned int jointIdx = 0; jointIdx < mesh->mNumBones; jointIdx++)
			{
				aiBone* aiJoint = mesh->mBones[jointIdx];

				auto it = nameToNodeId.find(aiJoint->mName.C_Str());
				if (it != nameToNodeId.end()) {
					tempJointFromMesh.jointNodeIds[jointIdx] = it->second;
					tempJointFromMesh.inverseBindMatrices[jointIdx] = aiMatToGlmMat(aiJoint->mOffsetMatrix);
				}
				else {
					std::cerr << "Can't find the joint's name" << aiJoint->mName.C_Str() << std::endl;
				}

				for (unsigned int id = 0; id < aiJoint->mNumWeights; id++) {
					int vertexId = aiJoint->mWeights[id].mVertexId;
					float weight_val = aiJoint->mWeights[id].mWeight;


					if (tmpBoneData[vertexId].weight.size() < 4) {
						tmpBoneData[vertexId].weight.push_back(weight_val);
						tmpBoneData[vertexId].jointIndex.push_back(jointIdx);
					}
					else {
						int minIdx = 0;
						float minWeight = 1;
						for (int wId = 0; wId < tmpBoneData[vertexId].weight.size(); wId++) {
							if (tmpBoneData[vertexId].weight[wId] < minWeight) {
								minWeight = tmpBoneData[vertexId].weight[wId];
								minIdx = wId;
							}
						}
						if (minWeight < weight_val) {
							tmpBoneData[vertexId].weight[minIdx] = weight_val;
							tmpBoneData[vertexId].jointIndex[minIdx] = jointIdx;
						}
					}
				}
			}
			meshSkins.push_back(tempJointFromMesh);


			for (int vIdx = 0; vIdx < tmpBoneData.size(); vIdx++)
			{
				VertexJointWeights& currentBoneData = tmpBoneData[vIdx];
				while (currentBoneData.weight.size() < 4) {
					currentBoneData.weight.push_back(0.0f);
				}
				while (currentBoneData.jointIndex.size() < 4) {
					currentBoneData.jointIndex.push_back(0);
				}
				float weightSum = 0;
				for (int wId = 0; wId < 4; wId++) {
					weightSum += currentBoneData.weight[wId];
				}
				if (weightSum != 0) {
					for (int wId = 0; wId < 4; wId++) {
						currentBoneData.weight[wId] = currentBoneData.weight[wId] / weightSum;
					}
				}
				jointWeightsMesh[meshIdx].push_back(currentBoneData);
			}

		}

		//make space for each draw call's skinning calculation val
		meshSkinningMatrices.assign(meshSkins.size(), vector<glm::mat4>());
		for (int i = 0; i < meshSkins.size(); i++) {
			meshSkinningMatrices[i].assign(meshSkins[i].inverseBindMatrices.size(), glm::mat4(1.0f));
		}


		//Collect Animation informaiton from FBX Bone file
		if (loadAnimation(scene, objectFileName) == 0) cerr << objectFileName << " : No animation found" << endl;

		// ..from Another Animation file
		for (int fileCount = 0; fileCount < animationFileName.size(); fileCount++) {
			const aiScene* animScene = animationImporter.ReadFile(animationFileName[fileCount].c_str(), flags);
			if (!animScene)
			{
				cerr << animationFileName[fileCount] << " : Animation Import Error: " << animationImporter.GetErrorString() << std::endl;
				continue;
			}
			if (loadAnimation(animScene, animationFileName[fileCount]) == 0) {
				cerr << animationFileName[fileCount] << " : No animation found" << endl;
			}
		}

		nodeGlobalTransforms.assign(nodes.size(), glm::mat4(1.0f)); //nodeGlobalTransform initializing(after nodes available)

		this->init();
		prepareBuffers();
		loadDiffuseMap("");
	}

	void calculateTick(double currentTime) {
		if (animations.size() > currentAnimationId && currentAnimationId >= 0) {
			double lastedSinceChange = currentTime - currentAnimStartedTime;
			double lastAnimTick = fmod((currentTime - lastAnimStartedTime) * animations[lastAnimationId].tickperSec, animations[lastAnimationId].duration);
			double currentAnimTick = fmod(lastedSinceChange * animations[currentAnimationId].tickperSec, animations[currentAnimationId].duration);
			double w = lastedSinceChange / transitionLength;
			{
				CPUProfiler boneProfile(CPUBoneUpdate);
				updatePose(lastAnimTick, currentAnimTick, w);
			}
		}
	}

	void setModelMatrices(const vector<glm::mat4>& matrices) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBOId);
		modelAmount = static_cast<int>(matrices.size());
		glBufferData(GL_SHADER_STORAGE_BUFFER, modelAmount * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, modelAmount *sizeof(glm::mat4), matrices.data());
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void draw(GLuint _shaderID, double currentTime) override {
		if (_shaderID != shaderInfo.shaderId) setShaderInfo(_shaderID);
		glUniform3fv(shaderInfo.ulDefaultAmbient, 1, glm::value_ptr(defaultAmbient));
		glUniform3fv(shaderInfo.ulDefaultDiffuse, 1, glm::value_ptr(defaultDiffuse));
		glUniform3fv(shaderInfo.ulDefaultSpecular, 1, glm::value_ptr(defaultSpecular));
		glUniform1i(shaderInfo.ulUseDiffuseMap, (int)useDiffuseMap);
		glUniform1i(shaderInfo.ulUseSpecularMap, (int)useSpecularMap);
		for (int i = 0; i < meshesVAO.size(); i++)
		{
			//Setting UBO offset
			glBindBuffer(GL_UNIFORM_BUFFER, skinningUBO);
			glBufferData(GL_UNIFORM_BUFFER, MAX_JOINTS * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4) * meshSkinningMatrices[i].size(), meshSkinningMatrices[i].data());
			
			glBindBuffer(GL_UNIFORM_BUFFER, 0);

			glUniform1i(shaderInfo.ulUseNormal, (int)normalMesh[i].size());
			glUniform1f(shaderInfo.ulShininess, shininess);

			if (useDiffuseMap) {
				glUniform1i(shaderInfo.ulDiffuse, i);
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, meshesTexture[i]);
			}

			glBindVertexArray(meshesVAO[i]);
			glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(vIndicesMesh[i].size()), GL_UNSIGNED_INT, 0, modelAmount);
			//glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(vIndicesMesh[i].size()), GL_UNSIGNED_INT, 0);

			glBindVertexArray(0);
		}


	}

	void setCurrentAnimationId(string animationName, double currentTime) override{
		auto it = findAnimation.find(animationName);
		if (it != findAnimation.end()) {
			if (currentAnimationId != it->second) {
				lastAnimationId = currentAnimationId;
				currentAnimationId = it->second;
				lastAnimStartedTime = currentAnimStartedTime;
				currentAnimStartedTime = currentTime;
			}
		}
	}

protected:
	void prepareBuffers() override
	{
		for (int i = 0; i < meshesVAO.size(); i++)
		{
			glBindVertexArray(meshesVAO[i]);
			glBindBuffer(GL_ARRAY_BUFFER, meshesVBOPosition[i]);
			glBufferData(GL_ARRAY_BUFFER, positionMesh[i].size() * sizeof(glm::vec3), &positionMesh[i][0], GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), reinterpret_cast<void*>(0));
			glEnableVertexAttribArray(0);

			if (!texCoordMesh[i].empty())
			{
				glBindBuffer(GL_ARRAY_BUFFER, meshesVBOTexCoord[i]);
				glBufferData(GL_ARRAY_BUFFER, texCoordMesh[i].size() * sizeof(glm::vec2), &texCoordMesh[i][0], GL_STATIC_DRAW);
				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), reinterpret_cast<void*>(0));
				glEnableVertexAttribArray(1);
			}
			
			if (!normalMesh[i].empty())
			{
				glBindBuffer(GL_ARRAY_BUFFER, meshesVBONormal[i]);
				glBufferData(GL_ARRAY_BUFFER, normalMesh[i].size() * sizeof(glm::vec3), &normalMesh[i][0], GL_STATIC_DRAW);
				glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), reinterpret_cast<void*>(0));
				glEnableVertexAttribArray(2);
			}

			if (!jointWeightsMesh[i].empty())
			{
				vector<glm::ivec4> boneIndicesPerVertex;
				vector<glm::vec4> boneWeightsPerVertex;

				boneIndicesPerVertex.reserve(jointWeightsMesh[i].size());
				boneWeightsPerVertex.reserve(jointWeightsMesh[i].size());

				for (const auto& vertexBoneData : jointWeightsMesh[i])
				{
					glm::ivec4 indices;
					glm::vec4 weights;
					for (int j = 0; j < 4; j++) {
						indices[j] = (j < vertexBoneData.jointIndex.size()) ? vertexBoneData.jointIndex[j] : 0;
						weights[j] = (j < vertexBoneData.weight.size()) ? vertexBoneData.weight[j] : 0;
					}
					boneIndicesPerVertex.push_back(indices);
					boneWeightsPerVertex.push_back(weights);
				}
				
				glBindBuffer(GL_ARRAY_BUFFER, meshesVBOBoneID[i]);
				glBufferData(GL_ARRAY_BUFFER, boneIndicesPerVertex.size() * sizeof(glm::ivec4), boneIndicesPerVertex.data(), GL_STATIC_DRAW);
				glVertexAttribIPointer(3, 4, GL_INT, sizeof(glm::ivec4), reinterpret_cast<void*>(0));
				glEnableVertexAttribArray(3);

				glBindBuffer(GL_ARRAY_BUFFER, meshesVBOWeights[i]);
				glBufferData(GL_ARRAY_BUFFER, boneWeightsPerVertex.size() * sizeof(glm::vec4), boneWeightsPerVertex.data(), GL_STATIC_DRAW);
				glVertexAttribPointer(4, 4, GL_FLOAT,GL_FALSE, sizeof(glm::vec4), reinterpret_cast<void*>(0));
				glEnableVertexAttribArray(4);

				}
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshesEBO[i]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, vIndicesMesh[i].size() * sizeof(GLuint), vIndicesMesh[i].data(), GL_STATIC_DRAW);

			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}
		glBindBuffer(GL_UNIFORM_BUFFER, skinningUBO);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, skinningUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBOId);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, SSBOId);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}	

	void setShaderInfo(GLuint _shaderId) {
		shaderInfo.shaderId = _shaderId;
		shaderInfo.ulDefaultAmbient = glGetUniformLocation(shaderInfo.shaderId, "material.defaultAmbient");
		shaderInfo.ulDefaultDiffuse = glGetUniformLocation(shaderInfo.shaderId, "material.defaultDiffuse");
		shaderInfo.ulDefaultSpecular = glGetUniformLocation(shaderInfo.shaderId, "material.defaultSpecular");
		shaderInfo.ulUseDiffuseMap = glGetUniformLocation(shaderInfo.shaderId, "material.useDiffuseMap");
		shaderInfo.ulUseSpecularMap = glGetUniformLocation(shaderInfo.shaderId, "material.useSpecularMap");
		shaderInfo.ulUseNormal = glGetUniformLocation(shaderInfo.shaderId, "useNormal");
		shaderInfo.ulShininess = glGetUniformLocation(shaderInfo.shaderId, "material.shininess");
		shaderInfo.ulDiffuse = glGetUniformLocation(shaderInfo.shaderId, "material.diffuse");
	}

private:
	void updatePose(double lastAnimTick, double currentAnimTick, double w) {
		w = clamp(w, 0.0, 1.0);
		for (int nodeIdx = 0; nodeIdx < nodes.size(); nodeIdx++) {
			glm::mat4 localNodeTransform;
			glm::vec3 currentT, currentS, lastT,  lastS;
			glm::quat currentR, lastR;
			// Channel found: build the local transform from interpolated keys. Otherwise use the bind pose.
			
			int currentId = animations[currentAnimationId].nodeToChannel[nodeIdx];
			if (currentId != -1) {
				const Animation::Channel& channel = animations[currentAnimationId].channels[currentId];
				currentT = pickKey(channel.positionKeys, currentAnimTick);
				currentR = pickKey(channel.rotationKeys, currentAnimTick);
				currentS = pickKey(channel.scalingKeys, currentAnimTick);
			}
			else {
				currentT = nodes[nodeIdx].translate;
				currentR = nodes[nodeIdx].rotate;
				currentS = nodes[nodeIdx].scale;
			}

			glm::mat4 mixedTranslation, mixedRotation, mixedScale;

			if(w < 1.0){
				int lastId = animations[lastAnimationId].nodeToChannel[nodeIdx];
				if (lastId != -1) {
					const Animation::Channel& lastCh = animations[lastAnimationId].channels[lastId];
					lastT = pickKey(lastCh.positionKeys, lastAnimTick);
					lastR = pickKey(lastCh.rotationKeys, lastAnimTick);
					lastS = pickKey(lastCh.scalingKeys, lastAnimTick);
				}
				else {
					lastT = nodes[nodeIdx].translate;
					lastR = nodes[nodeIdx].rotate;
					lastS = nodes[nodeIdx].scale;
				}

				mixedTranslation = glm::translate(glm::mat4(1.0f), glm::mix(lastT, currentT, static_cast<float>(w)));
				mixedRotation = glm::mat4_cast(glm::slerp(lastR, currentR, static_cast<float>(w)));
				mixedScale = glm::scale(glm::mat4(1.0f), glm::mix(lastS, currentS, static_cast<float>(w)));
			}
			else {
				mixedTranslation = glm::translate(glm::mat4(1.0f), currentT);
				mixedRotation = glm::mat4_cast(currentR);
				mixedScale = glm::scale(glm::mat4(1.0f), currentS);
			}
			
			


			localNodeTransform = mixedTranslation * mixedRotation * mixedScale;
			int p = nodes[nodeIdx].parent;

			if (p == -1) nodeGlobalTransforms[nodeIdx] = localNodeTransform;
			else nodeGlobalTransforms[nodeIdx] = nodeGlobalTransforms[p] * localNodeTransform;
		}
		for (int i = 0; i < meshSkins.size(); i++) {
			for (int j = 0; j < meshSkins[i].jointNodeIds.size(); j++) {
				if (meshSkins[i].jointNodeIds[j] == -1) meshSkinningMatrices[i][j] = glm::mat4(1.0f);
				else meshSkinningMatrices[i][j] = globalInverseRootTransform * nodeGlobalTransforms[meshSkins[i].jointNodeIds[j]] * meshSkins[i].inverseBindMatrices[j];
			}
		}
	}

	int saveNodes(const aiNode* cur, int parentInfo) {
		if (!cur) return -1;
		glm::mat4 matrix = aiMatToGlmMat(cur->mTransformation);
		glm::vec3 trash1;
		glm::vec4 trash2;
		Node temp;
		temp.name = cur->mName.C_Str();
		temp.parent = parentInfo;
		glm::decompose(matrix, temp.scale, temp.rotate, temp.translate, trash1, trash2);
		nodes.push_back(std::move(temp));
		int currInfo = static_cast<int>(nodes.size()) - 1;
		for (unsigned int i = 0; i < cur->mNumChildren; i++) {
			int childrenInfo = saveNodes(cur->mChildren[i], currInfo);
			if (childrenInfo >= 0) nodes[currInfo].children.push_back(childrenInfo);
		}
		return currInfo;
	}

	//ai matrices to glm matrices
	glm::mat4 aiMatToGlmMat(aiMatrix4x4 aiMat)
	{
		glm::mat4 result;
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				result[j][i] = aiMat[i][j];
			}
		}
		return result;
	}

	glm::vec3 pickKey(const vector<Animation::Channel::VectorKey>& keys, double tickTime) {
		if (tickTime > keys[keys.size() - 1].time) return keys[keys.size() - 1].value;
		else if (tickTime < keys[0].time) return keys[0].value;
		if (keys.size() == 1) {
			return keys[0].value;
		}
		int left = 0;
		int right = static_cast<int>(keys.size()) - 1;
		while ((left + 1 <= keys.size() - 1 && right - 1 >= 0) && (right - left != 1)) {
			if (keys[left + 1].time <= tickTime) left++;
			if (keys[right - 1].time > tickTime) right--;
		}
		
		double portion = (tickTime - keys[left].time) / (keys[right].time - keys[left].time);
		return glm::mix(keys[left].value, keys[right].value, static_cast<float>(portion));
	}

	glm::quat pickKey(const vector<Animation::Channel::QuaternionKey>& keys, double tickTime) {
		if (tickTime > keys[keys.size() - 1].time) return keys[keys.size() - 1].value;
		else if (tickTime < keys[0].time) return keys[0].value;
		if (keys.size() == 1) {
			return keys[0].value;
		}
		int left = 0;
		int right = static_cast<int>(keys.size()) - 1;
		while ((left + 1 <= keys.size() - 1 && right - 1 >= 0) && right - left != 1) {
			if (keys[left + 1].time <= tickTime) left++;
			if (keys[right - 1].time > tickTime) right--;
		}

		double portion = (tickTime - keys[left].time) / (keys[right].time - keys[left].time);
		return glm::slerp(keys[left].value, keys[right].value, static_cast<float>(portion));
	}
};