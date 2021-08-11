/*
	Program:     CSS-330: Module 6 Milestone
	Author:      Drew Townsend
	Date:        08/06/21
	Description: This program renders a 3D scene, with Rubik's Cube sitting atop a book. The plane is a marble tabletop.
				 There is a moveable light source that provides ambience, reflections, and attenuation to the scene

	Controls:
	WASD -							[Control forward and side movement of camera]
	Mouse -							[Control panning of camera]
	ScrollWheel -					[Control speed of movement]
	E -								[Increase camera's Z axis]
	Q -								[Decrease camera's Z axis]
	P -								[*Sensitive to press* Changes Projection]
	Arrow Keys -                    [Controls light source's X and Y axis]
	Right shift and Right CTRL -    [Controls Light source's Z axis]

	Todo: Separate shaders, VBOs, camera, etc... into their own classes for readability

*/

// including libraries
#include <iostream>
#include <cstdlib>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// GLM Math Headers
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace std;

// Shader program macro
#ifndef GLSL
#define GLSL(Version, Source) "#version " #Version " core \n" #Source
#endif

// Unnamed namespace
namespace
{
	const char* const WINDOW_TITLE = "Drew Townsend - Module 6 Milestone"; // Window title

	// Window width and height
	const int WINDOW_WIDTH = 800;
	const int WINDOW_HEIGHT = 600;

	// Declaring unsigned ints for vertex array and buffer, as well as number of indices
	struct GLMesh
	{
		GLuint vao;
		GLuint vbo;
		GLuint nIndices;
	};

	// defining main window
	GLFWwindow* gWindow = nullptr;
	// Triangle mesh data
	GLMesh gMesh;
	// Texture IDs
	GLuint texture0, texture1, texture2, texture3;
	// defining both shader programs
	GLuint gProgramId;

	// global cam variables
	glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

	// deltaTime ensures all users have similar movement speed
	float deltaTime = 0.0f;
	float lastFrame = 0.0f;

	// initial mouse coords
	float lastX = 400, lastY = 300;

	// values for mouse input
	float yaw = 0.0f, pitch = 0.0f;
	bool firstMouse = true;
	float sensitivity = 0.1f;
	float scrollSpeed = 0.1f;

	// bool to change perspective to ortho
	bool perspective = true;

	// Checking to see if projection was changed on last frame
	bool lastFrameCheck = false;

	// light color
	glm::vec3 gLightColor(1.0f, 1.0f, 1.0f);

	// Light position and scale
	float lX = 0.1f, lY = 0.04f, lZ = 3.5f; // Allows light source variables to be changed
	glm::vec3 gLightPosition(lX, lY, lZ);
	glm::vec3 gLightScale(0.3f);
}

// Initializes libraries and window/context
bool UInitialize(int, char* [], GLFWwindow** window);
// Resizes active window if user or program changes size
void UResizeWindow(GLFWwindow* window, int width, int height);
// Processes input (like a keyboard key) and does something with it
void UProcessInput(GLFWwindow* window);
// Mouse scroll callback
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
// Setting index locations, colors, etc...
void UCreateMesh(GLMesh& mesh);
// Destroys locations
void UDestroyMesh(GLMesh& mesh);
// Actually renders the pyramid and allows for transformations
void URender();
// Creates, compiles, and deleted shader programs (when error occurs)
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId);
// Deleting shader programs
void UDestroyShaderProgram(GLuint programId);
// Captures mouse events commented out for now
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
// Loads texture for placing
bool UCreateTexture(const char* filename, GLuint& textureId);
// Deallocates memory from texture
void UDestroyTexture(GLuint textureId);
// Flipping image on Y axis
void flipImageVertically(unsigned char* image, int width, int height, int channels);


