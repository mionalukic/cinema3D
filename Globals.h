#pragma once
#include <glm/glm.hpp>
#include <vector>

class Model;

extern std::vector<Model*> humanModels;
struct Seat {
    glm::vec3 center;
    glm::vec3 halfSize;
};

enum class SeatStatus { FREE, RESERVED, SOLD };

enum PersonState {
    ENTERING, TURNING, CLIMBING, WALKING_ROW,
    SEATED, EXIT_ROW, DESCENDING, EXITING_DOOR
};

struct Person {
    glm::vec3 pos;
    int targetRow;
    int targetSeat;
    PersonState state;
    int modelIndex;
};

// === GLOBAL STATE ===
extern std::vector<Seat> seats;
extern std::vector<SeatStatus> seatStatus;
extern std::vector<Person> people;

// room
extern float roomWidth, roomDepth, roomHeight;
extern float stepRise, stepRun;
extern int seatRows, seatsPerRow;

// camera
extern glm::vec3 cameraPos;
extern glm::vec3 cameraFront;
extern float fov;

// === DOOR ===
extern glm::vec3 doorPos;
extern float exitZ;

// === FLOOR / ROWS ===
extern float floorY0;
extern float rowsZStart;

// === SEATS LAYOUT ===
extern float seatSpacing;
extern float xStart;

// === PEOPLE ===
extern float personSpeed;

// === STAIRS ===
extern float stairWidth;
extern float stairInset;

extern float eyeHeight;

extern float seatWidth;
extern float seatHeight;
extern float seatDepth;
extern float backHeight;
extern float backTilt;

extern float seatBackHeight;
extern float seatBackDepth;

