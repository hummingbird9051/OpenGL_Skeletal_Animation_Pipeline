#pragma once
#include <iostream>
#include<map>
#include "Model.h"

using namespace std;
using namespace vmath;


struct Bone
{
	mat4 boneMatrix;
};

struct VertexBoneData
{
	vector<float> weight;
	vector<int> boneIndex;
	vector<string> name;
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
	};

	struct SkeletonBone {
		string name;
		glm::mat4 localBindTransform = glm::mat4(1.0f);
		int parent = -1;
		vector<int> children;
	};

	vector<Animation> animations;

	vector<SkeletonBone> nodes;

	mat4 globalInverseRootTransform;

	vector<mat4> currentBoneMatrices; //save bone matrices each frame

	vector<GLuint> meshesVAO;
	vector<GLuint> meshesVBOPosition;
	vector<GLuint> meshesVBONormal;
	vector<GLuint> meshesVBOTexCoord;
	vector<GLuint> meshesVBOBoneID;
	vector<GLuint> meshesVBOWeights;
	GLuint UboMatrices;

	vector<GLuint> meshesTexture;

	vector<vector<vec3>> positionMesh;
	vector<vector<vec2>> texCoordMesh;
	vector<vector<vec3>> normalMesh;
	vector<vector<VertexBoneData>> boneDataMesh;					//vbo로 넘길 버텍스별 본과의 관계

	map<string, Bone> meshBone;

	const mat4 Identity = mat4(vec4(1.0, 0.0, 0.0, 0.0), vec4(0.0, 1.0, 0.0, 0.0), vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));

	int boneCount;

	vector<string> boneName;
	map<string, int> boneNameMap;

	void loadAnimation(const aiAnimation* anim) {
		Animation animation;
		animation.duration = anim->mDuration;
		animation.tickperSec = anim->mTicksPerSecond > 0.0 ? anim->mTicksPerSecond : 25.0;
		animation.channels.resize(static_cast<size_t>(anim->mNumChannels));
		//collect by channel
		for (int ch = 0; ch < anim->mNumChannels; ch++) {
			aiNodeAnim* nodeAnim = anim->mChannels[ch];
			//collecting information by keys
			animation.channels[ch].name = nodeAnim->mNodeName.C_Str();
			animation.channelIndexByName[nodeAnim->mNodeName.C_Str()] = ch; // collect node name with channel number
			animation.channels[ch].positionKeys.resize(nodeAnim->mNumPositionKeys);
			animation.channels[ch].rotationKeys.resize(nodeAnim->mNumRotationKeys);
			animation.channels[ch].scalingKeys.resize(nodeAnim->mNumScalingKeys);
			for (int posKey = 0; posKey < nodeAnim->mNumPositionKeys; posKey++) {
				animation.channels[ch].positionKeys[posKey].value.x = nodeAnim->mPositionKeys[posKey].mValue.x;
				animation.channels[ch].positionKeys[posKey].value.y = nodeAnim->mPositionKeys[posKey].mValue.y;
				animation.channels[ch].positionKeys[posKey].value.z = nodeAnim->mPositionKeys[posKey].mValue.z;
				animation.channels[ch].positionKeys[posKey].time = nodeAnim->mPositionKeys[posKey].mTime;
			}
			for (int rotKey = 0; rotKey < nodeAnim->mNumRotationKeys; rotKey++) {
				animation.channels[ch].rotationKeys[rotKey].value.w = nodeAnim->mRotationKeys[rotKey].mValue.w;
				animation.channels[ch].rotationKeys[rotKey].value.x = nodeAnim->mRotationKeys[rotKey].mValue.x;
				animation.channels[ch].rotationKeys[rotKey].value.y = nodeAnim->mRotationKeys[rotKey].mValue.y;
				animation.channels[ch].rotationKeys[rotKey].value.z = nodeAnim->mRotationKeys[rotKey].mValue.z;
				animation.channels[ch].rotationKeys[rotKey].time = nodeAnim->mRotationKeys[rotKey].mTime;
			}
			for (int scaleKey = 0; scaleKey < nodeAnim->mNumScalingKeys; scaleKey++) {
				animation.channels[ch].scalingKeys[scaleKey].value.x = nodeAnim->mScalingKeys[scaleKey].mValue.x;
				animation.channels[ch].scalingKeys[scaleKey].value.y = nodeAnim->mScalingKeys[scaleKey].mValue.y;
				animation.channels[ch].scalingKeys[scaleKey].value.z = nodeAnim->mScalingKeys[scaleKey].mValue.z;
				animation.channels[ch].scalingKeys[scaleKey].time = nodeAnim->mScalingKeys[scaleKey].mTime;
			}
		}

		animations.push_back(animation);
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

		//Collect Mesh Information
		boneDataMesh.resize(scene->mNumMeshes);
		for (int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++)
		{
			const aiMesh* mesh = scene->mMeshes[meshIdx];
			vector<vmath::vec3> tmpPositions;
			vector<vmath::vec2> tmpTexCoords;
			vector<vmath::vec3> tmpNormals;
			vector<GLuint> tmpVIndices;
			vector<VertexBoneData> tmpBoneData;


			vector<vmath::vec3> meshPositions;
			vector<vmath::vec2> meshTexCoords;
			vector<vmath::vec3> meshNormals;

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
				tmpPositions.push_back(vec3(vPosition[0], vPosition[1], vPosition[2]));
				vNormal.x = mesh->mNormals[i].x;
				vNormal.y = mesh->mNormals[i].y;
				vNormal.z = mesh->mNormals[i].z;
				tmpNormals.push_back(vec3(vNormal[0], vNormal[1], vNormal[2]));
				if (mesh->HasTextureCoords(0))
				{
					vTexCoord.x = mesh->mTextureCoords[0][i].x;
					vTexCoord.y = mesh->mTextureCoords[0][i].y;
				}
				else
				{
					vTexCoord = aiVector2D(0.0f, 0.0f);
				}
				tmpTexCoords.push_back(vec2(vTexCoord.x, vTexCoord.y));

			}



			for (unsigned i = 0; i < mesh->mNumFaces; ++i)
			{
				const aiFace& face = mesh->mFaces[i];
				for (unsigned k = 0; k < face.mNumIndices; ++k)
				{
					vec3 position = tmpPositions[face.mIndices[k]];
					vec3 normal = tmpNormals[face.mIndices[k]];
					vec2 texCoord = tmpTexCoords[face.mIndices[k]];
					tmpVIndices.push_back(face.mIndices[k]);
					meshPositions.push_back(position);
					meshNormals.push_back(normal);
					meshTexCoords.push_back(texCoord);
				}
			}
			positionMesh.push_back(meshPositions);
			normalMesh.push_back(meshNormals);
			texCoordMesh.push_back(meshTexCoords);


			meshesVAO.push_back(0);
			meshesVBOPosition.push_back(0);
			meshesVBONormal.push_back(0);
			meshesVBOTexCoord.push_back(0);
			meshesVBOBoneID.push_back(0);
			meshesVBOWeights.push_back(0);
			meshesTexture.push_back(0);

			for (int boneMeshId = 0; boneMeshId < mesh->mNumBones; boneMeshId++)
			{
				aiBone* bone_mesh = mesh->mBones[boneMeshId];
				if (find(boneName.begin(), boneName.end(), bone_mesh->mName.C_Str()) == boneName.end())
				{
					boneName.push_back(bone_mesh->mName.C_Str());
				}
				string name_of_current_aiBone = bone_mesh->mName.C_Str();
				for (int id = 0; id < bone_mesh->mNumWeights; id++) {
					int vertexId = bone_mesh->mWeights[id].mVertexId;
					float weight_val = bone_mesh->mWeights[id].mWeight;


					if (tmpBoneData[vertexId].name.size() < 4) {
						tmpBoneData[vertexId].name.push_back(name_of_current_aiBone);
						tmpBoneData[vertexId].weight.push_back(weight_val);
						tmpBoneData[vertexId].boneIndex.push_back(0);
					}
				}
				meshBone[name_of_current_aiBone].boneMatrix = aiMatToVmathMat(bone_mesh->mOffsetMatrix);
			}

			for (int vIdx = 0; vIdx < tmpVIndices.size(); vIdx++)
			{
				VertexBoneData& currentBoneData = tmpBoneData[tmpVIndices[vIdx]];
				while (currentBoneData.weight.size() < 4) {
					currentBoneData.weight.push_back(0.0f);
				}
				while (currentBoneData.boneIndex.size() < 4) {
					currentBoneData.boneIndex.push_back(0);
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
		boneCount = boneName.size();

		//refactor: Bone data saving(from scene)
		currentBoneMatrices.assign(boneCount, Identity);
		nodes.clear();
		saveBoneData(scene->mRootNode, -1);
		aiMatrix4x4 invRootTransformAssimp = scene->mRootNode->mTransformation;
		invRootTransformAssimp = invRootTransformAssimp.Inverse();
		globalInverseRootTransform = aiMatToVmathMat(invRootTransformAssimp);

		//Collect Animation informaiton from FBX Bone file
		for (int animIdx = 0; animIdx < scene->mNumAnimations; animIdx++) {
			loadAnimation(scene->mAnimations[animIdx]);
		}

		// ..from Another Animation file
		for (int fileCount = 0; fileCount < animationFileName.size(); fileCount++) {
			const aiScene* animScene = animationImporter.ReadFile(animationFileName[fileCount].c_str(), flags);
			if (!animScene)
			{
				cerr << animationFileName[fileCount] << " : Animation Import Error: " << animationImporter.GetErrorString() << std::endl;
				continue;
			}
			if (animScene->mNumAnimations == 0) {
				cerr << animationFileName[fileCount] << " : No animation found" << endl;
				continue;
			}
			for (int animIdx = 0; animIdx < animScene->mNumAnimations; animIdx++) {
				loadAnimation(animScene->mAnimations[animIdx]);
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

		this->init();
		prepareBuffers();
		loadDiffuseMap("");
	}

	void draw(GLuint _shaderID, double currentTime) override {
		glUniform3fv(glGetUniformLocation(_shaderID, "material.defaultAmbient"), 1, defaultAmbient);
		glUniform3fv(glGetUniformLocation(_shaderID, "material.defaultDiffuse"), 1, defaultDiffuse);
		glUniform3fv(glGetUniformLocation(_shaderID, "material.defaultSpecular"), 1, defaultSpecular);
		glUniform1i(glGetUniformLocation(_shaderID, "material.useDiffuseMap"), (int)useDiffuseMap);
		glUniform1i(glGetUniformLocation(_shaderID, "material.useSpecularMap"), (int)useSpecularMap);

		//calculate final bon matrices each frame
		if (!animations.empty()) {
			double tick = fmod(currentTime * animations[0].tickperSec, animations[0].duration);
			currentBoneMatrices.assign(boneCount, Identity);
			calculateBoneTransformRecursive(tick, 0, Identity, currentBoneMatrices, globalInverseRootTransform);
		}

		//Setting UBO offset
		glBindBuffer(GL_UNIFORM_BUFFER, UboMatrices);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(mat4) * currentBoneMatrices.size(), currentBoneMatrices.data());

		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		for (int i = 0; i < meshesVAO.size(); i++)
		{

			glUniform1i(glGetUniformLocation(_shaderID, "useNormal"), (int)normalMesh[i].size());
			glUniform1f(glGetUniformLocation(_shaderID, "material.shininess"), shininess);

			if (useDiffuseMap) {
				glUniform1i(glGetUniformLocation(_shaderID, "material.diffuse"), i);
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, meshesTexture[i]);
			}

			glBindVertexArray(meshesVAO[i]);
			glDrawArrays(GL_TRIANGLES, 0, positionMesh[i].size());

			glBindVertexArray(0);
		}

	}

protected:
	void prepareBuffers() override
	{
		for (int i = 0; i < meshesVAO.size(); i++)
		{
			glBindVertexArray(meshesVAO[i]);
			glBindBuffer(GL_ARRAY_BUFFER, meshesVBOPosition[i]);
			glBufferData(GL_ARRAY_BUFFER, positionMesh[i].size() * sizeof(vmath::vec3), &positionMesh[i][0], GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), reinterpret_cast<void*>(0));
			glEnableVertexAttribArray(0);

			if (!texCoordMesh[i].empty())
			{
				glBindBuffer(GL_ARRAY_BUFFER, meshesVBOTexCoord[i]);
				glBufferData(GL_ARRAY_BUFFER, texCoordMesh[i].size() * sizeof(vmath::vec2), &texCoordMesh[i][0], GL_STATIC_DRAW);
				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), reinterpret_cast<void*>(0));
				glEnableVertexAttribArray(1);
			}
			
			if (!normalMesh[i].empty())
			{
				glBindBuffer(GL_ARRAY_BUFFER, meshesVBONormal[i]);
				glBufferData(GL_ARRAY_BUFFER, normalMesh[i].size() * sizeof(vmath::vec3), &normalMesh[i][0], GL_STATIC_DRAW);
				glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), reinterpret_cast<void*>(0));
				glEnableVertexAttribArray(2);
			}

			if (!boneDataMesh[i].empty())
			{
				vector<ivec4> boneIndicesPerVertex;
				vector<vec4> boneWeightsPerVertex;

				boneIndicesPerVertex.reserve(boneDataMesh[i].size());
				boneWeightsPerVertex.reserve(boneDataMesh[i].size());

				for (const auto& vertexBoneData : boneDataMesh[i])
				{
					ivec4 indices;
					vec4 weights;
					for (int j = 0; j < 4; j++) {
						indices[j] = (j < vertexBoneData.boneIndex.size()) ? vertexBoneData.boneIndex[j] : 0;
						weights[j] = (j < vertexBoneData.weight.size()) ? vertexBoneData.weight[j] : 0;
					}
					boneIndicesPerVertex.push_back(indices);
					boneWeightsPerVertex.push_back(weights);
				}
				
				glBindBuffer(GL_ARRAY_BUFFER, meshesVBOBoneID[i]);
				glBufferData(GL_ARRAY_BUFFER, boneIndicesPerVertex.size() * sizeof(ivec4), boneIndicesPerVertex.data(), GL_STATIC_DRAW);
				glVertexAttribIPointer(3, 4, GL_INT, sizeof(ivec4), reinterpret_cast<void*>(0));
				glEnableVertexAttribArray(3);

				glBindBuffer(GL_ARRAY_BUFFER, meshesVBOWeights[i]);
				glBufferData(GL_ARRAY_BUFFER, boneWeightsPerVertex.size() * sizeof(vec4), boneWeightsPerVertex.data(), GL_STATIC_DRAW);
				glVertexAttribPointer(4, 4, GL_FLOAT,GL_FALSE, sizeof(vec4), reinterpret_cast<void*>(0));
				glEnableVertexAttribArray(4);
			}

			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
		}
		glBindBuffer(GL_UNIFORM_BUFFER, UboMatrices);
		glBufferData(GL_UNIFORM_BUFFER, 258 * sizeof(mat4), reinterpret_cast<void*>(0), GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, UboMatrices);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}	


