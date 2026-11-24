#version 430 core
#define MAX_JOINTS 52
layout (location = 0) in vec3 pos;	
layout (location = 1) in vec2 texCoord;		
layout (location = 2) in vec3 normal;
layout (location = 3) in ivec4 jointIds;
layout (location = 4) in vec4 jointWeights;

out vec3 vsPos;
out vec3 vsNormal;
out vec2 vsTexCoord;

uniform mat4 view;
uniform mat4 projection;

layout (std140, binding = 0) uniform SkinningBlock {
    mat4 skinningMatrices[MAX_JOINTS];
} boneData;

layout (std430, binding = 1) buffer InstancedBlock {
    mat4 models[];
};

void main()
{
    mat4 model = models[gl_InstanceID];

    mat4 skinMatrix = jointWeights[0] * boneData.skinningMatrices[jointIds[0]];

    for(int i = 1; i < 4; i++) {
        skinMatrix += jointWeights[i] * boneData.skinningMatrices[jointIds[i]];
    }

    vec4 worldPos = model * skinMatrix * vec4(pos, 1.0);

    vsPos = vec3(worldPos);

    
   mat3 normalMatrix = mat3(model * skinMatrix);
    vsNormal = normalize(normalMatrix * normal);
	vsTexCoord = texCoord;
    
    gl_Position = projection * view * worldPos;
}