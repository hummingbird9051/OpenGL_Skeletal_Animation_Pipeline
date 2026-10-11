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

	double animDuration;

	vector<vector<mat4>> uniformBoneMat;

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

	void loadFBX(const char* objectFileName, const char* animationFileName = nullptr) override
	{
		Assimp::Importer objectImporter;
		unsigned int flags = aiProcess_GenSmoothNormals |
			aiProcess_CalcTangentSpace |
			aiProcess_SortByPType |
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_FindInvalidData |
			aiProcess_FindDegenerates |
			aiProcess_ImproveCacheLocality;
		const aiScene* scene = objectImporter.ReadFile(objectFileName, flags);
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			cerr << "Assimp Error: " << objectImporter.GetErrorString() << std::endl;
			return;
		}

		map<string, int> tmpBoneNameMap;
		int boneCounter{ 0 };

		boneDataMesh.resize(scene->mNumMeshes);
		for (int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++)
		{
			const aiMesh* mesh = scene->mMeshes[meshIdx];
			// 정점 데이터(노멀 벡터 포함)
			vector<vmath::vec3> tmpPositions;
			vector<vmath::vec2> tmpTexCoords;
			vector<vmath::vec3> tmpNormals;
			vector<GLuint> tmpVIndices;
			vector<VertexBoneData> tmpBoneData;
			

			vector<vmath::vec3> meshPositions;
			vector<vmath::vec2> meshTexCoords;
			vector<vmath::vec3> meshNormals;

			//cout << mesh->mName.C_Str() << std::endl;
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


			//FBX는 정점 정보, 노말 정보, 텍스쳐 좌표정보 등이 다 인덱스로 매핑 되어있음.
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

		cout << boneName.size() << endl;
		boneNameMap.clear();
		for (int i = 0; i < boneName.size(); i++)
		{
			boneNameMap[boneName[i]] = i;
		}
		boneCount = boneName.size();

		//프레임 인터폴레이션 기술 사용 실패로 프레임 자체를 늘려 사용
		animDuration = 258;
		aiMatrix4x4 invRootTransformAssimp = scene->mRootNode->mTransformation;
		invRootTransformAssimp = invRootTransformAssimp.Inverse();
		mat4 globalInverseRootTransform = aiMatToVmathMat(invRootTransformAssimp);
		uniformBoneMat.clear();
		uniformBoneMat.resize(static_cast<size_t>(animDuration) + 1);

		for (int tick = 0; tick <= static_cast<int>(animDuration); tick++)
		{
			uniformBoneMat[tick].assign(boneCount, Identity);
			calculateBoneTransformRecursive(static_cast<float>(tick), scene->mRootNode, Identity, uniformBoneMat[tick], globalInverseRootTransform, scene);
		}

		//버텍스별 본 가중치와 본 인덱스 값 저장.
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

	void draw(GLuint _shaderID, float currentTime) override {
		glUniform3fv(glGetUniformLocation(_shaderID, "material.defaultAmbient"), 1, defaultAmbient);
		glUniform3fv(glGetUniformLocation(_shaderID, "material.defaultDiffuse"), 1, defaultDiffuse);
		glUniform3fv(glGetUniformLocation(_shaderID, "material.defaultSpecular"), 1, defaultSpecular);
		glUniform1i(glGetUniformLocation(_shaderID, "material.useDiffuseMap"), (int)useDiffuseMap);
		glUniform1i(glGetUniformLocation(_shaderID, "material.useSpecularMap"), (int)useSpecularMap);

		//UBO 오프셋 설정
		int tick = 0;
		if (animDuration > 0)
		{
			float normalizedTime = fmod(currentTime * 258, animDuration);
			tick = static_cast<int>(normalizedTime);
		}
		glBindBuffer(GL_UNIFORM_BUFFER, UboMatrices);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(mat4) * uniformBoneMat[tick].size(), uniformBoneMat[tick].data());

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

	//재귀로 접근해 하나의 본을 움직이면 다른 본에도 영향을 가도록 만듦.
	// TODO: [Animation System & Performance] Remove hardcoded procedural animation (sin math on specific Mixamo bones).
	// Refactoring plan: 
	// 1. [Data-Driven] Parse aiAnimation data from the FBX file and interpolate Keyframes (Position/Rotation/Scale) based on tick.
	// 2. [Performance] Eliminate expensive std::string map lookups (boneNameMap, meshBone) inside this per-frame recursive loop. Replace with flat index arrays.
	// 3. [Maintenance] Replace aiMatToVmathMat with GLM integration.
	void calculateBoneTransformRecursive(
		float tick,const aiNode* pNode, 
		const mat4& parentGlobalTransform, 
		vector<mat4>& outFinalBoneMatrices, 
		const mat4& globalInverseRootTransform, 
		const aiScene* scene)
	{
		string nodeName = pNode->mName.C_Str();
		mat4 localNodeTransform;
		mat4 R = Identity;
		int boneMappedIndex = boneNameMap[nodeName];

		if (nodeName == "mixamorig:RightUpLeg")
			R = rotate(static_cast<float>(20.0f * sin(tick / 40)), 1.0f, 0.0f, 0.0f);
		if (nodeName == "mixamorig:LeftUpLeg")
			R = rotate(static_cast<float>(-20.0f * sin(tick / 40)), 1.0f, 0.0f, 0.0f);
		if (nodeName == "mixamorig:LeftArm")
			R =  rotate(-70.0f, -1.0f, 0.0f, 0.0f) * rotate(static_cast<float>(10.0f * sin(tick / 40)), 0.0f, 0.0f, 1.0f);
		if (nodeName == "mixamorig:RightArm")
			R = rotate(70.0f, 1.0f, 0.0f, 0.0f) * rotate(static_cast<float>(10.0f * sin(tick / 40)), 0.0f, 0.0f, 1.0f);
		if (nodeName == "mixamorig:LeftForeArm")
			R = rotate(10.0f * (static_cast<float>(sin(tick / 40)) + 0.5f), 0.0f, 0.0f, 1.0f);
		if (nodeName == "mixamorig:RightForeArm")
			R = rotate(10.0f * (static_cast<float>(sin(tick / 40)) + 0.5f), 0.0f, 0.0f, 1.0f);
		localNodeTransform = aiMatToVmathMat(pNode->mTransformation) * R;

		mat4 currentGlobalTransform = parentGlobalTransform * localNodeTransform;
		
		if (meshBone.count(nodeName))
		{
			int boneMappedIndex = boneNameMap[nodeName];
			if (boneMappedIndex < outFinalBoneMatrices.size())
			{
				outFinalBoneMatrices[boneMappedIndex] = globalInverseRootTransform * currentGlobalTransform * meshBone[nodeName].boneMatrix;
			}
		}

		for (unsigned int i = 0; i < pNode->mNumChildren; i++)
		{
			calculateBoneTransformRecursive(tick, pNode->mChildren[i], currentGlobalTransform, outFinalBoneMatrices, globalInverseRootTransform, scene);
		}
	}
};