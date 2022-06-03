#include <cstdio>
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>

#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "common.h"
#include "cellular_automaton.h"

static int WIN_WIDTH = 500;                       // �E�B���h�E�̕�
static int WIN_HEIGHT = 500;                       // �E�B���h�E�̍���
static const char *WIN_TITLE = "OpenGL Course";     // �E�B���h�E�̃^�C�g��

static const std::string TEX_FILE = std::string(DATA_DIRECTORY) + "hue.png";

static const std::string VERT_SHADER_FILE = std::string(SHADER_DIRECTORY) + "glsl.vert";
static const std::string FRAG_SHADER_FILE = std::string(SHADER_DIRECTORY) + "glsl.frag";

static const std::string VERT_SHADER_FILE2 = std::string(SHADER_DIRECTORY) + "render_history.vert";
static const std::string FRAG_SHADER_FILE2 = std::string(SHADER_DIRECTORY) + "render_history.frag";


// VAO�֘A�̕ϐ�
GLuint vaoId;
GLuint vboId;
GLuint iboId;

GLuint vaoId2;
GLuint vboId2;
GLuint indexBufferId2;

// ���_�I�u�W�F�N�g
struct Vertex {
	Vertex(const glm::vec3 &position_, const glm::vec3 &color_)
		: position(position_)
		, color(color_) {
	}

	glm::vec3 position;
	glm::vec3 color;
};

// �V�F�[�_���Q�Ƃ���ԍ�
GLuint programId;
GLuint programId2;


// �e�N�X�`��
GLuint textureId;

//
CellularAutomaton cellularAutomaton;
static const int xCells = 1024; // 100
static const int yCells = 512;
static const int historyNum = 1024;
static const double speed = 0.1; //0.1
static const double dx = 0.005; // 0.001
static const double dt = 0.05;

int rule = 110;
// ���_�̃f�[�^
std::vector<Vertex> vertices;
std::vector<Vertex> verticesHistory;

//
// arcball controll

// �����̂̉�]�p�x
static float theta = 0.0f;

// Arcball�R���g���[���̂��߂̕ϐ�
bool isDragging = false;

enum ArcballMode {
	ARCBALL_MODE_NONE = 0x00,
	ARCBALL_MODE_TRANSLATE = 0x01,
	ARCBALL_MODE_ROTATE = 0x02,
	ARCBALL_MODE_SCALE = 0x04
};

int arcballMode = ARCBALL_MODE_NONE;
glm::mat4 modelMat, viewMat, projMat;
glm::mat4 acRotMat, acTransMat, acScaleMat;
glm::vec3 gravity;

float acScale = 1.0f;
glm::ivec2 oldPos;
glm::ivec2 newPos;



GLuint compileShader(const std::string &filename, GLuint type) {
	// �V�F�[�_�̍쐬
	GLuint shaderId = glCreateShader(type);

	// �t�@�C���̓ǂݍ���
	std::ifstream reader;
	size_t codeSize;
	std::string code;

	// �t�@�C�����J��
	reader.open(filename.c_str(), std::ios::in);
	if (!reader.is_open()) {
		// �t�@�C�����J���Ȃ�������G���[���o���ďI��
		fprintf(stderr, "Failed to load a shader: %s\n", VERT_SHADER_FILE.c_str());
		exit(1);
	}

	// �t�@�C�������ׂēǂ�ŕϐ��Ɋi�[ (����)
	reader.seekg(0, std::ios::end);             // �t�@�C���ǂݎ��ʒu���I�[�Ɉړ�
	codeSize = reader.tellg();                  // ���݂̉ӏ�(=�I�[)�̈ʒu���t�@�C���T�C�Y
	code.resize(codeSize);                      // �R�[�h���i�[����ϐ��̑傫����ݒ�
	reader.seekg(0);                            // �t�@�C���̓ǂݎ��ʒu��擪�Ɉړ�
	reader.read(&code[0], codeSize);            // �擪����t�@�C���T�C�Y����ǂ�ŃR�[�h�̕ϐ��Ɋi�[

												// �t�@�C�������
	reader.close();

	// �R�[�h�̃R���p�C��
	const char *codeChars = code.c_str();
	glShaderSource(shaderId, 1, &codeChars, NULL);
	glCompileShader(shaderId);

	// �R���p�C���̐��ۂ𔻒肷��
	GLint compileStatus;
	glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus == GL_FALSE) {
		// �R���p�C�������s������G���[���b�Z�[�W�ƃ\�[�X�R�[�h��\�����ďI��
		fprintf(stderr, "Failed to compile a shader!\n");

		// �G���[���b�Z�[�W�̒������擾����
		GLint logLength;
		glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 0) {
			// �G���[���b�Z�[�W���擾����
			GLsizei length;
			std::string errMsg;
			errMsg.resize(logLength);
			glGetShaderInfoLog(shaderId, logLength, &length, &errMsg[0]);

			// �G���[���b�Z�[�W�ƃ\�[�X�R�[�h�̏o��
			fprintf(stderr, "[ ERROR ] %s\n", errMsg.c_str());
			fprintf(stderr, "%s\n", code.c_str());
		}
		while (1);
		exit(1);
	}

	return shaderId;
}

