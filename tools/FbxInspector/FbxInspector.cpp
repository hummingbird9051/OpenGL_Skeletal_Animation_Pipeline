#include<assimp/Importer.hpp>
#include<assimp/scene.h>
#include<assimp/postprocess.h>
#include<assimp/mesh.h>
#include<assimp/material.h>
#include<iostream>
#include<stack>
#include<unordered_map>
#include<string>

using namespace std;

int main() {
	Assimp::Importer ipt; // Assimp Importer variable
	ipt.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false); // Merge $AssimpFbx$ helper nodes (PreRotation, Translation...) into each bone. Skeleton and animation files must use the same setting.
	const aiScene* scene = ipt.ReadFile("assets/anim/Walking.fbx", 0); // import scene(FBX metadata)

	if (!scene) { // if FBX file is not valid or available, return
		cout << "Failed to load : " << ipt.GetErrorString() << endl;
		return 1;
	}

	int countNodes = 0;
	int countAllBonesReference = 0;
	int countBones = 0; // need to check amount of Bones, References, Nodes
	int countAnim = scene->mNumAnimations;
	cout << "Animation Count : " << countAnim << endl;

	//animation print
	for (int i = 0; i < scene->mNumAnimations; i++) {
		aiAnimation* anim = scene->mAnimations[i];
		int duration = anim->mDuration;
		cout << "Durations : " << duration << endl;
		cout << "Ticks Per Seconds : " << anim->mTicksPerSecond << endl;
		cout << "Non-Seamless Channel info : " << endl;
		int seamless = 0;
		double maxDifferenceCh = 0;
		string maxCh = "";
		for(int ch = 0; ch < anim->mNumChannels; ch++) {
			aiNodeAnim* channel = anim->mChannels[ch];
			//cout << "  Animation Channel's PositionKeys Counts : " << channel->mNumPositionKeys << endl;
			vector<double> diff(4, 0);
			diff[0] = channel->mRotationKeys[29].mValue.w - channel->mRotationKeys[0].mValue.w;
			diff[1] = channel->mRotationKeys[29].mValue.x - channel->mRotationKeys[0].mValue.x;
			diff[2] = channel->mRotationKeys[29].mValue.y - channel->mRotationKeys[0].mValue.y;
			diff[3] = channel->mRotationKeys[29].mValue.z - channel->mRotationKeys[0].mValue.z;
			double maxDiff = max({ diff[0], diff[1], diff[2], diff[3] });
			if (maxDifferenceCh < maxDiff) {
				maxDifferenceCh = maxDiff;
				maxCh = channel->mNodeName.C_Str();
			}
			bool isSeamless = (maxDiff <= 0.001);
			if (!isSeamless) {
				cout << "Channel : " << ch << ", Name : " << channel->mNodeName.C_Str() << endl;
				cout << "max difference in this channel : " << maxDiff << endl;
				cout << endl;
				seamless += 1;
			}

			//cout << "  Animation Channel's RotationKeys Counts : " << channel->mNumRotationKeys << endl;
			//cout << "  Animation Channel's ScalingKeys Counts : " << channel->mNumScalingKeys << endl;
		}

		if (seamless == 0) cout << "  NONE" << endl;
		cout << "Loop Check :  max diff " << maxDifferenceCh << "( " << maxCh << " ) -> ";
		if (maxDifferenceCh < 0.001)  cout << "Seamless" << endl;
		else cout << "not Seamless" << endl;
	}

	/*
	//bone print
	unordered_map<string, int> bones; // bone name -> number of meshes that reference it (skinning bones only)
	for (int i = 0; i < scene->mNumMeshes; i++) {
		countAllBonesReference += scene->mMeshes[i]->mNumBones;
		for (int j = 0; j < scene->mMeshes[i]->mNumBones; j++) {
			bones[scene->mMeshes[i]->mBones[j]->mName.C_Str()] += 1;
		}
	}
	countBones = bones.size();
	
	stack<pair<aiNode*, int>> nodes; // Tree Search with pair(node, depth) 
	nodes.push({ scene->mRootNode, 0 });
	while (!nodes.empty()) {
		const aiNode* curNode = nodes.top().first;
		int curDepth = nodes.top().second;
		nodes.pop();
		
		if (!curNode) continue;
		countNodes++;
		for (int i = curNode->mNumChildren - 1; i >= 0; i--) {
			nodes.push({ curNode->mChildren[i], curDepth + 1 });
		}

		string blank(curDepth, ' '); // easily find the hierarchy

		string name = curNode->mName.C_Str();
		int calledCount = 0;

		auto it = bones.find(name);
		calledCount = it != bones.end() ? it->second : 0; // if bones has name, put the count
		
		cout << blank << name << " (bone X " << calledCount << ") " << endl;
	}

	cout << "NodeCounts : " << countNodes << endl;
	cout << "BoneAllReferenceCounts : " << countAllBonesReference << endl;
	cout << "BoneCounts : " << countBones << endl;
	
	
	*/
}