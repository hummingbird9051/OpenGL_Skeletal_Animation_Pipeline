#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<iostream>
#include<fstream>
#include<string>
#include<vector>
#include<memory>
#include<glm/glm.hpp>
#include <vmath.h>

#include "FBXModel.h"
#include "PostProcessing.h"

using namespace std;

static void glfwErrorCallback(int error, const char* description) {
	cerr << "GLFW error" << error << ": " << description << endl;
}

string readFile(const char* filename) { //read glsl files with fstream
	string allLines;
	string line;
	ifstream f(filename);
	if (f.is_open()) {
		while (getline(f, line)) {
			allLines.append(line);
			allLines += '\n';
		}
	}

	else return "";
	return allLines;
}

GLuint getShaderFile(const char* filename, GLenum shader_type) {
	string sSource = readFile(filename);
	if (sSource == "") {
		cerr << "Fail to load shaderSource (" << filename << ")" << endl;
		return 0;
	}
	GLuint shader = glCreateShader(shader_type);
	const char* str = sSource.c_str();
	GLsizei len = static_cast<GLsizei>(sSource.size());

	glShaderSource(shader, 1, &str, &len);
	glCompileShader(shader);

	GLint isSucceed;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isSucceed);
	if (isSucceed == GL_FALSE) {
		GLint logLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

		vector<GLchar> log(logLength);
		glGetShaderInfoLog(shader, logLength, nullptr, log.data());

		cerr << "Shader compile failed (" << filename << "):\n" << log.data() << endl;
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

GLuint getProgram(const char* vsFile, const char* fsFile) {

	GLuint vs_shader = getShaderFile(vsFile, GL_VERTEX_SHADER);
	GLuint fs_shader = getShaderFile(fsFile, GL_FRAGMENT_SHADER);

	if (!vs_shader || !fs_shader) {
		glDeleteShader(fs_shader);
		glDeleteShader(vs_shader);
		return 0;
	}

	GLuint program = glCreateProgram();

	glAttachShader(program, vs_shader);
	glAttachShader(program, fs_shader);

	glLinkProgram(program);

	glDeleteShader(vs_shader);
	glDeleteShader(fs_shader);

	GLint isLinkSucceed;
	glGetProgramiv(program, GL_LINK_STATUS, &isLinkSucceed);

	if (isLinkSucceed == GL_FALSE) {
		GLint logLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

		vector<GLchar> log(logLength);
		glGetProgramInfoLog(program, logLength, nullptr, log.data());

		cerr << "Program Link failed ( " << vsFile << ", " << fsFile << "):\n" << log.data() << endl;
		glDeleteProgram(program);
		return 0;
	}


	return program;
}

class my_application
{
public:
	my_application() {
		initValues();
	}

	void startup()
	{
		objModel = new FBXModel();
		pyramidModel = new Model();
		plateModel = new Model();
		post = new PostProcessing();

		// 쉐이더 프로그램 컴파일 및 연결
		shader_program = getProgram("Model_vs.glsl", "Model_fs.glsl");
		shader_light_program = getProgram("Light_vs.glsl", "Light_fs.glsl");
		shader_postprocessing_program = getProgram("screen_filtering_vs.glsl", "screen_filtering_fs.glsl");
		// startup() 내부에서 셰이더 컴파일 직후
		/*std::cout << "Shader Program ID: " << shader_program << std::endl;
		std::cout << "Shader Program ID: " << shader_light_program << std::endl;
		std::cout << "Shader Program ID: " << shader_postprocessing_program << std::endl;*/
		post->initScreen();
		post->initFrameBuffer(info.windowWidth, info.windowHeight);

		stbi_set_flip_vertically_on_load(true); // Do NOT convert this we need to use stb_image.h

		// 첫 번째 객체 정의 : OBJ 모델  --------------------------------------------------
		objModel->init();
		objModel->loadFBX("../assets/anim/re_RIG_RedHood_3.fbx");

		// 두 번째 객체 정의 : 피라미드 --------------------------------------------------
		// 피라미드 점들의 위치와 컬러, 텍스처 좌표를 정의한다.
		GLfloat pyramid_vertices[] = {
			1.0f, 0.0f, -1.0f,    // 우측 상단
			-1.0f, 0.0f, -1.0f,   // 좌측 상단
			-1.0f, 0.0f, 1.0f,    // 좌측 하단d
			1.0f, 0.0f, 1.0f,     // 우측 하단
			0.0f, 1.0f, 0.0f,      // 상단 꼭지점
			0.0f, -1.0f, 0.0f,      // 하단 꼭지점
		};

		// 삼각형으로 그릴 인덱스를 정의한다.
		GLuint pyramid_indices[] = {
			4, 0, 1,
			4, 1, 2,
			4, 2, 3,
			4, 3, 0,

			5, 1, 0,
			5, 2, 1,
			5, 3, 2,
			5, 0, 3,
		};

		pyramidModel->init();
		pyramidModel->setupMesh(6, pyramid_vertices);
		pyramidModel->setupIndices(24, pyramid_indices);

		GLfloat plate_verticies[] = {
			1.0f, 1.0f, 0.0f, 
			- 1.0f, 1.0f, 0.0f,
			1.0f, -1.0f, 0.0f,
			-1.0f, 1.0f, 0.0f,
			-1.0f, -1.0f, 0.0f,
			1.0f, -1.0f, 0.0f
		};

		GLfloat plate_texCoords[] = {
			20.0f, 20.0f,
			0.0f, 20.0f,
			20.0f, 0.0f,
			0.0f, 20.0f,
			 0.0f, 0.0f,
			20.0f, 0.0f
		};

		GLfloat plate_normal[] = {
			0.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f
		};

		plateModel->init();
		plateModel->setupMesh(6, plate_verticies, plate_texCoords, plate_normal); 
		// If we make new class, use pointer paramiter in place of vector container to reduce Error. For now, we need to input the size of the primitive pointer, and this is dangerous.
		plateModel->loadDiffuseMap("wall.jpg");

		glEnable(GL_MULTISAMPLE);

	}

	// 애플리케이션 끝날 때 호출된다.
	void shutdown()
	{
		glDeleteProgram(shader_program);
		glDeleteProgram(shader_light_program);
		glDeleteProgram(shader_postprocessing_program);
		delete objModel;
		delete pyramidModel;
		delete post; 
		delete plateModel;


		glfwDestroyWindow(window);
	}

	bool setupWindow(int width, int height, const char* windowName) {
		window = glfwCreateWindow(width, height, "Skeletal Animation", nullptr, nullptr);

		if (!window) return false;

		glfwMakeContextCurrent(window);
		info.windowWidth = static_cast<float>(width);
		info.windowHeight = static_cast<float>(height);

		glfwSetWindowUserPointer(window, this);
		glfwSetKeyCallback(window, keyCallback);
		glfwSetMouseButtonCallback(window, mouseButtonCallback);
		glfwSetCursorPosCallback(window, mousePositionCallback);
		glfwSetScrollCallback(window, mouseWheelCallback);
		glfwSetFramebufferSizeCallback(window, onResizeCallback);

		return true;
	}

	void deleteWindow() {
		glfwDestroyWindow(window);
	}

	int windowShouldClose() {
		return glfwWindowShouldClose(window);
	}

	// changed overrided render function
	void render(double currentTime)
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);

		// calculating camera matrices
		float distance = 5.f;
		//vmath::vec3 eye((float)cos(animationTime*0.3f)*distance, 1.0, (float)sin(animationTime*0.3f)*distance);
		vmath::vec3 eye(0.0f, 1.0f, distance);
		vmath::vec3 center(0.0, 0.0, 0.0);
		vmath::vec3 up(0.0, 1.0, 0.0);
		vmath::mat4 lookAt = vmath::lookat(eye, center, up);
		vmath::mat4 projM = vmath::perspective(fov, info.windowWidth / (float)info.windowHeight, 0.1f, 1000.0f);

		// setting lighting
		float r = 1.f;
		vmath::vec3 lightPos = vmath::vec3((float)sin(animationTime * 0.5f) * r, 0.f, (float)cos(animationTime * 0.5f) * r);
		vmath::vec3 viewPos = eye;

		float deltaTime = static_cast<float>(currentTime) - previousTime;
		if (deltaTime >= 0.1f) deltaTime = 0.1f;
		if (!pause)
			animationTime += deltaTime;
		previousTime = currentTime;

		vmath::vec3 lightAmbient(0.2f, 0.2f, 0.2f);
		vmath::vec3 lightDiffuse(0.5f, 0.5f, 0.5f);
		vmath::vec3 lightSpecular(1.0f, 1.0f, 1.0f);


		post->bindFrameBuffer();

		//모델 그리기
		glUseProgram(shader_program);

		glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"), 1, GL_FALSE, projM);
		glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"), 1, GL_FALSE, lookAt);

		glUniform3fv(glGetUniformLocation(shader_program, "viewPos"), 1, viewPos);

		glUniform3fv(glGetUniformLocation(shader_program, "light.position"), 1, lightPos);
		glUniform3fv(glGetUniformLocation(shader_program, "light.ambient"), 1, lightAmbient);
		glUniform3fv(glGetUniformLocation(shader_program, "light.diffuse"), 1, lightDiffuse);
		glUniform3fv(glGetUniformLocation(shader_program, "light.specular"), 1, lightSpecular);
		// TODO: [Performance / CPU Bottleneck] Severe CPU overhead caused by redundant string lookups (glGetUniformLocation) 
		// and multiple state changes (4x glUniform calls) per frame inside the render loop.
		// Refactoring plan: Pack light properties into a single struct and migrate to UBO (Uniform Buffer Object) 
		// to achieve a single batch upload, drastically reducing CPU-GPU communication costs.

		if (lineMode)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		//모델이 그려지는동안 위치 이동( pause상태가 아닐 때)
		if (drawModel) {
			float rotateSpeed = 0.0f;
			float forwardSpeed = 0.0f;
			if (!pause)
			{
				if (keyPressed_A)
				{
					rotateSpeed = deltaTime * 300.0f;
				}
				else if (keyPressed_D)
				{
					rotateSpeed = deltaTime  * (- 300.0f);
				}
				else
				{
					rotateSpeed = 0.0f;
				}

				if (keyPressed_W)
				{
					forwardSpeed = deltaTime * 3.0f;
				}
				else if (keyPressed_S)
				{
					forwardSpeed = deltaTime * (-3.0f);
				}
				else
				{
					forwardSpeed = 0.0f;
				}
			}
			

			objYangle += rotateSpeed;
			objPosition[0] += sin(vmath::radians(objYangle)) * forwardSpeed;
			objPosition[2] += cos(vmath::radians(objYangle)) * forwardSpeed;

			vmath::mat4 model = vmath::translate(objPosition) *
				vmath::rotate(objYangle, 0.0f, 1.0f, 0.0f) *
				vmath::scale(0.01f);

			glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, model);
			if (!pause)
			{
				if (keyPressed_W || keyPressed_S)
					objModel->draw(shader_program, currentTime);
				else
					objModel->draw(shader_program, 0.0f);
			}
			else
				objModel->draw(shader_program, 0.0f);
			//Optimize render loop branches by replacing redundant if-else blocks with a ternary operator.
		}

		glUseProgram(shader_light_program);
		// 피라미드 그리기 ---------------------------------------
		if (drawLight) {
			vmath::mat4 transform = vmath::translate(lightPos) * vmath::scale(0.05f);

			glUniformMatrix4fv(glGetUniformLocation(shader_light_program, "projection"), 1, GL_FALSE, projM);
			glUniformMatrix4fv(glGetUniformLocation(shader_light_program, "view"), 1, GL_FALSE, lookAt);
			glUniformMatrix4fv(glGetUniformLocation(shader_light_program, "model"), 1, GL_FALSE, transform);
			pyramidModel->draw(shader_light_program, currentTime);
		}

		mat4 plateTransform = translate(0.0f, -1.0f, 0.0f) *  rotate(-90.0f, vec3(1.0f, 0.0f, 0.0f))* scale(10.0f, 10.0f, 1.0f);
		glUniformMatrix4fv(glGetUniformLocation(shader_light_program, "model"), 1, GL_FALSE, plateTransform);
		glUniform3fv(glGetUniformLocation(shader_light_program, "viewPos"), 1, viewPos);

		glUniform3fv(glGetUniformLocation(shader_light_program, "light.position"), 1, lightPos);
		glUniform3fv(glGetUniformLocation(shader_light_program, "light.ambient"), 1, lightAmbient);
		glUniform3fv(glGetUniformLocation(shader_light_program, "light.diffuse"), 1, lightDiffuse);
		glUniform3fv(glGetUniformLocation(shader_light_program, "light.specular"), 1, lightSpecular);
		plateModel->draw(shader_light_program, currentTime);

		post->unbindFrameBuffer();
		post->drawScreen(shader_postprocessing_program, postConvolNum);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}


	static void onResizeCallback(GLFWwindow* window, int w, int h) {
		auto* app = static_cast<my_application*>(glfwGetWindowUserPointer(window));
		app->onResize(w, h);
	}
	void onResize(int w, int h)
	{
		if (w == 0 && h == 0) return;
		info.windowWidth = static_cast<float>(w);
		info.windowHeight = static_cast<float>(h);

		post->screenResize(w, h);

		glViewport(0, 0, info.windowWidth, info.windowHeight);
	}

	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
		auto* app = static_cast<my_application*>(glfwGetWindowUserPointer(window));
		app->onKey(key, action);
	}
	void onKey(int key, int action)
	{
		if (action == GLFW_PRESS) {
			switch (key) {
			case ' ': // 스페이스바
				pause = !pause;
				break;
			case '1':
				drawModel = !drawModel;
				break;
			case '2':
				drawLight = !drawLight;
				break;
			case 'M':
				lineMode = !lineMode;
				break;
			case 'R':
				initValues();
				break;
			case 'Z':
				postConvolNum = 0;
				break;
			case 'X':
				postConvolNum = 1;
				break;
			case 'C':
				postConvolNum = 2;
			default:
				break;
			};

			if (key == GLFW_KEY_A)
				keyPressed_A = true;

			if (key == GLFW_KEY_D)
				keyPressed_D = true;

			if (key == GLFW_KEY_W)
				keyPressed_W = true;

			if (key == GLFW_KEY_S)
				keyPressed_S = true;

		}
		else if (action == GLFW_RELEASE)
		{
			if (key == GLFW_KEY_A)
				keyPressed_A = false;
			if (key == GLFW_KEY_D)
				keyPressed_D = false;
			if (key == GLFW_KEY_W)
				keyPressed_W = false;
			if (key == GLFW_KEY_S)
				keyPressed_S = false;
		}
	}


	static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
		auto* app = static_cast<my_application*>(glfwGetWindowUserPointer(window));
		app->onMouseButton(button, action);
	}
	void onMouseButton(int button, int action)
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
		{
			mouseDown = true;

			double x, y;
			glfwGetCursorPos(window, &x, &y);
			mousePostion = vmath::vec2(float(x), float(y));
		}
		else
		{
			mouseDown = false;
		}
	}

	static void mousePositionCallback(GLFWwindow* window, double x, double y) {
		auto* app = static_cast<my_application*>(glfwGetWindowUserPointer(window));
		app->onMouseMove(static_cast<int>(x), static_cast<int>(y));
	}
	void onMouseMove(int x, int y)
	{
		if (mouseDown)
		{
			objYangle += float(x - mousePostion[0]) / 2.f;
			mousePostion = vmath::vec2(float(x), float(y));
		}
	}