GLuint buildShaderProgram(const std::string &vShaderFile, const std::string &fShaderFile) {
	// �V�F�[�_�̍쐬
	GLuint vertShaderId = compileShader(vShaderFile, GL_VERTEX_SHADER);
	GLuint fragShaderId = compileShader(fShaderFile, GL_FRAGMENT_SHADER);

	// �V�F�[�_�v���O�����̃����N
	GLuint programId = glCreateProgram();
	glAttachShader(programId, vertShaderId);
	glAttachShader(programId, fragShaderId);
	glLinkProgram(programId);

	// �����N�̐��ۂ𔻒肷��
	GLint linkState;
	glGetProgramiv(programId, GL_LINK_STATUS, &linkState);
	if (linkState == GL_FALSE) {
		// �����N�Ɏ��s������G���[���b�Z�[�W��\�����ďI��
		fprintf(stderr, "Failed to link shaders!\n");

		// �G���[���b�Z�[�W�̒������擾����
		GLint logLength;
		glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 0) {
			// �G���[���b�Z�[�W���擾����
			GLsizei length;
			std::string errMsg;
			errMsg.resize(logLength);
			glGetProgramInfoLog(programId, logLength, &length, &errMsg[0]);

			// �G���[���b�Z�[�W���o�͂���
			fprintf(stderr, "[ ERROR ] %s\n", errMsg.c_str());
		}
		exit(1);
	}

	// �V�F�[�_�𖳌����������ID��Ԃ�
	glUseProgram(0);
	return programId;
}


