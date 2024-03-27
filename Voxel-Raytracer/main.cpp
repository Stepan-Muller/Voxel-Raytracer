#include<iostream>

#include"glad/glad.h"
#include"GLFW/glfw3.h"

#include "util.c"

/* Konstanty hráèe */
#define MOVE_SPEED 20
#define TURN_SPEED 0.002
/* Promìnné hráèe */
float playerPos[3] = { -10, 0, 0 }, playerDelta[3] = { 0, 0, 0 }, lastMouse[2], playerAngle = PI / 2 * 3, cameraAngle = 0, deltaTime;
bool menu = false, resetMouse = true;

/* Konstanty kamery */
#define RENDER_DISTANCE 160
#define DEFAULT_WIDTH 500
#define DEFAULT_HEIGHT 500
/* Promìnné kamery */
float fov = 60 * PI / 180.0;
int screenWidth = DEFAULT_WIDTH;
int screenHeight = DEFAULT_HEIGHT;

/* Konstanty mapy */
#define GRID_WIDTH 10
#define GRID_HEIGHT 10
#define GRID_DEPTH 10
/* Promìnné mapy */
unsigned int voxel_grid[GRID_WIDTH * GRID_HEIGHT * GRID_DEPTH];

/* Verze OpenGL (4.6) */
const unsigned short OPENGL_MAJOR_VERSION = 4;
const unsigned short OPENGL_MINOR_VERSION = 6;

bool vSync = true;

/* Trojuhleníky pøes celou obrazovku, na nì se renderuje raytracing */
GLuint screenTex;

GLfloat vertices[] =
{
    -1.0f, -1.0f , 0.0f, 0.0f, 0.0f,
    -1.0f,  1.0f , 0.0f, 0.0f, 1.0f,
     1.0f,  1.0f , 0.0f, 1.0f, 1.0f,
     1.0f, -1.0f , 0.0f, 1.0f, 0.0f,
};

GLuint indices[] =
{
    0, 2, 1,
    0, 3, 2
};

/* OpenGL shadery */

const char* screenVertexShaderSource = R"(#version 460 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 uvs;
out vec2 UVs;
void main()
{
	gl_Position = vec4(pos.x, pos.y, pos.z, 1.0);
	UVs = uvs;
})";

const char* screenFragmentShaderSource = R"(#version 460 core
out vec4 FragColor;
uniform sampler2D screen;
in vec2 UVs;
void main()
{
	FragColor = texture(screen, UVs);
})";

const char* screenComputeShaderSource = R"(#version 460 core
layout(local_size_x = 8, local_size_y = 4) in;
layout(rgba32f, binding = 0) uniform image2D screen;

uniform int renderDist;

uniform float playerAngle;
uniform float cameraAngle;
uniform float fov;

uniform vec3 playerPos;

