#include "Scene.h" 
#include <cmath>
#include "People.h"
#include "Globals.h"
#include <algorithm>
#include <cstdlib>

void spawnPeople()
{
    people.clear();

    std::vector<int> availableSeats;
    for (int i = 0; i < (int)seatStatus.size(); i++)
    {
        if (seatStatus[i] == SeatStatus::RESERVED ||
            seatStatus[i] == SeatStatus::SOLD)
        {
            availableSeats.push_back(i);
        }
    }

    if (availableSeats.empty())
        return;

    int maxPeople = (int)availableSeats.size();
    int count = 1 + rand() % maxPeople;

    for (int i = 0; i < count; i++)
    {
        Person p;

        p.pos = glm::vec3(
            doorPos.x - 0.15f,
            0.0f,
            doorPos.z - 0.3f - i * 0.4f
        );

        int idx = rand() % availableSeats.size();
        int seatIndex = availableSeats[idx];
        availableSeats.erase(availableSeats.begin() + idx);

        p.targetRow = seatIndex / seatsPerRow;
        p.targetSeat = seatIndex % seatsPerRow;
        p.state = ENTERING;
        p.modelIndex = rand() % humanModels.size();

        people.push_back(p);
    }
}

bool allPeopleSeated()
{
    for (auto& p : people)
        if (p.state != SEATED)
            return false;

    return true;
}


void updatePeople(float dt)
{
    for (auto& p : people)
    {
        float targetZ = rowsZStart - p.targetRow * stepRun;
        float targetY = floorY0 + p.targetRow * stepRise;

        float seatX = xStart + p.targetSeat * seatSpacing;

        switch (p.state)
        {
        case ENTERING:
            p.pos.z -= personSpeed * dt;
            if (p.pos.z < doorPos.z - 1.2f)
                p.state = TURNING;
            break;

        case TURNING:
            p.state = CLIMBING;
            break;

        case CLIMBING:
        {
            if (p.pos.z > targetZ)
                p.pos.z -= personSpeed * dt;

            p.pos.y = getFloorHeight(p.pos.z);

            if (p.pos.z <= targetZ + 0.05f)
                p.state = WALKING_ROW;
            break;
        }

        case WALKING_ROW:
            if (fabs(p.pos.x - seatX) > 0.05f)
            {
                float dir = (seatX > p.pos.x) ? 1.0f : -1.0f;
                p.pos.x += dir * personSpeed * dt;
            }
            else
            {
                p.state = SEATED;
            }
            break;

        case SEATED:
            break;

        case EXIT_ROW:
        {
            float targetX =
                (p.pos.x < 0.0f)
                ? (-roomWidth * 0.5f + stairInset + stairWidth * 0.5f)
                : (roomWidth * 0.5f - stairInset - stairWidth * 0.5f);

            if (fabs(p.pos.x - targetX) > 0.05f)
            {
                float dir = (targetX > p.pos.x) ? 1.0f : -1.0f;
                p.pos.x += dir * personSpeed * dt;
            }
            else
            {
                p.state = DESCENDING;
            }
            break;
        }

        case DESCENDING:
            p.pos.z += personSpeed * dt;
            p.pos.y = getFloorHeight(p.pos.z);

            if (p.pos.z >= rowsZStart + 0.3f)
                p.state = EXITING_DOOR;
            break;

        case EXITING_DOOR:
            if (fabs(p.pos.x - doorPos.x) > 0.05f)
            {
                float dir = (doorPos.x > p.pos.x) ? 1.0f : -1.0f;
                p.pos.x += dir * personSpeed * dt;
            }
            else
            {
                p.pos.z += personSpeed * dt;
            }
            break;
        }
    }

    people.erase(
        std::remove_if(
            people.begin(),
            people.end(),
            [&](const Person& p)
            {
                return p.pos.z > exitZ;
            }),
        people.end()
    );
}

void resetPeople()
{
    people.clear();
}