// OpenGL�̏������֐�
void initializeGL() {
	// �w�i�F�̐ݒ� (��)
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	// �[�x�e�X�g�̗L����
	glEnable(GL_DEPTH_TEST);

	// initialize cellular automaton
	cellularAutomaton.setParams(xCells, yCells, historyNum, rule, dx, dt);
	cellularAutomaton.start();

	// ���_�f�[�^�̏�����
	for (int y = 0; y < yCells; y++) {
		for (int x = 0; x < xCells; x++) {
			double vx = (x - xCells / 2) * dx;
			double vy = (y - yCells / 2) * dx;
			Vertex v(glm::vec3(vx, vy, 0.0), glm::vec3(0.0, 1.0, 0.0));
			vertices.push_back(v);
		}
	}

	// VAO�̗p��
	glGenVertexArrays(1, &vaoId);
	glBindVertexArray(vaoId);

	glGenBuffers(1, &vboId);
	glBindBuffer(GL_ARRAY_BUFFER, vboId);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);

	//
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));


	std::vector<unsigned int> indices;
	for (int y = 0; y < yCells - 1; y++) {
		for (int x = 0; x < xCells - 1; x++) {
			const int i0 = y * xCells + x;
			const int i1 = y * xCells + (x + 1);
			const int i2 = (y + 1) * xCells + x;
			const int i3 = (y + 1) * xCells + (x + 1);

			indices.push_back(i0);
			indices.push_back(i1);
			indices.push_back(i3);
			indices.push_back(i0);
			indices.push_back(i3);
			indices.push_back(i2);
		}
	}

	glGenBuffers(1, &iboId);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboId);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), &indices[0], GL_DYNAMIC_DRAW);

	glBindVertexArray(0);

	//
	//
	// VAO�̍쐬2

	// Vertex�z��̍쐬
	std::vector<unsigned int> indices2;

	for (int z = 0; z < historyNum; z++) {
		for (int x = 0; x < xCells; x++) {
			double vx = (x - xCells / 2) * dx;
			double vz = (z)* dx;
			Vertex v(glm::vec3(vx, -yCells / 2 * dx, -vz), glm::vec3(0.5, 0.5, 0.5));
			verticesHistory.push_back(v);
		}
	}

	for (int z = 0; z < historyNum - 1; z++) {
		for (int x = 0; x < xCells - 1; x++) {
			const int i0 = z * xCells + x;
			const int i1 = z * xCells + (x + 1);
			const int i2 = (z + 1) * xCells + x;
			const int i3 = (z + 1) * xCells + (x + 1);

			indices2.push_back(i0);
			indices2.push_back(i1);
			indices2.push_back(i3);
			indices2.push_back(i0);
			indices2.push_back(i3);
			indices2.push_back(i2);
		}
	}
	//
	glGenVertexArrays(1, &vaoId2);
	glBindVertexArray(vaoId2);

	// ���_�o�b�t�@�̍쐬
	glGenBuffers(1, &vboId2);
	glBindBuffer(GL_ARRAY_BUFFER, vboId2);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * verticesHistory.size(), verticesHistory.data(), GL_DYNAMIC_DRAW);

	// ���_�o�b�t�@�̗L����
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

	// ���_�ԍ��o�b�t�@�̍쐬
	glGenBuffers(1, &indexBufferId2);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferId2);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices2.size(),
		indices2.data(), GL_DYNAMIC_DRAW);

	// VAO��OFF�ɂ��Ă���
	glBindVertexArray(0);

	//
	//
	// �e�N�X�`���̗p��
	int texWidth, texHeight, channels;
	unsigned char *bytes = stbi_load(TEX_FILE.c_str(), &texWidth, &texHeight, &channels, STBI_rgb_alpha);
	if (!bytes) {
		fprintf(stderr, "Failed to load image file: %s\n", TEX_FILE.c_str());
		exit(1);
	}

	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_1D, textureId);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, texWidth,
		0, GL_RGBA, GL_UNSIGNED_BYTE, bytes);

	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_1D, 0);

	stbi_image_free(bytes);

	// shaders
	programId = buildShaderProgram(VERT_SHADER_FILE, FRAG_SHADER_FILE);
	programId2 = buildShaderProgram(VERT_SHADER_FILE2, FRAG_SHADER_FILE2);

	// matrices

	// �J�����̏�����
	projMat = glm::perspective(45.0f, (float)WIN_WIDTH / (float)WIN_HEIGHT, 0.1f, 1000.0f);

	viewMat = glm::lookAt(glm::vec3(xCells * dx + 10.0f, 0.0f, 0.0f),   // ���_�̈ʒu
		glm::vec3(0.0f, 0.0f, 0.0f),   // ���Ă����
		glm::vec3(0.0f, 0.0f, 1.0f));  // ���E�̏����

									   // ���̑��̍s��̏�����
	modelMat = glm::mat4(1.0);
	acRotMat = glm::mat4(1.0);
	acTransMat = glm::mat4(1.0);
	acScaleMat = glm::mat4(1.0);

}

void drawAxis() {
	static float axis_pos[] = { 0.0f, 0.0f, 0.0f };
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(axis_pos[0], axis_pos[1], axis_pos[2]);

	glBegin(GL_LINES);

	glColor3f(1.0f, 0.0f, 0.0f);
	glVertex3f(1.0f, 0.0f, 0.0f);
	glVertex3f(0.0f, 0.0f, 0.0f);

	glColor3f(0.0f, 1.0f, 0.0f);
	glVertex3f(0.0f, 1.0f, 0.0f);
	glVertex3f(0.0f, 0.0f, 0.0f);

	glColor3f(0.0f, 0.0f, 1.0f);
	glVertex3f(0.0f, 0.0f, 1.0f);
	glVertex3f(0.0f, 0.0f, 0.0f);

	glEnd();
	glPopMatrix();
}

