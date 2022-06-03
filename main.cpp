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

static int WIN_WIDTH = 500;                       // ウィンドウの幅
static int WIN_HEIGHT = 500;                       // ウィンドウの高さ
static const char *WIN_TITLE = "OpenGL Course";     // ウィンドウのタイトル

static const std::string TEX_FILE = std::string(DATA_DIRECTORY) + "hue.png";

static const std::string VERT_SHADER_FILE = std::string(SHADER_DIRECTORY) + "glsl.vert";
static const std::string FRAG_SHADER_FILE = std::string(SHADER_DIRECTORY) + "glsl.frag";

static const std::string VERT_SHADER_FILE2 = std::string(SHADER_DIRECTORY) + "render_history.vert";
static const std::string FRAG_SHADER_FILE2 = std::string(SHADER_DIRECTORY) + "render_history.frag";


// VAO関連の変数
GLuint vaoId;
GLuint vboId;
GLuint iboId;

GLuint vaoId2;
GLuint vboId2;
GLuint indexBufferId2;

// 頂点オブジェクト
struct Vertex {
	Vertex(const glm::vec3 &position_, const glm::vec3 &color_)
		: position(position_)
		, color(color_) {
	}

	glm::vec3 position;
	glm::vec3 color;
};

// シェーダを参照する番号
GLuint programId;
GLuint programId2;


// テクスチャ
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
// 頂点のデータ
std::vector<Vertex> vertices;
std::vector<Vertex> verticesHistory;

//
// arcball controll

// 立方体の回転角度
static float theta = 0.0f;

// Arcballコントロールのための変数
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
	// シェーダの作成
	GLuint shaderId = glCreateShader(type);

	// ファイルの読み込み
	std::ifstream reader;
	size_t codeSize;
	std::string code;

	// ファイルを開く
	reader.open(filename.c_str(), std::ios::in);
	if (!reader.is_open()) {
		// ファイルを開けなかったらエラーを出して終了
		fprintf(stderr, "Failed to load a shader: %s\n", VERT_SHADER_FILE.c_str());
		exit(1);
	}

	// ファイルをすべて読んで変数に格納 (やや難)
	reader.seekg(0, std::ios::end);             // ファイル読み取り位置を終端に移動
	codeSize = reader.tellg();                  // 現在の箇所(=終端)の位置がファイルサイズ
	code.resize(codeSize);                      // コードを格納する変数の大きさを設定
	reader.seekg(0);                            // ファイルの読み取り位置を先頭に移動
	reader.read(&code[0], codeSize);            // 先頭からファイルサイズ分を読んでコードの変数に格納

												// ファイルを閉じる
	reader.close();

	// コードのコンパイル
	const char *codeChars = code.c_str();
	glShaderSource(shaderId, 1, &codeChars, NULL);
	glCompileShader(shaderId);

	// コンパイルの成否を判定する
	GLint compileStatus;
	glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus == GL_FALSE) {
		// コンパイルが失敗したらエラーメッセージとソースコードを表示して終了
		fprintf(stderr, "Failed to compile a shader!\n");

		// エラーメッセージの長さを取得する
		GLint logLength;
		glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 0) {
			// エラーメッセージを取得する
			GLsizei length;
			std::string errMsg;
			errMsg.resize(logLength);
			glGetShaderInfoLog(shaderId, logLength, &length, &errMsg[0]);

			// エラーメッセージとソースコードの出力
			fprintf(stderr, "[ ERROR ] %s\n", errMsg.c_str());
			fprintf(stderr, "%s\n", code.c_str());
		}
		while (1);
		exit(1);
	}

	return shaderId;
}

