// Windows includes (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.

// OpenGL includes
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Assimp includes
#include <assimp/cimport.h> // scene importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

// Project includes
//#include "maths_funcs.h"
#include "camera.h"
//typedef double DWORD;
vec3 PosData(0.0f, 0.0f, 3.0f);

Camera camera(PosData);


/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here
#define SNOWMAN_MESH "snowman.dae"
#define WINTER_MESH "winter1.dae"
#define TOPHAT_MESH "tophat.dae"
#define ARM_MESH "arm.dae"

/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

#pragma region SimpleTypes
typedef struct
{
	size_t mPointCount = 0;
	std::vector<vec3> mVertices;
	std::vector<vec3> mNormals;
	std::vector<vec2> mTextureCoords;
} ModelData;
#pragma endregion SimpleTypes

using namespace std;
GLuint shaderProgramID;


ModelData mesh_data, winter_data, tophat_data, arm_data;
unsigned int mesh_vao = 0;

int width = 800;
int height = 600;

GLuint loc1, loc2, loc3;
GLfloat rotate_y = 0.0f;

float pers_x = width / 2.0f;
float pers_y = height / 2.0f;

unsigned int snowman_vp_vbo = 0;
unsigned int snowman_vn_vbo = 0;

unsigned int winter_vn_vbo = 0;
unsigned int winter_vp_vbo = 0;

unsigned int tophat_vp_vbo = 0;
unsigned int tophat_vn_vbo = 0;

#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

ModelData load_mesh(const char* file_name) {
	ModelData modelData;

	/* Use assimp to read the model file, forcing it to be read as    */
	/* triangles. The second flag (aiProcess_PreTransformVertices) is */
	/* relevant if there are multiple meshes in the model file that   */
	/* are offset from the origin. This is pre-transform them so      */
	/* they're in the right position.                                 */
	const aiScene* scene = aiImportFile(
		file_name, 
		aiProcess_Triangulate | aiProcess_PreTransformVertices
	); 

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", file_name);
		return modelData;
	}

	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);
		modelData.mPointCount += mesh->mNumVertices;
		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				modelData.mVertices.push_back(vec3(vp->x, vp->y, vp->z));
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				modelData.mNormals.push_back(vec3(vn->x, vn->y, vn->z));
			}
			if (mesh->HasTextureCoords(0)) {
				const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
				modelData.mTextureCoords.push_back(vec2(vt->x, vt->y));
			}
			if (mesh->HasTangentsAndBitangents()) {
				/* You can extract tangents and bitangents here              */
				/* Note that you might need to make Assimp generate this     */
				/* data for you. Take a look at the flags that aiImportFile  */
				/* can take.                                                 */
			}
		}
	}

	aiReleaseImport(scene);
	return modelData;
}

#pragma endregion MESH LOADING

// Shader Functions- click on + to expand
#pragma region SHADER_FUNCTIONS
char* readShaderSource(const char* shaderFile) {
	FILE* fp;
	fopen_s(&fp, shaderFile, "rb");

	if (fp == NULL) { return NULL; }

	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);
	char* buf = new char[size + 1];
	fread(buf, 1, size, fp);
	buf[size] = '\0';

	fclose(fp);

	return buf;
}


static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		std::cerr << "Error creating shader..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	const char* pShaderSource = readShaderSource(pShaderText);

	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	// compile the shader and check for errors
	glCompileShader(ShaderObj);
	GLint success;
	// check for shader related errors using glGetShaderiv
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024] = { '\0' };
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		std::cerr << "Error compiling "
			<< (ShaderType == GL_VERTEX_SHADER ? "vertex" : "fragment")
			<< " shader program: " << InfoLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Attach the compiled shader object to the program object
	glAttachShader(ShaderProgram, ShaderObj);
}