// OpenGL�̕`��֐�
void paintGL() {
	// �w�i�F�Ɛ[�x�l�̃N���A
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// ���W�̕ϊ�
	glm::mat4 projMat = glm::perspective(45.0f,
		(float)WIN_WIDTH / (float)WIN_HEIGHT, 0.1f, 1000.0f);


	glm::mat4 mvpMat = projMat * viewMat * modelMat * acTransMat * acRotMat * acScaleMat;

	//
	//
	// Spectrum

	// VAO�̗L����
	glBindVertexArray(vaoId);

	// �V�F�[�_�̗L����
	glUseProgram(programId);

	// �e�N�X�`���̗L����
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_1D, textureId);

	// Uniform�ϐ��̓]��
	GLuint mvpMatLocId = glGetUniformLocation(programId, "u_mvpMat");
	glUniformMatrix4fv(mvpMatLocId, 1, GL_FALSE, glm::value_ptr(mvpMat));

	GLuint texLocId = glGetUniformLocation(programId, "u_texture");
	glUniform1i(texLocId, 0);

	// �O�p�`�̕`��
	glDrawElements(GL_TRIANGLES, 3 * (yCells - 1) * (xCells - 1) * 2, GL_UNSIGNED_INT, 0);

	// VAO�̖�����
	glBindVertexArray(0);

	// �V�F�[�_�̖�����
	glUseProgram(0);

	// �e�N�X�`���̖�����
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_1D, 0);

	//
	//
	// History
	glBindVertexArray(vaoId2);

	// �V�F�[�_�̗L����
	glUseProgram(programId2);

	// Uniform�ϐ��̓]��
	mvpMatLocId = glGetUniformLocation(programId2, "u_mvpMat");
	glUniformMatrix4fv(mvpMatLocId, 1, GL_FALSE, glm::value_ptr(mvpMat));


	// �O�p�`�̕`��
	glDrawElements(GL_TRIANGLES, 3 * (historyNum - 1) * (xCells - 1) * 2, GL_UNSIGNED_INT, 0);

	// VAO�̖�����
	glBindVertexArray(0);

	// �V�F�[�_�̖�����
	glUseProgram(0);
}

void resizeGL(GLFWwindow *window, int width, int height) {
	// ���[�U�Ǘ��̃E�B���h�E�T�C�Y��ύX
	WIN_WIDTH = width;
	WIN_HEIGHT = height;

	// GLFW�Ǘ��̃E�B���h�E�T�C�Y��ύX
	glfwSetWindowSize(window, WIN_WIDTH, WIN_HEIGHT);

	// ���ۂ̃E�B���h�E�T�C�Y (�s�N�Z����) ���擾
	int renderBufferWidth, renderBufferHeight;
	glfwGetFramebufferSize(window, &renderBufferWidth, &renderBufferHeight);

	// �r���[�|�[�g�ϊ��̍X�V
	glViewport(0, 0, renderBufferWidth, renderBufferHeight);
}

// �A�j���[�V�����̂��߂̃A�b�v�f�[�g
void update() {

	// �f�[�^�̍X�V
	cellularAutomaton.step();

	for (int y = 0; y < yCells; y++) {
		for (int x = 0; x < xCells; x++) {
			vertices[y * xCells + x].position.z = cellularAutomaton.get(x, y);
			if (cellularAutomaton.get(x, y) < 0.0) vertices[y * xCells + x].position.z = 0.0;
		}
	}
	glBindVertexArray(vaoId);
	glBindBuffer(GL_ARRAY_BUFFER, vboId);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
	glBindVertexArray(0);

	//
	for (int z = 0; z < historyNum; z++) {
		for (int x = 0; x < xCells; x++) {
			auto state = cellularAutomaton.historyGet(x, z);
			if (state == 0) {
				verticesHistory[z * xCells + x].color = glm::vec3(0.5, 0.5, 0.5);
			}
			else {
				verticesHistory[z * xCells + x].color = glm::vec3(1.0, 0.0, 0.0);
			}
		}
	}
	glBindVertexArray(vaoId2);
	glBindBuffer(GL_ARRAY_BUFFER, vboId2);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * verticesHistory.size(), verticesHistory.data(), GL_DYNAMIC_DRAW);
	glBindVertexArray(0);
}

