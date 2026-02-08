#pragma once

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

// GLFW callback-i
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

// view matrica kamere
glm::mat4 getViewMatrix();