// Vertex Shader Source Code
const GLchar* objectVertexShaderSource = GLSL(440,
	layout(location = 0) in vec3 position;
	layout(location = 1) in vec3 normal;
	layout(location = 2) in vec2 textureCoordinate;

	out vec3 vertexNormal;
	out vec3 vertexFragmentPos;
	out vec2 vertexTextureCoordinate;

	// variables to be used for transforming
	uniform mat4 model;
	uniform mat4 view;
	uniform mat4 projection;

	void main()
	{
		gl_Position = projection * view * model * vec4(position, 1.0f); // transforming vertices to clip coords

		vertexFragmentPos = vec3(model * vec4(position, 1.0f));

		vertexNormal = mat3(transpose(inverse(model))) * normal;
		vertexTextureCoordinate = textureCoordinate; // receives color data
	}
);



// Fragment shader source code
const GLchar* objectFragmentShaderSource = GLSL(440,
	in vec3 vertexNormal; // For incoming normals
	in vec3 vertexFragmentPos; // For incoming fragment position
	in vec2 vertexTextureCoordinate; // For incoming texture coordinates

	// Implementing attenuation
	struct Light {
		vec3 position;
		vec3 ambient;
		vec3 diffuse;
		vec3 specular;
		
		float constant;
		float linear;
		float quadratic;
	};

	out vec4 fragmentColor; // For outgoing pyramid color to the GPU

	// Uniform / Global variables for object color, light color, light position, and camera/view position
	uniform vec3 lightPos;
	uniform vec3 viewPosition;
	uniform sampler2D uTexture; // Useful when working with multiple textures
	uniform Light light;

	layout(binding = 3) uniform sampler2D texSampler1;

	void main()
	{
		/*Phong lighting model calculations to generate ambient, diffuse, and specular components*/
		// Calc ambient lighting
		vec3 ambient = light.ambient * texture(uTexture, vertexTextureCoordinate).rgb;

		// Calc Diffuse Lighting
		vec3 norm = normalize(vertexNormal);
		vec3 lightDirection = normalize(lightPos - vertexFragmentPos);
		float impact = max(dot(norm, lightDirection), 0.0);
		vec3 diffuse = light.diffuse * impact * texture(uTexture, vertexTextureCoordinate).rgb;

		// Calc Specular lighting
		float highlightSize = 32.0f;
		vec3 viewDir = normalize(viewPosition - vertexFragmentPos);
		vec3 reflectDir = reflect(-lightDirection, norm);
		float specularComponent = pow(max(dot(viewDir, reflectDir), 0.0), highlightSize);
		vec3 specular = light.specular * specularComponent * texture(uTexture, vertexTextureCoordinate).rgb;

		// attenuation
		float distance = length(light.position - vertexFragmentPos);
		float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

		ambient		*= attenuation;
		diffuse		*= attenuation;
		specular	*= attenuation;

		vec3 result = ambient + diffuse + specular;

		fragmentColor = vec4(result, 1.0);
	}
);

int main(int argc, char* argv[])
{
	if (!UInitialize(argc, argv, &gWindow))
		return EXIT_FAILURE;

	UCreateMesh(gMesh);

	if (!UCreateShaderProgram(objectVertexShaderSource, objectFragmentShaderSource, gProgramId))
		return EXIT_FAILURE;

	// Telling OpenGL which texture the sample is connected to, which is unit 0
	glUseProgram(gProgramId);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	// render loop
	while (!glfwWindowShouldClose(gWindow))
	{
		UProcessInput(gWindow);

		// bind texture on corresponding texture unit
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, texture1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, texture2);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, texture3);

		URender();

		glfwPollEvents();

		glfwSetCursorPosCallback(gWindow, mouse_callback);
	}

	UDestroyMesh(gMesh);

	// release textures
	UDestroyTexture(texture0);
	UDestroyTexture(texture1);
	UDestroyTexture(texture2);
	UDestroyTexture(texture3);

	UDestroyShaderProgram(gProgramId);

	exit(EXIT_SUCCESS);
}

