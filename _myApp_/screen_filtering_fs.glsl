#version 430 core

out vec4 fragColor;
  
in vec2 vsTexCoord;

uniform sampler2D screenTexture;
uniform int convolNum;

void main()
{
    float offsetX = 1.0f / float(textureSize(screenTexture, 0).x);  
    float offsetY = 1.0f / float(textureSize(screenTexture, 0).y);  

    vec2 offsets[9] = vec2[](
        vec2(-offsetX,  offsetY), // top-left
        vec2( 0.0f,     offsetY), // top-center
        vec2( offsetX,  offsetY), // top-right
        vec2(-offsetX,  0.0f),   // center-left
        vec2( 0.0f,     0.0f),   // center-center
        vec2( offsetX,  0.0f),   // center-right
        vec2(-offsetX, -offsetY), // bottom-left
        vec2( 0.0f,    -offsetY), // bottom-center
        vec2( offsetX, -offsetY)  // bottom-right    
    );

    float kernel[3][9] = float[3][9](
        float[](
            0.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 0.0
        ), 
        float[](
            -1.0, -1.0, -1.0,
            -1.0,  9.0, -1.0,
            -1.0, -1.0, -1.0
        ), 
        float[](
            2.0/17.0, 2.0/17.0, 2.0/17.0,
            2.0/17.0, 1.0/17.0, 2.0/17.0,
            2.0/17.0, 2.0/17.0, 2.0/17.0
        )
    );

    vec3 sampleTex[9];
    for(int i = 0; i < 9; i++)
    {
        sampleTex[i] = vec3(texture(screenTexture, vsTexCoord + offsets[i]));
    }

    int idx = clamp(convolNum, 0, 2);
    vec3 color = vec3(0.0);
    for(int i = 0; i < 9; i++)
        color += sampleTex[i] * kernel[idx][i];

   fragColor = vec4(color, 1.0);
    // fragColor = vec4(texture(screenTexture, vsTexCoord).rgb, 1.0);
}