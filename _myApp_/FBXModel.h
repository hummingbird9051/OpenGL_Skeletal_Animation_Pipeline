#pragma once
#include <iostream>
#include<map>
#include<filesystem>
#include "Model.h"
#include "CPUProfiler.h"
#include "GPUProfiler.h"

using namespace std;


struct VertexBoneData
{
	vector<float> weight;
	vector<int> boneIndex;
	vector<string> name;
};

class FBXModel : public Model
{
private:
	struct Bone
	{
		glm::mat4 boneMatrix = glm::mat4(1.0f);
	};
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

	struct SkeletonBone {
		string name;
		glm::mat4 localBindTransform = glm::mat4(1.0f);
		int parent = -1;
		vector<int> children;
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

	int currentAnimationId = 0;

	vector<SkeletonBone> nodes;

	glm::mat4 globalInverseRootTransform = glm::mat4(1.0f);

	vector<glm::mat4> currentBoneMatrices; //save bone matrices each frame

	vector<GLuint> meshesVAO;
	vector<GLuint> meshesVBOPosition;
	vector<GLuint> meshesVBONormal;
	vector<GLuint> meshesVBOTexCoord;
	vector<GLuint> meshesVBOBoneID;
	vector<GLuint> meshesVBOWeights;
	vector<GLuint> meshesEBO;
	GLuint UboMatrices = 0;

	vector<GLuint> meshesTexture;

	vector<vector<glm::vec3>> positionMesh;
	vector<vector<glm::vec2>> texCoordMesh;
	vector<vector<glm::vec3>> normalMesh;
	vector<vector<GLuint>> vIndicesMesh;
	vector<vector<VertexBoneData>> boneDataMesh;					//vbo로 넘길 버텍스별 본과의 관계

	map<string, Bone> meshBone;
	vector<Bone> meshBoneWithIndex;

	vector<glm::mat4> nodeGlobalTransforms; //global transform per node (indexed by node), used by UpdateAndCalculateMatrix()

	

	int boneCount = 0;

	vector<string> boneName;
	map<string, int> boneNameMap;
	vector<int> nodeToBone;

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
		glDeleteBuffers(1, &UboMatrices);
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
		glGenBuffers(1, &UboMatrices);
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

		//refactor: Bone data saving(from scene)
		nodes.clear();
		saveBoneData(scene->mRootNode, -1);
		aiMatrix4x4 invRootTransformAssimp = scene->mRootNode->mTransformation;
		invRootTransformAssimp = invRootTransformAssimp.Inverse();
		globalInverseRootTransform = aiMatToGlmMat(invRootTransformAssimp);

		//Collect Mesh Information
		boneDataMesh.resize(scene->mNumMeshes);
		for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++)
		{
			const aiMesh* mesh = scene->mMeshes[meshIdx];
			vector<glm::vec3> tmpPositions;
			vector<glm::vec2> tmpTexCoords;
			vector<glm::vec3> tmpNormals;
			vector<GLuint> tmpVIndices;
			vector<VertexBoneData> tmpBoneData;

			//cout << mesh->mName.C_Str() << std::endl;
			//collect vertex information
			tmpBoneData.resize(mesh->mNumVertices);
			for (unsigned i = 0; i < mesh->mNumVertices; ++i)
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

			for (unsigned int boneMeshId = 0; boneMeshId < mesh->mNumBones; boneMeshId++)
			{
				aiBone* bone_mesh = mesh->mBones[boneMeshId];
				if (find(boneName.begin(), boneName.end(), bone_mesh->mName.C_Str()) == boneName.end())
				{
					boneName.push_back(bone_mesh->mName.C_Str());
				}
				string name_of_current_aiBone = bone_mesh->mName.C_Str();
				for (unsigned int id = 0; id < bone_mesh->mNumWeights; id++) {
					int vertexId = bone_mesh->mWeights[id].mVertexId;
					float weight_val = bone_mesh->mWeights[id].mWeight;


					if (tmpBoneData[vertexId].name.size() < 4) {
						tmpBoneData[vertexId].name.push_back(name_of_current_aiBone);
						tmpBoneData[vertexId].weight.push_back(weight_val);
						tmpBoneData[vertexId].boneIndex.push_back(0);
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
							tmpBoneData[vertexId].name[minIdx] = name_of_current_aiBone;
							tmpBoneData[vertexId].boneIndex[minIdx] = 0;
						}
					}
				}
				meshBone[name_of_current_aiBone].boneMatrix = aiMatToGlmMat(bone_mesh->mOffsetMatrix);
			}