bool UInitialize(int argc, char* argv[], GLFWwindow** window)
{
	glfwInit(); // initializing GLFW library
	// Setting OpenGL versions
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	// Core profile allows access to subset of OpenGL features without backwards-compatibility
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// If device is using MacOS, enable forward compatibility
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// Creating GLFW window using previously defined variables
	* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
	// If creation fails, alert user and terminate
	if (*window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return false;
	}

	// setting window context as current and setting framebuffer resize callback of window
	glfwMakeContextCurrent(*window);
	glfwSetFramebufferSizeCallback(*window, UResizeWindow);
	glfwSetScrollCallback(*window, UMouseScrollCallback);

	// When window has focus on PC, disable mouse cursor
	glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Enabling use of experimental/pre-release drivers
	glewExperimental = GL_TRUE;
	// initializing GLEW library
	GLenum GlewInitResult = glewInit();
	// Ensuring GLEW properly initialized
	if (GLEW_OK != GlewInitResult)
	{
		std::cerr << glewGetErrorString(GlewInitResult) << std::endl;
		return false;
	}

	// Displaying GPU OpenGL version
	cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << endl;

	return true;
}

// if declared key(s) are pressed during this frame, do something
void UProcessInput(GLFWwindow* window)
{
	// Checking if 'escape' key was pressed. If so, close window.
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	const float cameraSpeed = 2.5f * scrollSpeed;
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		cameraPos += cameraSpeed * cameraFront;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		cameraPos -= cameraSpeed * cameraFront;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		cameraPos -= cameraSpeed * cameraUp;
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
		cameraPos += cameraSpeed * cameraUp;
	if (glfwGetKey(window, GLFW_KEY_LEFT))
	{
		lX -= 0.01;
		cout << lX << endl;
	}
	if (glfwGetKey(window, GLFW_KEY_RIGHT))
	{
		lX += 0.01;
		cout << lX << endl;
	}
	if (glfwGetKey(window, GLFW_KEY_UP))
	{
		lY += 0.01;
		cout << lY << endl;
	}
	if (glfwGetKey(window, GLFW_KEY_DOWN))
	{
		lY -= 0.01;
		cout << lY << endl;
	}
	if (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT))
	{
		lZ += 0.01;
		cout << lZ << endl;
	}
	if (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL))
	{
		lZ -= 0.01;
		cout << lZ << endl;
	}

	// press "p" to change projections
	if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
	{
		// Checking to see if projection was changed on last frame
		if (!lastFrameCheck)
		{
			perspective = !perspective;
			cout << "Projection Changed" << endl;
			lastFrameCheck = true;
		}
		else
		{
			lastFrameCheck = false;
		}

	}
}


void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	if (yoffset > 0)
	{
		scrollSpeed += 0.01;
	}
	else
	{
		if (scrollSpeed > 0.01) // limiting speed to a minimum of 0.
		{
			scrollSpeed -= 0.01;
		}
	}
	cout << scrollSpeed << endl;
}