#define MAX_FOV 120.f
#define MIN_FOV 10.f


	static void mouseWheelCallback(GLFWwindow* window, double xOffset, double yOffset) {
		auto* app = static_cast<my_application*>(glfwGetWindowUserPointer(window));
		app->onMouseWheel(yOffset);
	}
	void onMouseWheel(double ypos)
	{
		if (ypos > 0)
			fov = vmath::min(MAX_FOV, fov + 1.0f);
		else if(ypos < 0)
			fov = vmath::max(MIN_FOV, fov - 1.0f);
	}

	void initValues()
	{
		drawModel = true;
		drawLight = true;
		pause = false;
		animationTime = 0;
		previousTime = 0;
		lineMode = false;

		mouseDown = false;

		fov = 50.f;

		postConvolNum = 0;

		objPosition = vmath::vec3(0.0f, -1.0f, 0.0f);
		objYangle = 0.f;
	}


private:
	GLuint shader_program = 0;
	GLuint shader_light_program = 0;
	GLuint shader_postprocessing_program = 0;

	PostProcessing* post = nullptr;
	GLuint postConvolNum = 0;

	Model* objModel = nullptr;
	Model* pyramidModel = nullptr;
	Model* plateModel = nullptr;
	vmath::vec3 objPosition;
	float objYangle;

	bool drawModel, drawLight;
	bool lineMode;
	bool pause;
	double previousTime;
	double animationTime;


	vmath::vec2 mousePostion;
	bool mouseDown;

	float fov;

	Assimp::Importer importer;

	bool keyPressed_A = false;
	bool keyPressed_D = false;
	bool keyPressed_W = false;
	bool keyPressed_S = false;

	struct WindowInformation {
		float windowWidth;
		float windowHeight;
	} info;

	GLFWwindow* window = nullptr;
};

int main() {
	glfwSetErrorCallback(glfwErrorCallback);

	if (!glfwInit()) return EXIT_FAILURE;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
	glfwWindowHint(GLFW_SAMPLES, 16);
	auto app = make_unique<my_application>();
	if (!app->setupWindow(600, 400, "Skeletal Animation")) {
		glfwTerminate();
		return EXIT_FAILURE;
	}

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		cerr << "Failed to load OpenGL functions" << endl;
		app->deleteWindow();
		glfwTerminate();
		return EXIT_FAILURE;
	}
	cout << "OpenGL " << glGetString(GL_VERSION) << endl;

	app->startup();
	while (!app->windowShouldClose()) {
		app->render(glfwGetTime());
	}
	app->shutdown();

	glfwTerminate();
	return EXIT_SUCCESS;
}