GLuint buildShaderProgram(const std::string &vShaderFile, const std::string &fShaderFile) {
	// シェーダの作成
	GLuint vertShaderId = compileShader(vShaderFile, GL_VERTEX_SHADER);
	GLuint fragShaderId = compileShader(fShaderFile, GL_FRAGMENT_SHADER);

	// シェーダプログラムのリンク
	GLuint programId = glCreateProgram();
	glAttachShader(programId, vertShaderId);
	glAttachShader(programId, fragShaderId);
	glLinkProgram(programId);

	// リンクの成否を判定する
	GLint linkState;
	glGetProgramiv(programId, GL_LINK_STATUS, &linkState);
	if (linkState == GL_FALSE) {
		// リンクに失敗したらエラーメッセージを表示して終了
		fprintf(stderr, "Failed to link shaders!\n");

		// エラーメッセージの長さを取得する
		GLint logLength;
		glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 0) {
			// エラーメッセージを取得する
			GLsizei length;
			std::string errMsg;
			errMsg.resize(logLength);
			glGetProgramInfoLog(programId, logLength, &length, &errMsg[0]);

			// エラーメッセージを出力する
			fprintf(stderr, "[ ERROR ] %s\n", errMsg.c_str());
		}
		exit(1);
	}

	// シェーダを無効化した後にIDを返す
	glUseProgram(0);
	return programId;
}


// OpenGLの初期化関数
void initializeGL() {
	// 背景色の設定 (黒)
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	// 深度テストの有効化
	glEnable(GL_DEPTH_TEST);

	// initialize cellular automaton
	cellularAutomaton.setParams(xCells, yCells, historyNum, rule, dx, dt);
	cellularAutomaton.start();

	// 頂点データの初期化
	for (int y = 0; y < yCells; y++) {
		for (int x = 0; x < xCells; x++) {
			double vx = (x - xCells / 2) * dx;
			double vy = (y - yCells / 2) * dx;
			Vertex v(glm::vec3(vx, vy, 0.0), glm::vec3(0.0, 1.0, 0.0));
			vertices.push_back(v);
		}
	}

	// VAOの用意
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
	// VAOの作成2

	// Vertex配列の作成
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

	// 頂点バッファの作成
	glGenBuffers(1, &vboId2);
	glBindBuffer(GL_ARRAY_BUFFER, vboId2);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * verticesHistory.size(), verticesHistory.data(), GL_DYNAMIC_DRAW);

	// 頂点バッファの有効化
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

	// 頂点番号バッファの作成
	glGenBuffers(1, &indexBufferId2);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferId2);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices2.size(),
		indices2.data(), GL_DYNAMIC_DRAW);

	// VAOをOFFにしておく
	glBindVertexArray(0);

	//
	//
	// テクスチャの用意
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

	// カメラの初期化
	projMat = glm::perspective(45.0f, (float)WIN_WIDTH / (float)WIN_HEIGHT, 0.1f, 1000.0f);

	viewMat = glm::lookAt(glm::vec3(xCells * dx + 10.0f, 0.0f, 0.0f),   // 視点の位置
		glm::vec3(0.0f, 0.0f, 0.0f),   // 見ている先
		glm::vec3(0.0f, 0.0f, 1.0f));  // 視界の上方向

									   // その他の行列の初期化
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