// Set viewport if window is resized
void UResizeWindow(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

// URender will render the frame. This function is in the while loop within main()
void URender()
{
	// Lamp orbits around the origin
	gLightPosition.x = lX;
	gLightPosition.y = lY;
	gLightPosition.z = lZ;

	// Enabling z-depth
	glEnable(GL_DEPTH_TEST);

	// Clear frame to black, clear the z buffers
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Activate VAO
	glBindVertexArray(gMesh.vao);

	// 1. Scales the object by 2
	glm::mat4 scale = glm::scale(glm::vec3(2.0f, 2.0f, 2.0f));
	// 2. Place object at the origin
	glm::mat4 translation = glm::translate(glm::vec3(0.0f, 0.0f, 0.0f));
	// Model matrix: transformations are applied right-to-left order
	glm::mat4 model = translation * scale;

	// Defining perspective projection to start, however pressing P will change perspective to ortho
	glm::mat4 projection = glm::perspective(1.0f, GLfloat(WINDOW_WIDTH / WINDOW_HEIGHT), 0.1f, 100.0f);
	if (perspective)
	{
		projection = glm::perspective(1.0f, GLfloat(WINDOW_WIDTH / WINDOW_HEIGHT), 0.1f, 100.0f);
	}
	else
	{
		projection = glm::ortho(0.0f, 5.0f, 0.0f, 5.0f, 0.1f, 100.0f);
	}

	const float radius = 10.0f;
	float camX = sin(glfwGetTime()) * radius;
	float camZ = cos(glfwGetTime()) * radius;

	// new camera view that allows movement. commented out for now
	glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

	glUseProgram(gProgramId);

	// Program 1
	GLint modelLoc = glGetUniformLocation(gProgramId, "model");
	GLint viewLoc = glGetUniformLocation(gProgramId, "view");
	GLint projLoc = glGetUniformLocation(gProgramId, "projection");

	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

	// Reference matrix uniforms for Shader program
	GLint lightPositionLoc = glGetUniformLocation(gProgramId, "lightPos");
	GLint viewPositionLoc = glGetUniformLocation(gProgramId, "viewPosition");
	GLfloat lightConstantLoc = glGetUniformLocation(gProgramId, "light.constant");
	GLfloat lightLinearLoc = glGetUniformLocation(gProgramId, "light.linear");
	GLfloat lightQuadraticLoc = glGetUniformLocation(gProgramId, "light.quadratic");
	GLint lightAmbientLoc = glGetUniformLocation(gProgramId, "light.ambient");
	GLint lightDiffuseLoc = glGetUniformLocation(gProgramId, "light.diffuse");
	GLint lightSpecularLoc = glGetUniformLocation(gProgramId, "light.specular");


	// Pass color, light, and camera data to the pyramid Shader program's corresponding uniforms
	glUniform3f(lightPositionLoc, gLightPosition.x, gLightPosition.y, gLightPosition.z);
	const glm::vec3 cameraPosition = (cameraPos, cameraPos + cameraFront, cameraUp);
	glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);
	// Setting float variables for caster
	glUniform3f(lightAmbientLoc, 0.1f, 0.1f, 0.1f);
	glUniform3f(lightDiffuseLoc, 0.8f, 0.8f, 0.8f);
	glUniform3f(lightSpecularLoc, 1.0f, 1.0f, 1.0f);
	glUniform1f(lightConstantLoc, 1.0f);
	glUniform1f(lightLinearLoc, 0.09f);
	glUniform1f(lightQuadraticLoc, 0.032f);

	// Loading textures and drawing objects
	// Marble texture
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 0);
	// Tabletop vertices
	glDrawArrays(GL_TRIANGLES, 0, gMesh.nIndices + 6);
	// Book Cover Texture
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 1);
	// Book
	glDrawArrays(GL_TRIANGLES, 0, gMesh.nIndices + 42);

	// Rubik's Cube Texture
	glUniform1i(glGetUniformLocation(gProgramId, "uTexture"), 2);
	// Cube
	glDrawArrays(GL_TRIANGLES, 0, gMesh.nIndices + 79);

	glBindVertexArray(0);

	glfwSwapBuffers(gWindow);
}

