#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION

#include"stb_image.h"
bool loadTextureFile(GLuint textureID, const char* filepath) {
	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true); // Do NOT convert this we need to use stb_image.h
	unsigned char* data = stbi_load(filepath, &width, &height, &nrChannels, 0);

	if (data) {
		GLenum format = 0;
		if (nrChannels == 1)
			format = GL_RED;
		else if (nrChannels == 3) // R, G, B (ex. jpg)
			format = GL_RGB;
		else if (nrChannels == 4) // PNG
			format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);

		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
		return true;
	}

	stbi_image_free(data);
	return false;
}