void main()
{	
	/* Konstanty mapy */
	#define GRID_WIDTH 10
	#define GRID_HEIGHT 10
	#define GRID_DEPTH 10
	/* Promìnné mapy */
	unsigned int voxel_grid[GRID_WIDTH * GRID_HEIGHT * GRID_DEPTH];	

	for (unsigned int i = 0; i < GRID_WIDTH; i++) {
        for (unsigned int j = 0; j < GRID_HEIGHT; j++) {
            for (unsigned int k = 0; k < GRID_DEPTH; k++) {
                voxel_grid[i + j * GRID_WIDTH + k * GRID_WIDTH * GRID_HEIGHT] = (i + j * GRID_WIDTH + k * GRID_WIDTH * GRID_HEIGHT) % 3;
            }
        }
    }
	
	vec3 cameraDir = vec3(cos(cameraAngle) * -sin(playerAngle), sin(cameraAngle), cos(cameraAngle) * -cos(playerAngle));
	vec3 cameraUp = vec3(sin(cameraAngle) * sin(playerAngle), cos(cameraAngle), sin(cameraAngle) * cos(playerAngle));

	ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);
	ivec2 dims = imageSize(screen);

    float u = (2.0 * pixel_coords.x / dims.x - 1.0) * tan(fov / 2.0);
    float v = -(2.0 * pixel_coords.y / dims.y - 1.0) * tan(fov / 2.0) * dims.y / dims.x;
    vec3 rayDir = normalize(cameraDir - u * cross(cameraDir, cameraUp) + v * cameraUp);

    unsigned int i = 0;
    ivec3 voxelPos = ivec3(ceil(playerPos));
    ivec3 step = ivec3(sign(rayDir));

    vec3 tMax = vec3(1.0 / 0.0);
	vec3 tDelta = vec3(1.0 / 0.0);
    
	if (step.x != 0.0)
    {
        tMax.x = (voxelPos.x + step.x * 0.5 - 0.5 - playerPos.x) / rayDir.x;
        tDelta.x = abs(1.0 / rayDir.x);
    }

    if (step.y != 0.0)
    {
        tMax.y = (voxelPos.y + step.y * 0.5 - 0.5 - playerPos.y) / rayDir.y;
        tDelta.y = abs(1.0 / rayDir.y);
    }

    if (step.z != 0.0)
    {
        tMax.z = (voxelPos.z + step.z * 0.5 - 0.5 - playerPos.z) / rayDir.z;
        tDelta.z = abs(1.0 / rayDir.z);
    }

    vec3 color = vec3(0, 0.5, 0);

    while (i < renderDist) {
        if (0 <= voxelPos.x && voxelPos.x < GRID_WIDTH && 0 <= voxelPos.y && voxelPos.y < GRID_HEIGHT && 0 <= voxelPos.z && voxelPos.z < GRID_DEPTH) {
            if (voxel_grid[voxelPos.x + voxelPos.y * GRID_WIDTH + voxelPos.z * GRID_WIDTH * GRID_HEIGHT] == 1) {
                color = vec3(0.5, 0.0, 0.0);
                break;
            }
            if (voxel_grid[voxelPos.x + voxelPos.y * GRID_WIDTH + voxelPos.z * GRID_WIDTH * GRID_HEIGHT] == 2) {
                color = vec3(1.0, 0.0, 1.0);
                break;
            }
        }

        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                voxelPos.x += step.x;
                tMax.x += tDelta.x;
            }
            else {
                voxelPos.z += step.z;
                tMax.z += tDelta.z;
            }
        }
        else {
            if (tMax.y < tMax.z) {
                voxelPos.y += step.y;
                tMax.y += tDelta.y;
            }
            else {
                voxelPos.z += step.z;
                tMax.z += tDelta.z;
            }
        }

        i++;
    }
	
	imageStore(screen, pixel_coords, vec4(color, 1.0));
})";

/* Kdyz je zmacknuta klavesa */
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // Uvolneni mysi
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        menu = !menu;
        if (menu)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        else
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        resetMouse = true;
    }
}

/* Pohyb hrace pomoci mysi */
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    // pokud je v menu, nehybat s kamerou
    if (menu)
        return;

    if (resetMouse)
    {
        lastMouse[0] = xpos;
        lastMouse[1] = ypos;
        resetMouse = false;
    }

    // Osa x - otaceni hrace
    playerAngle = capRad(playerAngle + (xpos - lastMouse[0]) * TURN_SPEED);
    lastMouse[0] = xpos;

    playerDelta[0] = sin(playerAngle);
    playerDelta[2] = cos(playerAngle);

    // Osa y - otaceni kamery
    cameraAngle += (ypos - lastMouse[1]) * TURN_SPEED;
    lastMouse[1] = ypos;

    if (cameraAngle > PI / 2)
        cameraAngle = PI / 2;
    else if (cameraAngle < -PI / 2)
        cameraAngle = -PI / 2;
}

/* Zmena velikosti okna */
void window_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    glBindTexture(GL_TEXTURE_2D, screenTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);

    screenWidth = width;
    screenHeight = height;
}

/* Pohyb hrace pomoci klaves WSAD */
void movePlayer(GLFWwindow* window)
{
    // pokud je v menu, nehybat s hracem
    if (menu)
        return;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        playerPos[0] -= playerDelta[0] * deltaTime * MOVE_SPEED;
        playerPos[2] -= playerDelta[2] * deltaTime * MOVE_SPEED;
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        playerPos[0] += playerDelta[0] * deltaTime * MOVE_SPEED;
        playerPos[2] += playerDelta[2] * deltaTime * MOVE_SPEED;
    }

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        playerPos[0] += playerDelta[2] * deltaTime * MOVE_SPEED;
        playerPos[2] -= playerDelta[0] * deltaTime * MOVE_SPEED;
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        playerPos[0] -= playerDelta[2] * deltaTime * MOVE_SPEED;
        playerPos[2] += playerDelta[0] * deltaTime * MOVE_SPEED;
    }

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
    {
        playerPos[1] -= deltaTime * MOVE_SPEED;
    }

    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
    {
        playerPos[1] += deltaTime * MOVE_SPEED;
    }
}