// OpenGLの描画関数
void paintGL() {
	// 背景色と深度値のクリア
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// 座標の変換
	glm::mat4 projMat = glm::perspective(45.0f,
		(float)WIN_WIDTH / (float)WIN_HEIGHT, 0.1f, 1000.0f);


	glm::mat4 mvpMat = projMat * viewMat * modelMat * acTransMat * acRotMat * acScaleMat;

	//
	//
	// Spectrum

	// VAOの有効化
	glBindVertexArray(vaoId);

	// シェーダの有効化
	glUseProgram(programId);

	// テクスチャの有効化
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_1D, textureId);

	// Uniform変数の転送
	GLuint mvpMatLocId = glGetUniformLocation(programId, "u_mvpMat");
	glUniformMatrix4fv(mvpMatLocId, 1, GL_FALSE, glm::value_ptr(mvpMat));

	GLuint texLocId = glGetUniformLocation(programId, "u_texture");
	glUniform1i(texLocId, 0);

	// 三角形の描画
	glDrawElements(GL_TRIANGLES, 3 * (yCells - 1) * (xCells - 1) * 2, GL_UNSIGNED_INT, 0);

	// VAOの無効化
	glBindVertexArray(0);

	// シェーダの無効化
	glUseProgram(0);

	// テクスチャの無効化
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_1D, 0);

	//
	//
	// History
	glBindVertexArray(vaoId2);

	// シェーダの有効化
	glUseProgram(programId2);

	// Uniform変数の転送
	mvpMatLocId = glGetUniformLocation(programId2, "u_mvpMat");
	glUniformMatrix4fv(mvpMatLocId, 1, GL_FALSE, glm::value_ptr(mvpMat));


	// 三角形の描画
	glDrawElements(GL_TRIANGLES, 3 * (historyNum - 1) * (xCells - 1) * 2, GL_UNSIGNED_INT, 0);

	// VAOの無効化
	glBindVertexArray(0);

	// シェーダの無効化
	glUseProgram(0);
}

void resizeGL(GLFWwindow *window, int width, int height) {
	// ユーザ管理のウィンドウサイズを変更
	WIN_WIDTH = width;
	WIN_HEIGHT = height;

	// GLFW管理のウィンドウサイズを変更
	glfwSetWindowSize(window, WIN_WIDTH, WIN_HEIGHT);

	// 実際のウィンドウサイズ (ピクセル数) を取得
	int renderBufferWidth, renderBufferHeight;
	glfwGetFramebufferSize(window, &renderBufferWidth, &renderBufferHeight);

	// ビューポート変換の更新
	glViewport(0, 0, renderBufferWidth, renderBufferHeight);
}

// アニメーションのためのアップデート
void update() {

	// データの更新
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
	// キーボードの状態と押されたキーを表示する
	printf("Keyboard: %s\n", action == GLFW_PRESS ? "Press" : "Release");
	printf("Key: %c\n", (char)key);

	// 特殊キーが押されているかの判定
	int specialKeys[] = { GLFW_MOD_SHIFT, GLFW_MOD_CONTROL, GLFW_MOD_ALT, GLFW_MOD_SUPER };
	const char *specialKeyNames[] = { "Shift", "Ctrl", "Alt", "Super" };

	printf("Special Keys: ");
	for (int i = 0; i < 4; i++) {
		if ((mods & specialKeys[i]) != 0) {
			printf("%s ", specialKeyNames[i]);
		}
	}
	printf("\n");

	// ルールの変更
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
	// クリックしたボタンで処理を切り替える
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		arcballMode = ARCBALL_MODE_ROTATE;
	}
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
		arcballMode = ARCBALL_MODE_SCALE;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		arcballMode = ARCBALL_MODE_TRANSLATE;
	}

	// クリックされた位置を取得
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

// スクリーン上の位置をアークボール球上の位置に変換する関数
glm::vec3 getVector(double x, double y) {
	glm::vec3 pt(2.0 * x / WIN_WIDTH - 1.0,
		-2.0 * y / WIN_HEIGHT + 1.0, 0.0);

	const double xySquared = pt.x * pt.x + pt.y * pt.y;
	if (xySquared <= 1.0) {
		// 単位円の内側ならz座標を計算
		pt.z = std::sqrt(1.0 - xySquared);
	}
	else {
		// 外側なら球の外枠上にあると考える
		pt = glm::normalize(pt);
	}

	return pt;
}