			for (int vIdx = 0; vIdx < tmpBoneData.size(); vIdx++)
			{
				VertexBoneData& currentBoneData = tmpBoneData[vIdx];
				while (currentBoneData.weight.size() < 4) {
					currentBoneData.weight.push_back(0.0f);
				}
				while (currentBoneData.boneIndex.size() < 4) {
					currentBoneData.boneIndex.push_back(0);
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
				boneDataMesh[meshIdx].push_back(currentBoneData);
			}

		}

		//cout << boneName.size() << endl;
		boneNameMap.clear();
		for (int i = 0; i < boneName.size(); i++)
		{
			boneNameMap[boneName[i]] = i;
		}
		boneCount = static_cast<int>(boneName.size());

		currentBoneMatrices.assign(boneCount, glm::mat4(1.0f));

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
		


		//Saving Bone weigh and indices each vertex
		for (int meshIdx = 0; meshIdx < boneDataMesh.size(); meshIdx++)
		{
			for (int meshVertex = 0; meshVertex < boneDataMesh[meshIdx].size(); meshVertex++)
			{
				for (int i = 0; i < boneDataMesh[meshIdx][meshVertex].name.size(); i++)
				{
					boneDataMesh[meshIdx][meshVertex].boneIndex[i] = boneNameMap[boneDataMesh[meshIdx][meshVertex].name[i]];
				}
			}
		}

		nodeGlobalTransforms.assign(nodes.size(), glm::mat4(1.0f)); //nodeGlobalTransform initializing(after nodes available)

		nodeToBone.assign(nodes.size(), -1);
		for (int i = 0; i < nodes.size(); i++) {
			auto it = boneNameMap.find(nodes[i].name);
			if (it != boneNameMap.end()) {
				nodeToBone[i] = it->second;
			}
		}

		meshBoneWithIndex.assign(boneCount, Bone());
		for (auto it : meshBone) {
			auto boneNameIt = boneNameMap.find(it.first);
			if (boneNameIt != boneNameMap.end()) {
				meshBoneWithIndex[boneNameIt->second] = it.second;
			}
		}