int main()
{
    glfwInit();

    // nastavení verze OpenGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_MAJOR_VERSION);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_MINOR_VERSION);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // tvorba okna
    GLFWwindow* window = glfwCreateWindow(DEFAULT_WIDTH, DEFAULT_HEIGHT, "OpenGL Compute Shaders", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create the GLFW window\n";
        glfwTerminate();
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(vSync);

    // nastaveni inputù + zmìny velikosti okna
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowSizeCallback(window, window_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize OpenGL context" << std::endl;
    }
    glViewport(0, 0, DEFAULT_WIDTH, DEFAULT_HEIGHT);

    /*
    VAO - Vertex Array Object
    VBO - Vertex Buffer Object
    EBO - Element Buffer Object
    */
    GLuint VAO, VBO, EBO;
    glCreateVertexArrays(1, &VAO);
    glCreateBuffers(1, &VBO);
    glCreateBuffers(1, &EBO);

    // nabinduje geometrii do bufferù
    glNamedBufferData(VBO, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glNamedBufferData(EBO, sizeof(indices), indices, GL_STATIC_DRAW);

    // nastavení pozice
    glEnableVertexArrayAttrib(VAO, 0);
    glVertexArrayAttribBinding(VAO, 0, 0);
    glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, 0);

    // nastavení souøadnic textury
    glEnableVertexArrayAttrib(VAO, 1);
    glVertexArrayAttribBinding(VAO, 1, 0);
    glVertexArrayAttribFormat(VAO, 1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat));

    // nabinduje VBO a EBO na VAO
    glVertexArrayVertexBuffer(VAO, 0, VBO, 0, 5 * sizeof(GLfloat));
    glVertexArrayElementBuffer(VAO, EBO);

    // nastevení textury
    glCreateTextures(GL_TEXTURE_2D, 1, &screenTex);
    glTextureParameteri(screenTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(screenTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(screenTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(screenTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, screenTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, DEFAULT_WIDTH, DEFAULT_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
    glBindImageTexture(0, screenTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    // shadery kromì compute
    GLuint screenVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(screenVertexShader, 1, &screenVertexShaderSource, NULL);
    glCompileShader(screenVertexShader);
    GLuint screenFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(screenFragmentShader, 1, &screenFragmentShaderSource, NULL);
    glCompileShader(screenFragmentShader);

    GLuint screenShaderProgram = glCreateProgram();
    glAttachShader(screenShaderProgram, screenVertexShader);
    glAttachShader(screenShaderProgram, screenFragmentShader);
    glLinkProgram(screenShaderProgram);

    glDeleteShader(screenVertexShader);
    glDeleteShader(screenFragmentShader);

    // compute shader
    GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(computeShader, 1, &screenComputeShaderSource, NULL);
    glCompileShader(computeShader);

    GLuint computeProgram = glCreateProgram();
    glAttachShader(computeProgram, computeShader);
    glLinkProgram(computeProgram);

    // priprava na delta time
    float lastTime = glfwGetTime();

    /* Main game loop */
    while (!glfwWindowShouldClose(window))
    {
        // vypocet delta time
        deltaTime = glfwGetTime() - lastTime;
        lastTime = glfwGetTime();

        // pohyb hrace
        movePlayer(window);

        // inputy + zmìna velikosti obrazovky
        glfwPollEvents();

        // spusteni compute shaderu
        glUseProgram(computeProgram);
        // parametry compute shaderu
        glUniform1i(glGetUniformLocation(computeProgram, "renderDist"), RENDER_DISTANCE);
        glUniform1f(glGetUniformLocation(computeProgram, "playerAngle"), playerAngle);
        glUniform1f(glGetUniformLocation(computeProgram, "cameraAngle"), cameraAngle);
        glUniform1f(glGetUniformLocation(computeProgram, "fov"), fov);
        glUniform3f(glGetUniformLocation(computeProgram, "playerPos"), playerPos[0], playerPos[1], playerPos[2]);
        // velikost
        glDispatchCompute(screenWidth / 8 + 1, screenHeight / 4 + 1, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);

        // screen shader
        glUseProgram(screenShaderProgram);
        glBindTextureUnit(0, screenTex);
        glUniform1i(glGetUniformLocation(screenShaderProgram, "screen"), 0);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(indices[0]), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}