GLuint CompileShaders()
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
	shaderProgramID = glCreateProgram();
	if (shaderProgramID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// Create two shader objects, one for the vertex, and one for the fragment shader
	AddShader(shaderProgramID, "simpleVertexShader.txt", GL_VERTEX_SHADER);
	AddShader(shaderProgramID, "simpleFragmentShader.txt", GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { '\0' };
	// After compiling all shader objects and attaching them to the program, we can finally link it
	glLinkProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	glValidateProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	glUseProgram(shaderProgramID);
	return shaderProgramID;
}
#pragma endregion SHADER_FUNCTIONS

// VBO Functions - click on + to expand
#pragma region VBO_FUNCTIONS
void generateObjectBufferMesh() {
	/*----------------------------------------------------------------------------
	LOAD MESH HERE AND COPY INTO BUFFERS
	----------------------------------------------------------------------------*/

	//Note: you may get an error "vector subscript out of range" if you are using this code for a mesh that doesnt have positions and normals
	//Might be an idea to do a check for that before generating and binding the buffer.

	//This is where we load mesh
	//SNOWMAN MESH
	mesh_data = load_mesh(SNOWMAN_MESH);
	winter_data = load_mesh(WINTER_MESH);
	tophat_data = load_mesh(TOPHAT_MESH);
	
	loc1 = glGetAttribLocation(shaderProgramID, "vertex_position");
	loc2 = glGetAttribLocation(shaderProgramID, "vertex_normal");
	loc3 = glGetAttribLocation(shaderProgramID, "vertex_texture");

	snowman_vp_vbo = 0;
	glGenBuffers(1, &snowman_vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, snowman_vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mVertices[0], GL_STATIC_DRAW);

	snowman_vn_vbo = 0;
	glGenBuffers(1, &snowman_vn_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, snowman_vn_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mNormals[0], GL_STATIC_DRAW);

	winter_vp_vbo = 0;
	glGenBuffers(1, &winter_vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, winter_vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, winter_data.mPointCount * sizeof(vec3), &winter_data.mVertices[0], GL_STATIC_DRAW);

	winter_vn_vbo = 0;
	glGenBuffers(1, &winter_vn_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, winter_vn_vbo);
	glBufferData(GL_ARRAY_BUFFER, winter_data.mPointCount * sizeof(vec3), &winter_data.mNormals[0], GL_STATIC_DRAW);

	tophat_vp_vbo = 0;
	glGenBuffers(1, &tophat_vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, tophat_vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, tophat_data.mPointCount * sizeof(vec3), &tophat_data.mVertices[0], GL_STATIC_DRAW);

	tophat_vn_vbo = 0;
	glGenBuffers(1, &tophat_vn_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, tophat_vn_vbo);
	glBufferData(GL_ARRAY_BUFFER, tophat_data.mPointCount * sizeof(vec3), &tophat_data.mVertices[0], GL_STATIC_DRAW);



	//unsigned int wintervp_vbo = 0;
	//glGenBuffers(1, &wintervp_vbo);
	//glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	//glBufferData(GL_ARRAY_BUFFER, winter_data.mPointCount * sizeof(vec3), &winter_data.mVertices[0], GL_STATIC_DRAW);

	//	This is for texture coordinates which you don't currently need, so I have commented it out
	//	unsigned int vt_vbo = 0;
	//	glGenBuffers (1, &vt_vbo);
	//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	//	glBufferData (GL_ARRAY_BUFFER, monkey_head_data.mTextureCoords * sizeof (vec2), &monkey_head_data.mTextureCoords[0], GL_STATIC_DRAW);



	//	This is for texture coordinates which you don't currently need, so I have commented it out
	//	glEnableVertexAttribArray (loc3);
	//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	//	glVertexAttribPointer (loc3, 2, GL_FLOAT, GL_FALSE, 0, NULL);
}
#pragma endregion VBO_FUNCTIONS

GLfloat angle = 0;


void display() {

	//tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); //enable depth-testing
	glDepthFunc(GL_LESS); //depth-testing interprets a smaller value as "closer"
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgramID);


	//Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation(shaderProgramID, "model");
	int view_mat_location = glGetUniformLocation(shaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation(shaderProgramID, "proj");

	//Root of the Hierarchy
	mat4 view = camera.GetViewMatrix();
	mat4 persp_proj = perspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);

	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);

	//Snowman
	mat4 model = identity_mat4();
	model = translate(model, vec3(-10.0, -10.0, 0.0f));

	glEnableVertexAttribArray(loc1);
	glBindBuffer(GL_ARRAY_BUFFER, snowman_vp_vbo);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc2);
	glBindBuffer(GL_ARRAY_BUFFER, snowman_vn_vbo);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, model.m);
	glDrawArrays(GL_TRIANGLES, 0, mesh_data.mPointCount);


	mat4 snow = identity_mat4();
	snow = translate(snow, vec3(0.0, 0.0, 0.0f));
	glEnableVertexAttribArray(loc1);
	glBindBuffer(GL_ARRAY_BUFFER, winter_vp_vbo);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc2);
	glBindBuffer(GL_ARRAY_BUFFER, winter_vn_vbo);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, snow.m);
	glDrawArrays(GL_TRIANGLES, 1, winter_data.mPointCount);


	// Set up the child matrix
	mat4 tophat = identity_mat4();
	tophat = rotate_y_deg(tophat, angle);
	//glm::mat4 trans = glm::mat4(1.0f);
	//trans = glm::translate(trans, glm::vec3(0.5f, -0.5f, 0.0f));
	//trans = glm::rotate(trans, (float)glfwGetTime(), glm::vec3(0.0f, 0.0f, 1.0f));

	tophat = translate(tophat, vec3(0.0f, 0.0f, 0.0f));
	tophat = model * tophat;


	glEnableVertexAttribArray(loc1);
	glBindBuffer(GL_ARRAY_BUFFER, tophat_vp_vbo);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc2);
	glBindBuffer(GL_ARRAY_BUFFER, tophat_vn_vbo);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, tophat.m);
	//trans = rotate(trans, (float)glfwGetTime(), glm::vec3(0.0f, 0.0f, 1.0f));
	glDrawArrays(GL_TRIANGLES, 2, tophat_data.mPointCount);
	//glDrawArrays(GL_TRIANGLES, 1, winter_data.mPointCount);
	//tophat = rotate_z_deg(tophat, 180);
	//tophat = rotate_y_deg(tophat, rotate_y);

	glutSwapBuffers();
}


