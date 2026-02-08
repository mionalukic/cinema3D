#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
unsigned int createShader(const char* vsSource, const char* fsSource);
unsigned loadImageToTexture(const char* filePath);
GLFWcursor* loadImageToCursor(const char* filePath);


void drawCube(
    unsigned int cubeVAO,
    unsigned int shader,
    unsigned int modelLoc,
    glm::vec3 position,
    glm::vec3 scale
);