// UCreateMesh contains positions and color data, and ensures data is in GPU memory
void UCreateMesh(GLMesh& mesh)
{
	// Position, Normal, and texture data for objects
	GLfloat verts[] = {
		// Table top			// Plane Normal		// Texture Coords
		-1.0f, -1.0f,   0.0f,	0.0f, 0.0f, 1.0f,	0.0f,  0.0f, // bottom left vertex
		-1.0f,  1.0f,   0.0f,	0.0f, 0.0f, 1.0f,	0.0f,  1.0f, // top left vertex
		 1.0f,  1.0f,   0.0f,	0.0f, 0.0f, 1.0f,	1.0f,  1.0f, // top right vertex
		 1.0f,  1.0f,   0.0f,	0.0f, 0.0f, 1.0f,	1.0f,  1.0f, // top right vertex
		 1.0f, -1.0f,   0.0f,	0.0f, 0.0f, 1.0f,	1.0f,  0.0f, // bottom right vertex
		-1.0f, -1.0f,   0.0f,	0.0f, 0.0f, 1.0f,	0.0f,  0.0f, // bottom left vertex

		// Book
		// Front Face			// Plane Normal		// Texture Coords
		-1.0f, -1.0f,   0.1f,	0.0f, 0.0f, 1.0f,	0.15f,  0.5f, // bottom left vertex
		-1.0f,  0.0f,   0.1f,	0.0f, 0.0f, 1.0f,	0.15f, 0.94f, // top left vertex
		-0.5f,  0.0f,   0.1f,	0.0f, 0.0f, 1.0f,	0.83f, 0.94f, // top right vertex
		-0.5f,  0.0f,   0.1f,	0.0f, 0.0f, 1.0f,	0.83f, 0.94f, // top right vertex
		-0.5f, -1.0f,   0.1f,	0.0f, 0.0f, 1.0f,	0.83f,  0.5f, // bottom right vertex
		-1.0f, -1.0f,   0.1f,	0.0f, 0.0f, 1.0f,	0.15f,  0.5f, // bottom left vertex
		// Back Face
		-1.0f, -1.0f, 0.001f,	0.0f, 0.0f,-1.0f,   0.15f,  0.0f, // bottom left vertex
		-1.0f,  0.0f, 0.001f,	0.0f, 0.0f,-1.0f,   0.15f, 0.44f, // top left vertex
		-0.5f,  0.0f, 0.001f,	0.0f, 0.0f,-1.0f,   0.83f, 0.44f, // top right vertex
		-0.5f,  0.0f, 0.001f,	0.0f, 0.0f,-1.0f,   0.83f, 0.44f, // top right vertex
		-0.5f, -1.0f, 0.001f,	0.0f, 0.0f,-1.0f,   0.83f,  0.0f, // bottom right vertex
		-1.0f, -1.0f, 0.001f,	0.0f, 0.0f,-1.0f,   0.15f,  0.0f, // bottom left vertex
		// Left Face
		-1.0f, -1.0f,   0.1f,  -1.0f, 0.0f, 0.0f,	0.15f,  0.5f, // bottom left vertex
		-1.0f,  0.0f,   0.1f,  -1.0f, 0.0f, 0.0f,	0.15f, 0.94f, // top left vertex
		-1.0f,  0.0f, 0.001f,  -1.0f, 0.0f, 0.0f,	0.0f,  0.94f, // back top left vertex
		-1.0f,  0.0f, 0.001f,  -1.0f, 0.0f, 0.0f,	0.0f,  0.94f, // back top left vertex
		-1.0f, -1.0f, 0.001f,  -1.0f, 0.0f, 0.0f,	0.0f,   0.5f, // back bottom left vertex
		-1.0f, -1.0f,   0.1f,  -1.0f, 0.0f, 0.0f,	0.15f,  0.5f, // bottom left vertex
		// Right Face
		-0.5f, -1.0f,   0.1f,   1.0f, 0.0f, 0.0f,	1.0f,  0.5f, // bottom right vertex
		-0.5f,  0.0f,   0.1f,   1.0f, 0.0f, 0.0f,   1.0f, 0.94f, // top right vertex
		-0.5f,  0.0f, 0.001f,   1.0f, 0.0f, 0.0f,	0.83f, 0.94f, // back top right vertex
		-0.5f,  0.0f, 0.001f,   1.0f, 0.0f, 0.0f,	0.83f, 0.94f, // back top right vertex
		-0.5f, -1.0f, 0.001f,   1.0f, 0.0f, 0.0f,	0.83f,  0.5f, // back bottom right vertex
		-0.5f, -1.0f,   0.1f,   1.0f, 0.0f, 0.0f,	1.0f,  0.5f, // bottom right vertex
		// Top Face
		-1.0f,  0.0f,   0.1f,	0.0f, 1.0f, 0.0f,   0.15f, 0.94f, // top left vertex
		-1.0f,  0.0f, 0.001f,	0.0f, 1.0f, 0.0f,	0.15f,  1.0f, // back top left vertex
		-0.5f,  0.0f, 0.001f,	0.0f, 1.0f, 0.0f,	0.83f,  1.0f, // back top right vertex
		-0.5f,  0.0f, 0.001f,	0.0f, 1.0f, 0.0f,	0.83f,  1.0f, // back top right vertex
		-0.5f,  0.0f,   0.1f,	0.0f, 1.0f, 0.0f,	0.83f, 0.94f, // top right vertex
		-1.0f,  0.0f,   0.1f,	0.0f, 1.0f, 0.0f,	0.15f, 0.94f, // top left vertex
		// Bottom Face
		-1.0f, -1.0f,   0.1f,	0.0f,-1.0f, 0.0f,	 0.15f,  0.5f, // bottom left vertex
		-1.0f, -1.0f, 0.001f,	0.0f,-1.0f, 0.0f,	 0.15f, 0.44f, // back bottom left vertex
		-0.5f, -1.0f, 0.001f,	0.0f,-1.0f, 0.0f,	 0.83f, 0.44f, // back bottom right vertex
		-0.5f, -1.0f, 0.001f,	0.0f,-1.0f, 0.0f,	 0.83f, 0.44f, // back bottom right vertex
		-0.5f, -1.0f,   0.1f,	0.0f,-1.0f, 0.0f,    0.83f,  0.5f, // bottom right vertex
		-1.0f, -1.0f,   0.1f,	0.0f,-1.0f, 0.0f,    0.15f,  0.5f, // bottom left vertex

		// Rubik's Cube
		// Front Face
		-0.75f,-0.25f,0.351f,	0.0f, 0.0f, 1.0f,		 0.34f,  0.5f, // bottom left vertex
		-0.75f, 0.0f, 0.351f,	0.0f, 0.0f, 1.0f,		 0.34f, 0.75f, // top left vertex
		-0.5f,  0.0f, 0.351f,	0.0f, 0.0f, 1.0f,		 0.66f, 0.75f, // top right vertex
		-0.5f,  0.0f, 0.351f,	0.0f, 0.0f, 1.0f,		 0.66f, 0.75f, // top right vertex
		-0.5f, -0.25f,0.351f,	0.0f, 0.0f, 1.0f,		 0.66f,  0.5f, // bottom right vertex
		-0.75f,-0.25f,0.351f,	0.0f, 0.0f, 1.0f,		 0.34f,  0.5f, // bottom left vertex
		// Back Face
		-0.75f,-0.25f,0.101f,	0.0f, 0.0f,-1.0f,		 0.34f,  0.0f, // back bottom left vertex
		-0.75f, 0.0f, 0.101f,	0.0f, 0.0f,-1.0f,	 0.34f, 0.25f, // back top left vertex
		-0.5f,  0.0f, 0.101f,	0.0f, 0.0f,-1.0f,   0.665f, 0.25f, // back top right vertex
		-0.5f,  0.0f, 0.101f,	0.0f, 0.0f,-1.0f,	0.665f, 0.25f, // back top right vertex
		-0.5f, -0.25f,0.101f,	0.0f, 0.0f,-1.0f,	0.665f,  0.0f, // back bottom right vertex
		-0.75f,-0.25f,0.101f,	0.0f, 0.0f,-1.0f,	 0.34f,  0.0f, // back bottom left vertex
		// Left Face
		-0.75f,-0.25f,0.351f,  -1.0f, 0.0f, 0.0f,	 0.33f,  0.5f, // bottom left vertex
		-0.75f, 0.0f, 0.351f,  -1.0f, 0.0f, 0.0f,	 0.33f, 0.75f, // top left vertex
		-0.75f, 0.0f, 0.101f,  -1.0f, 0.0f, 0.0f,	  0.0f, 0.75f, // back top left vertex
		-0.75f, 0.0f, 0.101f,  -1.0f, 0.0f, 0.0f,	  0.0f, 0.75f, // back top left vertex
		-0.75f,-0.25f,0.101f,  -1.0f, 0.0f, 0.0f,	  0.0f,  0.5f, // back bottom left vertex
		-0.75f,-0.25f,0.351f,  -1.0f, 0.0f, 0.0f,	 0.33f,  0.5f, // bottom left vertex
		// Right Face
		-0.5f, -0.25f,0.351f,   1.0f, 0.0f, 0.0f,	 0.66f,  0.5f, // bottom right vertex
		-0.5f,  0.0f, 0.351f,   1.0f, 0.0f, 0.0f,	 0.66f, 0.75f, // top right vertex
		-0.5f,  0.0f, 0.101f,   1.0f, 0.0f, 0.0f,	  1.0f, 0.75f, // back top right vertex
		-0.5f,  0.0f, 0.101f,   1.0f, 0.0f, 0.0f,	  1.0f, 0.75f, // back top right vertex
		-0.5f, -0.25f,0.101f,   1.0f, 0.0f, 0.0f,	  1.0f,  0.5f, // back bottom right vertex
		-0.5f, -0.25f,0.351f,   1.0f, 0.0f, 0.0f,	 0.66f,  0.5f, // bottom right vertex
		// Top Face
		-0.75f, 0.0f, 0.351f,	0.0f, 1.0f, 0.0f,	 0.34f, 0.75f, // top left vertex
		-0.75f, 0.0f, 0.101f,	0.0f, 1.0f, 0.0f,	 0.34f,  1.0f, // back top left vertex
		-0.5f,  0.0f, 0.101f,	0.0f, 1.0f, 0.0f,	0.665f,  1.0f, // back top right vertex
		-0.5f,  0.0f, 0.101f,	0.0f, 1.0f, 0.0f,	0.665f,  1.0f, // back top right vertex
		-0.5f,  0.0f, 0.351f,	0.0f, 1.0f, 0.0f,	0.665f, 0.75f, // top right vertex
		-0.75f, 0.0f, 0.351f,	0.0f, 1.0f, 0.0f,	 0.34f, 0.75f, // top left vertex
		// Bottom Face
		-0.75f,-0.25f,0.351f,	0.0f,-1.0f, 0.0f,	 0.34f,  0.5f, // bottom left vertex
		-0.75f,-0.25f,0.101f,	0.0f,-1.0f, 0.0f,	 0.34f, 0.25f, // back bottom left vertex
		-0.5f, -0.25f,0.101f,	0.0f,-1.0f, 0.0f,	0.665f, 0.25f, // back bottom right vertex
		-0.5f, -0.25f,0.101f,	0.0f,-1.0f, 0.0f,	0.665f, 0.25f, // back bottom right vertex
		-0.5f, -0.25f,0.351f,	0.0f,-1.0f, 0.0f,	0.665f,  0.5f, // bottom right vertex
		-0.75f,-0.25f,0.351f,	0.0f,-1.0f, 0.0f,	 0.34f,  0.5f, // bottom left vertex
	};

	const GLuint floatsPerVertex = 3;
	const GLuint floatsPerNormal = 3;
	const GLuint floatsPerUV = 2;

	// Basically setting a start point for drawing arrays
	mesh.nIndices = 0;

	// generating vertex array object names
	glGenVertexArrays(1, &mesh.vao);
	// Binding generated vertex array name
	glBindVertexArray(mesh.vao);

	// Generates buffers for vertex data and indices
	glGenBuffers(1, &mesh.vbo);
	// Binding vertex data
	glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
	// Sending vertex/coordinate data to GPU
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	// Setting stride, which is 5 (3 + 2)
	GLint stride = sizeof(float) * (floatsPerVertex + floatsPerNormal + floatsPerUV);

	// Creating vertex attrib pointers
	glVertexAttribPointer(0, floatsPerVertex, GL_FLOAT, GL_FALSE, stride, 0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, floatsPerNormal, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)* floatsPerVertex));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, floatsPerUV, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * (floatsPerVertex + floatsPerNormal)));
	glEnableVertexAttribArray(2);

	// Marble Texture
	const char* texFilename = "../CS330 Mod6Milestone/Resources/Textures/marble.jfif";
	if (!UCreateTexture(texFilename, texture0))
	{
		cout << "Failed to load texture " << texFilename << endl;
	}
	// Book Texture
	texFilename = "../CS330 Mod6Milestone/Resources/Textures/gulagArchipelago.png";
	if (!UCreateTexture(texFilename, texture1))
	{
		cout << "Failed to load texture " << texFilename << endl;
	}
	// Rubik's Cube Texture
	texFilename = "../CS330 Mod6Milestone/Resources/Textures/rubikscube.png";
	if (!UCreateTexture(texFilename, texture2))
	{
		cout << "Failed to load texture " << texFilename << endl;
	}
	// Dust Texture
	texFilename = "../CS330 Mod6Milestone/Resources/Textures/dust.jpg";
	if (!UCreateTexture(texFilename, texture3))
	{
		cout << "Failed to load texture " << texFilename << endl;
	}
}