private:
	//assimp 라이브러리와 vmath 라이브러리의 행기반 열기반 행렬 변환
	//TODO: convert to Universal matrix for GLM.
	mat4 aiMatToVmathMat(aiMatrix4x4 aiMat)
	{
		mat4 result;
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				result[j][i] = aiMat[i][j];
			}
		}
		return result;
	}

	//Affect all bone using one calculate
	void calculateBoneTransformRecursive(
		double tick, int nodeIdx, 
		const mat4& parentGlobalTransform, 
		vector<mat4>& outFinalBoneMatrices, 
		const mat4& globalInverseRootTransform)
	{
		string nodeName = nodes[nodeIdx].name;
		mat4 localNodeTransform;
		glm::mat4 T;
		glm::mat4 R;
		glm::mat4 S;

		// Channel found: build the local transform from interpolated keys. Otherwise use the bind pose.
		auto it = animations[0].channelIndexByName.find(nodeName);
		if (it != animations[0].channelIndexByName.end()) {
			Animation::Channel& channel = animations[0].channels[it->second];
			T = glm::translate(glm::mat4(1.0f), pickKey(channel.positionKeys, tick));
			R = glm::mat4_cast(pickKey(channel.rotationKeys, tick));
			S = glm::scale(glm::mat4(1.0f), pickKey(channel.scalingKeys, tick));
			localNodeTransform = glmMatToVmathMat(T * R * S);
		}
		else localNodeTransform = glmMatToVmathMat(nodes[nodeIdx].localBindTransform);
		
		mat4 currentGlobalTransform = parentGlobalTransform * localNodeTransform;
		
		if (meshBone.count(nodeName))
		{
			int boneMappedIndex = boneNameMap[nodeName];
			if (boneMappedIndex < outFinalBoneMatrices.size())
			{
				outFinalBoneMatrices[boneMappedIndex] = globalInverseRootTransform * currentGlobalTransform * meshBone[nodeName].boneMatrix;
			}
		}

		for (unsigned int i = 0; i < nodes[nodeIdx].children.size(); i++)
		{
			calculateBoneTransformRecursive(tick, nodes[nodeIdx].children[i], currentGlobalTransform, outFinalBoneMatrices, globalInverseRootTransform);
		}
	}

	mat4 glmMatToVmathMat(glm::mat4 g) {
		mat4 temp;
		for (int col = 0; col < 4; col++) {
			for (int row = 0; row < 4; row++) {
				temp[col][row] = g[col][row];
			}
		}

		return temp;
	}

	int saveBoneData(const aiNode* cur, int parentInfo) {
		if (!cur) return -1;
		SkeletonBone temp;
		temp.name = cur->mName.C_Str();
		temp.parent = parentInfo;
		temp.localBindTransform = aiMatToGlmMat(cur->mTransformation);
		nodes.push_back(std::move(temp));
		int currInfo = nodes.size() - 1;
		for (int i = 0; i < cur->mNumChildren; i++) {
			int childrenInfo = saveBoneData(cur->mChildren[i], currInfo);
			if (childrenInfo >= 0) nodes[currInfo].children.push_back(childrenInfo);
		}
		return currInfo;
	}

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
		int right = keys.size() - 1;
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
		int right = keys.size() - 1;
		while ((left + 1 <= keys.size() - 1 && right - 1 >= 0) && right - left != 1) {
			if (keys[left + 1].time <= tickTime) left++;
			if (keys[right - 1].time > tickTime) right--;
		}

		double portion = (tickTime - keys[left].time) / (keys[right].time - keys[left].time);
		return glm::slerp(keys[left].value, keys[right].value, static_cast<float>(portion));
	}
};