		this->init();
		prepareBuffers();
		loadDiffuseMap("");
	}

	void draw(GLuint _shaderID, double currentTime) override {
		if (_shaderID != shaderInfo.shaderId) setShaderInfo(_shaderID);
		glUniform3fv(shaderInfo.ulDefaultAmbient, 1, glm::value_ptr(defaultAmbient));
		glUniform3fv(shaderInfo.ulDefaultDiffuse, 1, glm::value_ptr(defaultDiffuse));
		glUniform3fv(shaderInfo.ulDefaultSpecular, 1, glm::value_ptr(defaultSpecular));
		glUniform1i(shaderInfo.ulUseDiffuseMap, (int)useDiffuseMap);
		glUniform1i(shaderInfo.ulUseSpecularMap, (int)useSpecularMap);

		
		if (animations.size() > currentAnimationId && currentAnimationId >=0) {
			double tick = fmod(currentTime * animations[currentAnimationId].tickperSec, animations[currentAnimationId].duration);
			{
				CPUProfiler boneProfile(CPUBoneUpdate);
				currentBoneMatrices.assign(boneCount, glm::mat4(1.0f));
				updateAndCalculateMatrix(tick);
			}
			
		}
		

		//Setting UBO offset
		glBindBuffer(GL_UNIFORM_BUFFER, UboMatrices);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4) * currentBoneMatrices.size(), currentBoneMatrices.data());

		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		for (int i = 0; i < meshesVAO.size(); i++)
		{

			glUniform1i(shaderInfo.ulUseNormal, (int)normalMesh[i].size());
			glUniform1f(shaderInfo.ulShininess, shininess);

			if (useDiffuseMap) {
				glUniform1i(shaderInfo.ulDiffuse, i);
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, meshesTexture[i]);
			}

			glBindVertexArray(meshesVAO[i]);
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(vIndicesMesh[i].size()), GL_UNSIGNED_INT, 0);

			glBindVertexArray(0);
		}

	}

	void setCurrentAnimationId(string animationName) {
		auto it = findAnimation.find(animationName);
		if (it != findAnimation.end()) currentAnimationId = it->second;
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

			if (!boneDataMesh[i].empty())
			{
				vector<glm::ivec4> boneIndicesPerVertex;
				vector<glm::vec4> boneWeightsPerVertex;

				boneIndicesPerVertex.reserve(boneDataMesh[i].size());
				boneWeightsPerVertex.reserve(boneDataMesh[i].size());

				for (const auto& vertexBoneData : boneDataMesh[i])
				{
					glm::ivec4 indices;
					glm::vec4 weights;
					for (int j = 0; j < 4; j++) {
						indices[j] = (j < vertexBoneData.boneIndex.size()) ? vertexBoneData.boneIndex[j] : 0;
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
		glBindBuffer(GL_UNIFORM_BUFFER, UboMatrices);
		glBufferData(GL_UNIFORM_BUFFER, 258 * sizeof(glm::mat4), reinterpret_cast<void*>(0), GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, UboMatrices);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
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
	void updateAndCalculateMatrix(double tick) {
		for (int nodeIdx = 0; nodeIdx < nodes.size(); nodeIdx++) {
			glm::mat4 localNodeTransform;
			glm::mat4 T;
			glm::mat4 R;
			glm::mat4 S;

			// Channel found: build the local transform from interpolated keys. Otherwise use the bind pose.
			int id = animations[currentAnimationId].nodeToChannel[nodeIdx];
			if (id != -1) {
				const Animation::Channel& channel = animations[currentAnimationId].channels[id];
				T = glm::translate(glm::mat4(1.0f), pickKey(channel.positionKeys, tick));
				R = glm::mat4_cast(pickKey(channel.rotationKeys, tick));
				S = glm::scale(glm::mat4(1.0f), pickKey(channel.scalingKeys, tick));
				localNodeTransform = T * R * S;
			}
			else localNodeTransform = nodes[nodeIdx].localBindTransform;
			int p = nodes[nodeIdx].parent;

			if (p == -1) nodeGlobalTransforms[nodeIdx] = localNodeTransform;
			else nodeGlobalTransforms[nodeIdx] = nodeGlobalTransforms[p] * localNodeTransform;
			
			int boneMappedIndex = nodeToBone[nodeIdx];
			if (boneMappedIndex != -1)
			{
				if (boneMappedIndex < currentBoneMatrices.size())
				{
					currentBoneMatrices[boneMappedIndex] = globalInverseRootTransform * nodeGlobalTransforms[nodeIdx] * meshBoneWithIndex[boneMappedIndex].boneMatrix;
				}
			}
		}
	}

	int saveBoneData(const aiNode* cur, int parentInfo) {
		if (!cur) return -1;
		SkeletonBone temp;
		temp.name = cur->mName.C_Str();
		temp.parent = parentInfo;
		temp.localBindTransform = aiMatToGlmMat(cur->mTransformation);
		nodes.push_back(std::move(temp));
		int currInfo = static_cast<int>(nodes.size()) - 1;
		for (unsigned int i = 0; i < cur->mNumChildren; i++) {
			int childrenInfo = saveBoneData(cur->mChildren[i], currInfo);
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