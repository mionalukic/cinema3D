#include <GL/glew.h>     
#include <GLFW/glfw3.h>

#include "Camera.h"
#include "Globals.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// ===== INTERNA STANJA KAMERE =====
static bool firstMouse = true;
static float lastX = 0.0f;
static float lastY = 0.0f;

static float yaw = -90.0f;
static float pitch = 0.0f;

// ===== CALLBACK: MOUSE =====
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;

    lastX = (float)xpos;
    lastY = (float)ypos;

    float sensitivity = 0.06f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    cameraFront = glm::normalize(direction);
}

// ===== CALLBACK: SCROLL =====
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    fov -= (float)yoffset;
    if (fov < 1.0f)  fov = 1.0f;
    if (fov > 45.0f) fov = 45.0f;
}

// ===== VIEW MATRICA =====
glm::mat4 getViewMatrix()
{
    glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);
    return glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
}
