#pragma once
#include"Model.h"


class PostProcessing
{
private:
	GLuint texture;
	GLuint vao;
	GLuint vbo;
	GLuint fbo;
	GLuint rbo;

public:

	//생성자로 new PostProcessing() 실행 시 바로 메모리 확보
	PostProcessing()
	{
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
		glGenFramebuffers(1, &fbo);
		glGenTextures(1, &texture);
		glGenRenderbuffers(1, &rbo);
	}

	//작업이 끝나면 자연스럽게 메모리 해제를 위한 작업
	~PostProcessing()
	{
		glDeleteVertexArrays(1, &vao);
		glDeleteBuffers(1, &vbo);
		glDeleteFramebuffers(1, &fbo);
		glDeleteTextures(1, &texture);
		glDeleteRenderbuffers(1, &rbo);
	}

	//스크린을 그리기 위한 준비
	void initScreen()
	{
		GLfloat screen[]
		{
			-1.0f, 1.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f,
			1.0f, -1.0f, 1.0f, 0.0f,

			-1.0f, 1.0f, 0.0f, 1.0f,
			1.0f, -1.0f, 1.0f, 0.0f,
			1.0f, 1.0f, 1.0f, 1.0f
		};
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(screen), screen, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	//프레임버퍼 설정
	void initFrameBuffer(const int& screenWidth, const int& screenHeight)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screenWidth, screenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, screenWidth, screenHeight);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			glfwTerminate();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void bindFrameBuffer()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void unbindFrameBuffer()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDisable(GL_DEPTH_TEST);
		glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void drawScreen(GLuint screenShaderProgram, int convolNum)
		// TODO: [Performance / CPU Bottleneck] glGetUniformLocation is being called every frame inside drawScreen().
		// Refactoring plan: Move these lookups to the initialization phase and cache the uniform IDs 
		// to prevent expensive per-frame string searches.
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glUseProgram(screenShaderProgram);
		glUniform1i(glGetUniformLocation(screenShaderProgram, "screenTexture"), 0);
		glUniform1i(glGetUniformLocation(screenShaderProgram, "convolNum"), convolNum);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}


	//화면 크기 재설정시 생기는 오차를 방지
	void screenResize(int w, int h) {
		glBindTexture(GL_TEXTURE_2D, texture);
		// TODO: [Bug Fix] Fix incorrect ID binding in screenResize().
		// Currently binding 'fbo' to GL_TEXTURE_2D, which causes memory corruption. 
		// Must change to glBindTexture(GL_TEXTURE_2D, texture).
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
};