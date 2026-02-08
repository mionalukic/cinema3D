#include <glm/gtc/type_ptr.hpp>
#include "Scene.h"
#include "Globals.h"
#include "Util.h"   // drawCube
#include <GL/glew.h>


float getFloorHeight(float z)
{
    if (z >= rowsZStart)
        return floorY0;

    float y = floorY0;
    float zStart = rowsZStart;

    for (int i = 0; i < seatRows; i++)
    {
        float zEnd = zStart - stepRun;

        if (z <= zStart && z >= zEnd)
            return y;

        zStart = zEnd;
        y += stepRise;
    }

    return floorY0 + seatRows * stepRise;
}


void clampCameraRoom(glm::vec3& pos)
{
    float margin = 0.3f;

    pos.x = glm::clamp(
        pos.x,
        -roomWidth * 0.5f + margin,
        roomWidth * 0.5f - margin
    );

    pos.z = glm::clamp(
        pos.z,
        -roomDepth * 0.5f + margin,
        roomDepth * 0.5f - margin
    );

    float floorY = getFloorHeight(pos.z);
    float minY = floorY + eyeHeight * 0.3f;
    float maxY = roomHeight - 0.2f;

    pos.y = glm::clamp(pos.y, minY, maxY);
}


void drawScene(
    unsigned int cubeVAO,
    unsigned int shader,
    unsigned int modelLoc,
    unsigned int seatFreeTex,
    unsigned int seatReservedTex,
    unsigned int seatSoldTex,
    int useTexLoc
)
{
    float platformWidth = roomWidth - 2.0f * (stairWidth + stairInset + 0.15f);
    float usableWidth = platformWidth - 0.4f;

    seatSpacing = usableWidth / seatsPerRow;
    xStart = -usableWidth * 0.5f + seatSpacing * 0.5f;

    float zCursor = rowsZStart;
    float y = floorY0;

    seats.clear();

    for (int i = 0; i < seatRows; i++)
    {
        float zCenter = zCursor - stepRun * 0.5f;

        // =====================================================
        // PLATFORMA / STEPENIK  (BEZ TEKSTURE)
        // =====================================================
        glUniform1i(useTexLoc, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        drawCube(
            cubeVAO, shader, modelLoc,
            glm::vec3(0.0f, y + stepRise * 0.5f, zCenter),
            glm::vec3(roomWidth, stepRise, stepRun)
        );

        // =====================================================
        // SEDIŠTA (SA TEKSTUROM)
        // =====================================================
        glUniform1i(useTexLoc, 1);

        for (int s = 0; s < seatsPerRow; s++)
        {
            int seatIndex = i * seatsPerRow + s;
            float xSeat = xStart + s * seatSpacing;

            GLuint tex = seatFreeTex;
            if (seatStatus[seatIndex] == SeatStatus::RESERVED)
                tex = seatReservedTex;
            else if (seatStatus[seatIndex] == SeatStatus::SOLD)
                tex = seatSoldTex;

            glBindTexture(GL_TEXTURE_2D, tex);

            // ---------------------------
            // SEDALNI DEO
            // ---------------------------
            drawCube(
                cubeVAO, shader, modelLoc,
                glm::vec3(
                    xSeat,
                    y + stepRise + 0.04f + seatHeight * 0.5f,
                    zCenter + stepRun * 0.1f
                ),
                glm::vec3(seatWidth, seatHeight, seatDepth)
            );

            // ---------------------------
            // NASLON STOLICE
            // ---------------------------
            drawCube(
                cubeVAO, shader, modelLoc,
                glm::vec3(
                    xSeat,
                    y + stepRise + 0.04f + seatHeight + seatBackHeight * 0.5f,
                    zCenter - seatDepth * 0.4f
                ),
                glm::vec3(seatWidth * 0.95f, seatBackHeight, seatBackDepth)
            );

            // ---------------------------
            // COLLISION / RAYCAST BOX
            // ---------------------------
            Seat seat;
            seat.center = glm::vec3(
                xSeat,
                y + stepRise + 0.04f + seatHeight * 0.5f,
                zCenter + stepRun * 0.1f
            );
            seat.halfSize = glm::vec3(
                seatWidth * 0.5f,
                seatHeight * 0.5f,
                seatDepth * 0.5f
            );

            seats.push_back(seat);
        }

        zCursor -= stepRun;
        y += stepRise;
    }

    // =====================================================
    // RESET STATE (DA NIŠTA DALJE NE NASLEDI TEKSTURU)
    // =====================================================
    glBindTexture(GL_TEXTURE_2D, 0);
    glUniform1i(useTexLoc, 0);
}

