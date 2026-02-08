#pragma once
#include <glm/glm.hpp>

float getFloorHeight(float z);
void clampCameraRoom(glm::vec3& pos);

void drawScene(
    unsigned int cubeVAO,
    unsigned int shader,
    unsigned int modelLoc,
    unsigned int seatFreeTex,
    unsigned int seatReservedTex,
    unsigned int seatSoldTex,
    int useTexLoc
);
