#include "Globals.h"

// ===== STORAGE =====
std::vector<Seat> seats;
std::vector<SeatStatus> seatStatus;
std::vector<Person> people;

// room
float roomWidth = 14.0f;
float roomDepth = 20.0f;
float roomHeight = 7.0f;

float stepRise = 0.16f;
float stepRun = 0.90f;

int seatRows = 14;
int seatsPerRow = 12;

// camera
glm::vec3 cameraPos(0.0f, 1.6f, 0.0f);
glm::vec3 cameraFront(0.0f, 0.0f, -1.0f);
float fov = 45.0f;
// === DOOR ===
glm::vec3 doorPos(
    roomWidth * 0.5f - 0.04f,
    1.05f,
    roomDepth * 0.5f - 1.0f
);

float exitZ = roomDepth * 0.5f + 1.0f;

// === FLOOR / ROWS ===
float floorY0 = 0.0f;
float rowsZStart = (roomDepth * 0.5f - 7.5f);

// === SEATS LAYOUT ===
float seatSpacing = 0.0f; 
float xStart = 0.0f;

//float seatSpacing = usableWidth / seatsPerRow;
//float xStart = -usableWidth * 0.5f + seatSpacing * 0.5f;

// === PEOPLE ===
float personSpeed = 1.2f;

float stairWidth = 1.0f;
float stairInset = 0.05f;
float eyeHeight = 1.60f;
float seatWidth = 0.55f;
float seatDepth = 0.55f;
float seatHeight = 0.16f;

float backHeight = 0.5f;
float backTilt = 0.03f;

float seatBackHeight = 0.5f;
float seatBackDepth = 0.08f;