void UDestroyMesh(GLMesh& mesh)
{
	// Delete mesh
	glDeleteVertexArrays(1, &mesh.vao);
	glDeleteBuffers(1, &mesh.vbo);
}

bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId)
{
	// for comp and linkage error reporting
	int success = 0;
	char infoLog[512];

	// Creating shader program object
	programId = glCreateProgram();

	// Creating vertex and fragment shader objects
	GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

	// get shader sources
	glShaderSource(vertexShaderId, 1, &vtxShaderSource, NULL);
	glShaderSource(fragmentShaderId, 1, &fragShaderSource, NULL);

	// compile vertex shader
	glCompileShader(vertexShaderId);
	// check for compile errors
	glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShaderId, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;

		return false;
	}

	// compile fragment shader
	glCompileShader(fragmentShaderId);
	// check for compile errors
	glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShaderId, sizeof(infoLog), NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;

		return false;
	}

	// Attach shaders to shader program
	glAttachShader(programId, vertexShaderId);
	glAttachShader(programId, fragmentShaderId);

	glLinkProgram(programId); // link shader program
	// check for linking errors
	glGetProgramiv(programId, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(programId, sizeof(infoLog), NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;

		return false;
	}

	glUseProgram(programId);

	return true;
}

void UDestroyShaderProgram(GLuint programId)
{
	glDeleteProgram(programId);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos;
	lastX = xpos;
	lastY = ypos;

	// modify if mouse sensitivity is too low or high
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	glm::vec3 direction;
	direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	direction.y = sin(glm::radians(pitch));
	direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(direction);
}

