#include "FBXModel.h"
#include<algorithm>
#include<iostream>
#include<filesystem>
#include<type_traits>

#define GLM_ENABLE_EXPERIMENTAL
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/matrix_decompose.hpp>

#include<assimp/Importer.hpp>
#include<assimp/scene.h>
#include<assimp/postprocess.h>


#include "CPUProfiler.h"
#include "GPUProfiler.h"
#include "Material.h"
#include "Texture.h"
#define MAX_JOINTS 52 //Set the same as shader's MAX_JOINTS

namespace 
{
	template<typename T>
	auto pickKey(const T& keys, double tickTime, const decltype(T::value_type::value)& fallback) {
		if (keys.empty()) return fallback;
		if (tickTime > keys[keys.size() - 1].time) return keys[keys.size() - 1].value;
		else if (tickTime < keys[0].time) return keys[0].value;

		if (keys.size() == 1) return keys[0].value;
		int left = 0;
		int right = static_cast<int>(keys.size()) - 1;
		while ((left + 1 <= keys.size() - 1 && right - 1 >= 0) && right - left != 1) {
			if (keys[left + 1].time <= tickTime) left++;
			if (keys[right - 1].time > tickTime) right--;
		}

		double portion = (tickTime - keys[left].time) / (keys[right].time - keys[left].time);
		if constexpr (std::is_same<decltype(keys[0].value), glm::vec3>::value) {
			return glm::mix(keys[left].value, keys[right].value, static_cast<float>(portion));
		}
		else {
			return glm::slerp(keys[left].value, keys[right].value, static_cast<float>(portion));
		}
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
}

int FBXModel::loadAnimation(const aiScene* scene, const std::string& originalFileName) {
	if (scene->mNumAnimations == 0) {
		return 0;
	}
	std::string filename = std::filesystem::path(originalFileName).stem().string();
	int putCount = 0;
	for (unsigned int animIdx = 0; animIdx < scene->mNumAnimations; animIdx++) {
		if (findAnimation.count(filename + "_" + std::to_string(animIdx)) != 0) {
			std::cerr << "There's " << filename << " animation already" << std::endl;
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
		findAnimation[filename + "_" + std::to_string(animIdx)] = static_cast<int>(animations.size()) - 1;
		putCount++;
	}
	return putCount;
}

FBXModel::FBXModel() {
	material.shininess = 32.f;
	material.useDiffuseMap = false;
	material.useSpecularMap = false;

	material.defaultAmbient = glm::vec3(1.0f, 1.0f, 1.0f);
	material.defaultDiffuse = glm::vec3(1.0f, 1.0f, 1.0f);
	material.defaultSpecular = glm::vec3(0.0f, 0.0f, 0.0f);
}

FBXModel::~FBXModel() {
	for (int i = 0; i < meshesVAO.size(); i++)
	{
		glDeleteVertexArrays(1, &meshesVAO[i]);
		glDeleteBuffers(1, &meshesVBOPosition[i]);
		glDeleteBuffers(1, &meshesVBONormal[i]);
		glDeleteBuffers(1, &meshesVBOTexCoord[i]);
		glDeleteBuffers(1, &meshesVBOJointIndices[i]);
		glDeleteBuffers(1, &meshesVBOJointWeights[i]);
		glDeleteTextures(1, &meshesTexture[i]);
		glDeleteBuffers(1, &meshesEBO[i]);
	}
	glDeleteBuffers(1, &skinningUBO);
	glDeleteBuffers(1, &SSBOId);
}

void FBXModel::init() {
	for (int i = 0; i < meshesVAO.size(); i++)
	{
		glGenVertexArrays(1, &meshesVAO[i]);
		glGenBuffers(1, &meshesVBOPosition[i]);
		glGenBuffers(1, &meshesVBONormal[i]);
		glGenBuffers(1, &meshesVBOTexCoord[i]);
		glGenBuffers(1, &meshesVBOJointIndices[i]);
		glGenBuffers(1, &meshesVBOJointWeights[i]);
		glGenTextures(1, &meshesTexture[i]);
		glGenBuffers(1, &meshesEBO[i]);
	}
	glGenBuffers(1, &skinningUBO);
	glGenBuffers(1, &SSBOId);
}

bool FBXModel::loadDiffuseMap(const char* texFile)
{
	loadTextureFile(meshesTexture[0], "Subpe_Redhood__head_BaseColor.png");
	loadTextureFile(meshesTexture[1], "Subpe_Redhood__Cloths_BaseColor.png");
	loadTextureFile(meshesTexture[2], "Subpe_Redhood__head_BaseColor.png");
	loadTextureFile(meshesTexture[3], "Subpe_Redhood__Cloths_BaseColor.png");
	loadTextureFile(meshesTexture[4], "Subpe_Redhood__Cloths_BaseColor.png");
	loadTextureFile(meshesTexture[5], "Subpe_Redhood__head_BaseColor.png");

	material.useDiffuseMap = true;
	return true;
}

void FBXModel::loadFBX(const std::string& objectFileName, const std::vector<std::string>& animationFileName)
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
		std::cerr << "FBX Object Import Error: " << objectImporter.GetErrorString() << std::endl;
		return;
	}