//
//
// Callbacks
void keyboardEvent(GLFWwindow *window, int key, int scancode, int action, int mods) {
	// �L�[�{�[�h�̏�ԂƉ����ꂽ�L�[��\������
	printf("Keyboard: %s\n", action == GLFW_PRESS ? "Press" : "Release");
	printf("Key: %c\n", (char)key);

	// ����L�[��������Ă��邩�̔���
	int specialKeys[] = { GLFW_MOD_SHIFT, GLFW_MOD_CONTROL, GLFW_MOD_ALT, GLFW_MOD_SUPER };
	const char *specialKeyNames[] = { "Shift", "Ctrl", "Alt", "Super" };

	printf("Special Keys: ");
	for (int i = 0; i < 4; i++) {
		if ((mods & specialKeys[i]) != 0) {
			printf("%s ", specialKeyNames[i]);
		}
	}
	printf("\n");

	// ���[���̕ύX
	if (action == GLFW_PRESS && key == GLFW_KEY_SPACE) {
		printf("Input rule number in 0-255\n");
		std::cin >> rule;
		std::cin.clear();
		std::cin.ignore(10000, '\n');
		printf("rule %d set!\n", rule);
		cellularAutomaton.setParams(xCells, yCells, historyNum, rule, dx, dt);
		cellularAutomaton.start();
	}
}

void mouseEvent(GLFWwindow *window, int button, int action, int mods) {
	// �N���b�N�����{�^���ŏ�����؂�ւ���
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		arcballMode = ARCBALL_MODE_ROTATE;
	}
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
		arcballMode = ARCBALL_MODE_SCALE;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		arcballMode = ARCBALL_MODE_TRANSLATE;
	}

	// �N���b�N���ꂽ�ʒu���擾
	double px, py;
	glfwGetCursorPos(window, &px, &py);

	if (action == GLFW_PRESS) {
		if (!isDragging) {
			isDragging = true;
			oldPos = glm::ivec2(px, py);
			newPos = glm::ivec2(px, py);
		}
	}
	else {
		isDragging = false;
		oldPos = glm::ivec2(0, 0);
		newPos = glm::ivec2(0, 0);
		arcballMode = ARCBALL_MODE_NONE;
	}
}

// �X�N���[����̈ʒu���A�[�N�{�[������̈ʒu�ɕϊ�����֐�
glm::vec3 getVector(double x, double y) {
	glm::vec3 pt(2.0 * x / WIN_WIDTH - 1.0,
		-2.0 * y / WIN_HEIGHT + 1.0, 0.0);

	const double xySquared = pt.x * pt.x + pt.y * pt.y;
	if (xySquared <= 1.0) {
		// �P�ʉ~�̓����Ȃ�z���W���v�Z
		pt.z = std::sqrt(1.0 - xySquared);
	}
	else {
		// �O���Ȃ狅�̊O�g��ɂ���ƍl����
		pt = glm::normalize(pt);
	}

	return pt;
}

void updateRotate() {
	static const double Pi = 4.0 * std::atan(1.0);

	// �X�N���[�����W���A�[�N�{�[������̍��W�ɕϊ�
	const glm::vec3 u = glm::normalize(getVector(newPos.x, newPos.y));
	const glm::vec3 v = glm::normalize(getVector(oldPos.x, oldPos.y));

	// �J�������W�ɂ������]�� (=�I�u�W�F�N�g���W�ɂ������]��)
	const double angle = std::acos(std::max(-1.0f, std::min(glm::dot(u, v), 1.0f)));

	// �J������Ԃɂ������]��
	const glm::vec3 rotAxis = glm::cross(v, u);

	// �J�������W�̏������[���h���W�ɕϊ�����s��
	const glm::mat4 c2oMat = glm::inverse(viewMat * modelMat);

	// �I�u�W�F�N�g���W�ɂ������]��
	const glm::vec3 rotAxisObjSpace = glm::vec3(c2oMat * glm::vec4(rotAxis, 0.0f));

	// ��]�s��̍X�V
	acRotMat = glm::rotate((float)(4.0f * angle), rotAxisObjSpace) * acRotMat;
}