void updateScene() {

	static DWORD last_time = 0;
	DWORD curr_time = timeGetTime();
	if (last_time == 0)
		last_time = curr_time;
	float delta = (curr_time - last_time) * 0.001f;
	last_time = curr_time;

	// Rotate the model slowly around the y axis at 20 degrees per second
	//rotate_y += 20.0f * delta;
	//rotate_y = fmodf(rotate_y, 360.0f);

	angle += 1.0f;

	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	// Set up the shaders
	GLuint shaderProgramID = CompileShaders();
	//load mesh into a vertex buffer array
	//glGenVertexArrays(1, &winter_data.vao);
	//generateObjectBufferMesh(winter);

	generateObjectBufferMesh();

}

// Placeholder code for the keypress
void keypress(unsigned char key, int x, int y) {
	if (key == 'w') {
		camera.ProcessKeyboard(FORWARD, 0.1);
	}
	if (key == 's') {
		camera.ProcessKeyboard(BACKWARD, 0.1);
	}
	if (key == 'a') {
		camera.ProcessKeyboard(LEFT, 0.1);
	}
	if (key == 'd') {
		camera.ProcessKeyboard(RIGHT, 0.1);
	}

}


void mouse(int x, int y) {
	float xoffset = x - pers_x;
	float yoffset = pers_y - y; // reversed since y-coordinates go from bottom to top

	pers_x = x;
	pers_y = y;

	camera.ProcessMouseMovement(xoffset, yoffset);
}


int main(int argc, char** argv) {

	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow("Snowman");

	// Tell glut where the display function is
	glutDisplayFunc(display);
	glutIdleFunc(updateScene);
	glutKeyboardFunc(keypress);
	glutPassiveMotionFunc(mouse);


	// A call to glewInit() must be done after glut is initialized!
	GLenum res = glewInit();
	// Check for any errors
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}
	// Set up your objects and shaders
	init();
	// Begin infinite event loop
	glutMainLoop();
	return 0;
}