bool UCreateTexture(const char* filename, GLuint& textureId)
{
	int width, height, channels;
	unsigned char* image = stbi_load(filename, &width, &height, &channels, 0);

	if (image)
	{
		flipImageVertically(image, width, height, channels);

		// generates texture names
		glGenTextures(1, &textureId);
		// binding texure to 2D texture
		glBindTexture(GL_TEXTURE_2D, textureId);

		// set texture wrapping params
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering params
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Assigning texture to pointer, and defining how it will be stored in memory
		if (channels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		else if (channels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			cout << "Not implemented to handle image with " << channels << "channels" << endl;
			return false;
		}

		// generating mipmap for GL_TEXTURE_2D
		glGenerateMipmap(GL_TEXTURE_2D);

		// free loaded image
		stbi_image_free(image);
		// rebinding GL_TEXTURE_2D to nothing
		glBindTexture(GL_TEXTURE_2D, 0);

		return true;
	}

	return false;
}

void UDestroyTexture(GLuint textureId)
{
	glGenTextures(1, &textureId);
}

void flipImageVertically(unsigned char* image, int width, int height, int channels)
{
	for (int j = 0; j < height / 2; ++j)
	{
		int index1 = j * width * channels;
		int index2 = (height - 1 - j) * width * channels;

		for (int i = width * channels; i > 0; --i)
		{
			unsigned char tmp = image[index1];
			image[index1] = image[index2];
			image[index2] = tmp;
			++index1;
			++index2;
		}
	}
}