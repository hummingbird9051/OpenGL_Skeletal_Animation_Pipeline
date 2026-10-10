#version 430 core

layout (location = 0) in vec3 pos;	
layout (location = 1) in vec2 texCoord;		
layout (location = 2) in vec3 normal;
layout (location = 3) in ivec4 boneId;
layout (location = 4) in vec4 boneWeight;

out vec3 vsPos;
out vec3 vsNormal;
out vec2 vsTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const int MAX_BONES = 258;
layout (std140, binding = 0) uniform BoneMatricesBlock {
    mat4 finalBoneMatrices[MAX_BONES];
} boneData;

void main()
{
    mat4 totalBoneTransform = mat4(vec4(1.0, 0.0, 0.0, 0.0), vec4(0.0, 1.0, 0.0, 0.0), vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));

    totalBoneTransform = boneWeight[0] * boneData.finalBoneMatrices[boneId[0]];
    for(int i = 1; i < 4; i++) {
        totalBoneTransform += boneWeight[i] * boneData.finalBoneMatrices[boneId[i]];
    }

    vec4 worldPos = model * totalBoneTransform * vec4(pos, 1.0);

    vsPos = vec3(worldPos);

    mat3 normalMatrix = mat3(transpose(inverse(model)) * totalBoneTransform);
    vsNormal = normalize(normalMatrix * normal);
	vsTexCoord = texCoord;
    
    gl_Position = projection * view * worldPos;
}