	std::unordered_map<std::string, int> nameToNodeId;

	//Save the node tree
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
		std::vector<glm::vec3> tmpPositions;
		std::vector<glm::vec2> tmpTexCoords;
		std::vector<glm::vec3> tmpNormals;
		std::vector<GLuint> tmpVIndices;
		std::vector<VertexJointWeights> tmpBoneData;

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
		meshesVBOJointIndices.push_back(0);
		meshesVBOJointWeights.push_back(0);
		meshesEBO.push_back(0);
		meshesTexture.push_back(0);

		Skin tempJointFromMesh;

		tempJointFromMesh.jointNodeIds.assign(mesh->mNumBones, -1);
		tempJointFromMesh.inverseBindMatrices.assign(mesh->mNumBones, glm::mat4(1.0f));

		if (mesh->mNumBones > MAX_JOINTS) {
			std::cerr << "Over maximum joints amount. please adjust maximum joints amount" << std::endl;
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
				std::cerr << "Can't find the joint's name : " << aiJoint->mName.C_Str() << std::endl;
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
	meshSkinningMatrices.assign(meshSkins.size(), std::vector<glm::mat4>());
	for (int i = 0; i < meshSkins.size(); i++) {
		meshSkinningMatrices[i].assign(meshSkins[i].inverseBindMatrices.size(), glm::mat4(1.0f));
	}


	//Collect Animation informaiton from FBX Bone file
	if (loadAnimation(scene, objectFileName) == 0) std::cerr << objectFileName << " : No animation found" << std::endl;

	// ..from Another Animation file
	for (int fileCount = 0; fileCount < animationFileName.size(); fileCount++) {
		const aiScene* animScene = animationImporter.ReadFile(animationFileName[fileCount].c_str(), flags);
		if (!animScene)
		{
			std::cerr << animationFileName[fileCount] << " : Animation Import Error: " << animationImporter.GetErrorString() << std::endl;
			continue;
		}
		if (loadAnimation(animScene, animationFileName[fileCount]) == 0) {
			std::cerr << animationFileName[fileCount] << " : No animation found" << std::endl;
		}
	}

	nodeGlobalTransforms.assign(nodes.size(), glm::mat4(1.0f)); //nodeGlobalTransform initializing(after nodes available)

	this->init();
	prepareBuffers();
	loadDiffuseMap("");
}

void FBXModel::updateAnimation(double currentTime) {
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

void FBXModel::setModelMatrices(const std::vector<glm::mat4>& matrices) {
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBOId);
	modelAmount = static_cast<int>(matrices.size());
	glBufferData(GL_SHADER_STORAGE_BUFFER, modelAmount * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, modelAmount * sizeof(glm::mat4), matrices.data());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void FBXModel::draw(GLuint _shaderID) {
	if (_shaderID != shaderInfo.shaderId) setShaderInfo(_shaderID);
	glUniform3fv(shaderInfo.ulDefaultAmbient, 1, glm::value_ptr(material.defaultAmbient));
	glUniform3fv(shaderInfo.ulDefaultDiffuse, 1, glm::value_ptr(material.defaultDiffuse));
	glUniform3fv(shaderInfo.ulDefaultSpecular, 1, glm::value_ptr(material.defaultSpecular));
	glUniform1i(shaderInfo.ulUseDiffuseMap, static_cast<int>(material.useDiffuseMap));
	glUniform1i(shaderInfo.ulUseSpecularMap, static_cast<int>(material.useSpecularMap));
	for (int i = 0; i < meshesVAO.size(); i++)
	{
		//Setting UBO offset
		glBindBuffer(GL_UNIFORM_BUFFER, skinningUBO);
		glBufferData(GL_UNIFORM_BUFFER, MAX_JOINTS * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4) * meshSkinningMatrices[i].size(), meshSkinningMatrices[i].data());

		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		glUniform1i(shaderInfo.ulUseNormal, (int)normalMesh[i].size());
		glUniform1f(shaderInfo.ulShininess, material.shininess);

		if (material.useDiffuseMap) {
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

void FBXModel::setCurrentAnimationId(const std::string& animationName, double currentTime) {
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

void FBXModel::prepareBuffers() {
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
			std::vector<glm::ivec4> boneIndicesPerVertex;
			std::vector<glm::vec4> boneWeightsPerVertex;

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

			glBindBuffer(GL_ARRAY_BUFFER, meshesVBOJointIndices[i]);
			glBufferData(GL_ARRAY_BUFFER, boneIndicesPerVertex.size() * sizeof(glm::ivec4), boneIndicesPerVertex.data(), GL_STATIC_DRAW);
			glVertexAttribIPointer(3, 4, GL_INT, sizeof(glm::ivec4), reinterpret_cast<void*>(0));
			glEnableVertexAttribArray(3);

			glBindBuffer(GL_ARRAY_BUFFER, meshesVBOJointWeights[i]);
			glBufferData(GL_ARRAY_BUFFER, boneWeightsPerVertex.size() * sizeof(glm::vec4), boneWeightsPerVertex.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), reinterpret_cast<void*>(0));
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

void FBXModel::setShaderInfo(GLuint _shaderId) {
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

void FBXModel::updatePose(double lastAnimTick, double currentAnimTick, double w) {
	w = std::clamp(w, 0.0, 1.0);
	for (int nodeIdx = 0; nodeIdx < nodes.size(); nodeIdx++) {
		glm::mat4 localNodeTransform;
		glm::vec3 currentT, currentS, lastT, lastS;
		glm::quat currentR, lastR;
		// Channel found: build the local transform from interpolated keys. Otherwise use the bind pose.

		int currentId = animations[currentAnimationId].nodeToChannel[nodeIdx];
		if (currentId != -1) {
			const Animation::Channel& channel = animations[currentAnimationId].channels[currentId];
			currentT = pickKey(channel.positionKeys, currentAnimTick, nodes[nodeIdx].translate);
			currentR = pickKey(channel.rotationKeys, currentAnimTick, nodes[nodeIdx].rotate);
			currentS = pickKey(channel.scalingKeys, currentAnimTick, nodes[nodeIdx].scale);
		}
		else {
			currentT = nodes[nodeIdx].translate;
			currentR = nodes[nodeIdx].rotate;
			currentS = nodes[nodeIdx].scale;
		}

		glm::mat4 mixedTranslation, mixedRotation, mixedScale;

		if (w < 1.0) {
			int lastId = animations[lastAnimationId].nodeToChannel[nodeIdx];
			if (lastId != -1) {
				const Animation::Channel& lastCh = animations[lastAnimationId].channels[lastId];
				lastT = pickKey(lastCh.positionKeys, lastAnimTick, nodes[nodeIdx].translate);
				lastR = pickKey(lastCh.rotationKeys, lastAnimTick, nodes[nodeIdx].rotate);
				lastS = pickKey(lastCh.scalingKeys, lastAnimTick, nodes[nodeIdx].scale);
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

int FBXModel::saveNodes(const aiNode* cur, int parentInfo) {
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