void updateTranslate() {
	// �I�u�W�F�N�g�d�S�̃X�N���[�����W�����߂�
	glm::vec4 gravityScreenSpace = (projMat * viewMat * modelMat) * glm::vec4(gravity.x, gravity.y, gravity.z, 1.0f);
	gravityScreenSpace /= gravityScreenSpace.w;

	// �X�N���[�����W�n�ɂ�����ړ���
	glm::vec4 newPosScreenSpace(2.0 * newPos.x / WIN_WIDTH - 1.0, -2.0 * newPos.y / WIN_HEIGHT + 1.0, gravityScreenSpace.z, 1.0f);
	glm::vec4 oldPosScreenSpace(2.0 * oldPos.x / WIN_WIDTH - 1.0, -2.0 * oldPos.y / WIN_HEIGHT + 1.0, gravityScreenSpace.z, 1.0f);

	// �X�N���[�����W�̏����I�u�W�F�N�g���W�ɕϊ�����s��
	const glm::mat4 s2oMat = glm::inverse(projMat * viewMat * modelMat);

	// �X�N���[����Ԃ̍��W���I�u�W�F�N�g��Ԃɕϊ�
	glm::vec4 newPosObjSpace = s2oMat * newPosScreenSpace;
	glm::vec4 oldPosObjSpace = s2oMat * oldPosScreenSpace;
	newPosObjSpace /= newPosObjSpace.w;
	oldPosObjSpace /= oldPosObjSpace.w;

	// �I�u�W�F�N�g���W�n�ł̈ړ���
	const glm::vec3 transObjSpace = glm::vec3(newPosObjSpace - oldPosObjSpace);

	// �I�u�W�F�N�g��Ԃł̕��s�ړ�
	acTransMat = glm::translate(acTransMat, transObjSpace);
}

void updateScale() {
	acScaleMat = glm::scale(glm::vec3(acScale, acScale, acScale));
}

void updateMouse() {
	switch (arcballMode) {
	case ARCBALL_MODE_ROTATE:
		updateRotate();
		break;

	case ARCBALL_MODE_TRANSLATE:
		updateTranslate();
		break;

	case ARCBALL_MODE_SCALE:
		acScale += (float)(oldPos.y - newPos.y) / WIN_HEIGHT;
		updateScale();
		break;
	}
}

void mouseMoveEvent(GLFWwindow *window, double xpos, double ypos) {
	if (isDragging) {
		// �}�E�X�̌��݈ʒu���X�V
		newPos = glm::ivec2(xpos, ypos);

		// �}�E�X�����܂蓮���Ă��Ȃ����͏��������Ȃ�
		const double dx = newPos.x - oldPos.x;
		const double dy = newPos.y - oldPos.y;
		const double length = dx * dx + dy * dy;
		if (length < 2.0f * 2.0f) {
			return;
		}
		else {
			updateMouse();
			oldPos = glm::ivec2(xpos, ypos);
		}
	}
}

void wheelEvent(GLFWwindow *window, double xpos, double ypos) {
	acScale += ypos / 10.0;
	updateScale();
}




int main(int argc, char **argv) {
	// OpenGL������������
	if (glfwInit() == GL_FALSE) {
		fprintf(stderr, "Initialization failed!\n");
		return 1;
	}

	// OpenGL�̃o�[�W�����w��
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Window�̍쐬
	GLFWwindow *window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, WIN_TITLE,
		NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "Window creation failed!");
		glfwTerminate();
		return 1;
	}

	// OpenGL�̕`��Ώۂ�Window��ǉ�
	glfwMakeContextCurrent(window);

	// �L�[�{�[�h�̃C�x���g����������֐���o�^
	glfwSetKeyCallback(window, keyboardEvent);

	// �}�E�X�̃C�x���g����������֐���o�^
	glfwSetMouseButtonCallback(window, mouseEvent);

	// �}�E�X�̓�������������֐���o�^
	glfwSetCursorPosCallback(window, mouseMoveEvent);

	glfwSetScrollCallback(window, wheelEvent);


	// OpenGL 3.x/4.x�̊֐������[�h���� (glfwMakeContextCurrent�̌�łȂ��Ƃ����Ȃ�)
	const int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0) {
		fprintf(stderr, "Failed to load OpenGL 3.x/4.x libraries!\n");
		return 1;
	}

	// �o�[�W�������o�͂���
	printf("Load OpenGL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

	// �E�B���h�E�̃��T�C�Y�������֐��̓o�^
	glfwSetWindowSizeCallback(window, resizeGL);

	// OpenGL��������
	initializeGL();

	// ���C�����[�v
	while (glfwWindowShouldClose(window) == GL_FALSE) {
		// �`��
		paintGL();

		// �A�j���[�V����
		update();


		// �`��p�o�b�t�@�̐؂�ւ�
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
}
