#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console") 
// sb7.h 헤더 파일을 포함시킨다
#include <sb7.h>
#include <vmath.h>
#include <shader.h>

#include <iostream>

#include "FBXModel.h"
#include "PostProcessing.h"

using namespace std;
// sb6::application을 상속받는다.
class my_application : public sb7::application
{
public:

	// 쉐이더 프로그램 컴파일한다.
	GLuint compile_shader(const char* vs_file, const char* fs_file)
	{
		// 버텍스 쉐이더를 생성하고 컴파일한다.
		GLuint vertex_shader = sb7::shader::load(vs_file, GL_VERTEX_SHADER);

		// 프래그먼트 쉐이더를 생성하고 컴파일한다.
		GLuint fragment_shader = sb7::shader::load(fs_file, GL_FRAGMENT_SHADER);

		// 프로그램을 생성하고 쉐이더를 Attach시키고 링크한다.
		GLuint program = glCreateProgram();
		glAttachShader(program, vertex_shader);
		glAttachShader(program, fragment_shader);
		glLinkProgram(program);

		// 이제 프로그램이 쉐이더를 소유하므로 쉐이더를 삭제한다.
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);

		return program;
	}

	// 애플리케이션 초기화 수행한다.
	virtual void startup()
	{
		// 클래스 내부 변수 초기화
		initValues();

		objModel = new FBXModel();
		pyramidModel = new Model();
		plateModel = new Model();
		post = new PostProcessing();

		// 쉐이더 프로그램 컴파일 및 연결
		shader_program = compile_shader("Model_vs.glsl", "Model_fs.glsl");
		shader_light_program = compile_shader("Light_vs.glsl", "Light_fs.glsl");
		shader_postprocessing_program = compile_shader("screen_filtering_vs.glsl", "screen_filtering_fs.glsl");
		// startup() 내부에서 셰이더 컴파일 직후
		std::cout << "Shader Program ID: " << shader_program << std::endl;
		std::cout << "Shader Program ID: " << shader_light_program << std::endl;
		std::cout << "Shader Program ID: " << shader_postprocessing_program << std::endl;
		post->initScreen();
		post->initFrameBuffer(info.windowWidth, info.windowHeight);

		stbi_set_flip_vertically_on_load(true);

		// 첫 번째 객체 정의 : OBJ 모델  --------------------------------------------------
		objModel->init();
		objModel->loadFBX("re_RIG_RedHood_3.fbx");

		// 두 번째 객체 정의 : 피라미드 --------------------------------------------------
		// 피라미드 점들의 위치와 컬러, 텍스처 좌표를 정의한다.
		GLfloat pyramid_vertices[] = {
			1.0f, 0.0f, -1.0f,    // 우측 상단
			-1.0f, 0.0f, -1.0f,   // 좌측 상단
			-1.0f, 0.0f, 1.0f,    // 좌측 하단
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
		plateModel->loadDiffuseMap("wall.jpg");

		glEnable(GL_MULTISAMPLE);

	}

	// 애플리케이션 끝날 때 호출된다.
	virtual void shutdown()
	{
		glDeleteProgram(shader_program);
		glDeleteProgram(shader_light_program);
		glDeleteProgram(shader_postprocessing_program);

		delete objModel;
		delete pyramidModel;
		delete post;

	}

	// 렌더링 virtual 함수를 작성해서 오버라이딩한다.
	virtual void render(double currentTime)
	{

		// 카메라 매트릭스 계산
		float distance = 5.f;
		//vmath::vec3 eye((float)cos(animationTime*0.3f)*distance, 1.0, (float)sin(animationTime*0.3f)*distance);
		vmath::vec3 eye(0.0f, 1.0f, distance);
		vmath::vec3 center(0.0, 0.0, 0.0);
		vmath::vec3 up(0.0, 1.0, 0.0);
		vmath::mat4 lookAt = vmath::lookat(eye, center, up);
		vmath::mat4 projM = vmath::perspective(fov, (float)info.windowWidth / (float)info.windowHeight, 0.1f, 1000.0f);

		// 라이팅 설정
		float r = 1.f;
		vmath::vec3 lightPos = vmath::vec3((float)sin(animationTime * 0.5f) * r, 0.f, (float)cos(animationTime * 0.5f) * r);
		vmath::vec3 viewPos = eye;


		if (!pause)
			animationTime += currentTime - previousTime;
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
					rotateSpeed = 1.0f;
				}
				else if (keyPressed_D)
				{
					rotateSpeed = -1.0f;
				}
				else
				{
					rotateSpeed = 0.0f;
				}

				if (keyPressed_W)
				{
					forwardSpeed += 0.001f;
				}
				else if (keyPressed_S)
				{
					forwardSpeed -= 0.001f;
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

	}

	virtual void onResize(int w, int h)
	{
		sb7::application::onResize(w, h);

		post->screenResize(w, h);

		if (glViewport != NULL)
			glViewport(0, 0, info.windowWidth, info.windowHeight);
	}

	virtual void init()
	{
		sb7::application::init();

		info.samples = 16;
		info.flags.debug = 1;
	}

	virtual void onKey(int key, int action)
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

	virtual void onMouseButton(int button, int action)
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
		{
			mouseDown = true;

			int x, y;
			getMousePosition(x, y);
			mousePostion = vmath::vec2(float(x), float(y));
		}
		else
		{
			mouseDown = false;
		}
	}

	virtual void onMouseMove(int x, int y)
	{
		if (mouseDown)
		{
			objYangle += float(x - mousePostion[0]) / 2.f;
			mousePostion = vmath::vec2(float(x), float(y));
		}
	}

#define MAX_FOV 120.f
#define MIN_FOV 10.f
	virtual void onMouseWheel(int pos)
	{
		if (pos > 0)
			fov = vmath::min(MAX_FOV, fov + 1.0f);
		else
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
	GLuint shader_program;
	GLuint shader_light_program;
	GLuint shader_postprocessing_program;

	PostProcessing* post;
	GLuint postConvolNum = 0;

	Model* objModel;
	Model* pyramidModel;
	Model* plateModel;
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

};

// DECLARE_MAIN의 하나뿐인 인스턴스
DECLARE_MAIN(my_application)