void updateRotate() {
	static const double Pi = 4.0 * std::atan(1.0);

	// スクリーン座標をアークボール球上の座標に変換
	const glm::vec3 u = glm::normalize(getVector(newPos.x, newPos.y));
	const glm::vec3 v = glm::normalize(getVector(oldPos.x, oldPos.y));

	// カメラ座標における回転量 (=オブジェクト座標における回転量)
	const double angle = std::acos(std::max(-1.0f, std::min(glm::dot(u, v), 1.0f)));

	// カメラ空間における回転軸
	const glm::vec3 rotAxis = glm::cross(v, u);

	// カメラ座標の情報をワールド座標に変換する行列
	const glm::mat4 c2oMat = glm::inverse(viewMat * modelMat);

	// オブジェクト座標における回転軸
	const glm::vec3 rotAxisObjSpace = glm::vec3(c2oMat * glm::vec4(rotAxis, 0.0f));

	// 回転行列の更新
	acRotMat = glm::rotate((float)(4.0f * angle), rotAxisObjSpace) * acRotMat;
}

void updateTranslate() {
	// オブジェクト重心のスクリーン座標を求める
	glm::vec4 gravityScreenSpace = (projMat * viewMat * modelMat) * glm::vec4(gravity.x, gravity.y, gravity.z, 1.0f);
	gravityScreenSpace /= gravityScreenSpace.w;

	// スクリーン座標系における移動量
	glm::vec4 newPosScreenSpace(2.0 * newPos.x / WIN_WIDTH - 1.0, -2.0 * newPos.y / WIN_HEIGHT + 1.0, gravityScreenSpace.z, 1.0f);
	glm::vec4 oldPosScreenSpace(2.0 * oldPos.x / WIN_WIDTH - 1.0, -2.0 * oldPos.y / WIN_HEIGHT + 1.0, gravityScreenSpace.z, 1.0f);

	// スクリーン座標の情報をオブジェクト座標に変換する行列
	const glm::mat4 s2oMat = glm::inverse(projMat * viewMat * modelMat);

	// スクリーン空間の座標をオブジェクト空間に変換
	glm::vec4 newPosObjSpace = s2oMat * newPosScreenSpace;
	glm::vec4 oldPosObjSpace = s2oMat * oldPosScreenSpace;
	newPosObjSpace /= newPosObjSpace.w;
	oldPosObjSpace /= oldPosObjSpace.w;

	// オブジェクト座標系での移動量
	const glm::vec3 transObjSpace = glm::vec3(newPosObjSpace - oldPosObjSpace);

	// オブジェクト空間での平行移動
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
		// マウスの現在位置を更新
		newPos = glm::ivec2(xpos, ypos);

		// マウスがあまり動いていない時は処理をしない
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
	// OpenGLを初期化する
	if (glfwInit() == GL_FALSE) {
		fprintf(stderr, "Initialization failed!\n");
		return 1;
	}

	// OpenGLのバージョン指定
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Windowの作成
	GLFWwindow *window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, WIN_TITLE,
		NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "Window creation failed!");
		glfwTerminate();
		return 1;
	}

	// OpenGLの描画対象にWindowを追加
	glfwMakeContextCurrent(window);

	// キーボードのイベントを処理する関数を登録
	glfwSetKeyCallback(window, keyboardEvent);

	// マウスのイベントを処理する関数を登録
	glfwSetMouseButtonCallback(window, mouseEvent);

	// マウスの動きを処理する関数を登録
	glfwSetCursorPosCallback(window, mouseMoveEvent);

	glfwSetScrollCallback(window, wheelEvent);


	// OpenGL 3.x/4.xの関数をロードする (glfwMakeContextCurrentの後でないといけない)
	const int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0) {
		fprintf(stderr, "Failed to load OpenGL 3.x/4.x libraries!\n");
		return 1;
	}

	// バージョンを出力する
	printf("Load OpenGL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

	// ウィンドウのリサイズを扱う関数の登録
	glfwSetWindowSizeCallback(window, resizeGL);

	// OpenGLを初期化
	initializeGL();

	// メインループ
	while (glfwWindowShouldClose(window) == GL_FALSE) {
		// 描画
		paintGL();

		// アニメーション
		update();


		// 描画用バッファの切り替え
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
}
