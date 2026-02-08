
 #include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>  
#include <ctime>  

#include <GL/glew.h>
#include <GLFW/glfw3.h>

//GLM biblioteke
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Util.h"

#include "shader.hpp"
#include "model.hpp"

#include <assimp/Importer.hpp>

struct Seat {
    glm::vec3 center;
    glm::vec3 halfSize;
};

std::vector<Seat> seats;
int selectedSeat = -1;

bool useTex = false;
bool transparent = false;

float roomWidth = 14.0f;  
float roomDepth = 20.0f;   
float roomHeight = 7.0f;  
float wallThick = 0.2f;

int seatRows = 14;        // broj redova
float stepRise = 0.16f;  // blago penjanje (16 cm)
float stepRun = 0.90f;  // dubina reda sa sedištima


float floorY0 = 0.0f;   // početna visina 
glm::vec3 cameraPos;

float stairWidth = 1.0f;      // bočno stepenište
float stairInset = 0.05f;     // mali razmak od zida
float platformHeight = stepRise;     // debljina platforme
float platformInset = stairWidth + stairInset + 0.15f;
float platformHeightExtra = 0.0f;

int seatsPerRow = 12;

float seatWidth = 0.55f;
float seatDepth = 0.55f;
float seatHeight = 0.16f;

float backHeight = 0.5f;
float backTilt = 0.03f; // mali nagib unazad
float platformWidth = roomWidth - 2.0f * (stairWidth + stairInset + 0.15f);

float usableWidth = platformWidth - 0.4f;
float seatSpacing = usableWidth / seatsPerRow;
float xStart = -usableWidth * 0.5f + seatSpacing * 0.5f;

float frontClear = 7.5f;   // prazan prostor kod ekrana (parter)
float backClear = 1.0f;   // margina kod zadnjeg zida
float rowsZStart = (roomDepth * 0.5f - frontClear);  // start kod +Z, malo unazad od zida


float screenWidth = roomWidth * 0.7f;   // ~70% zida
float screenHeight = roomHeight * 0.45f;
float screenZ = roomDepth * 0.5f -0.2f; // malo ispred zida
float screenY = roomHeight * 0.55f ;       // iznad partera


float lastFrame = (float)glfwGetTime();

// finiji osećaj
float baseSpeed = 2.2f;     // m/s (realističnije)
float sprintMul = 2.0f;     // SHIFT
float eyeHeight = 1.60f;    // visina očiju
float stepSmooth = 10.0f;   // veće = brže prati pod (8–14 je ok)

// ===== VRATA =====
float doorWidth = 1.0f;   // 90–100 cm realno
float doorHeight = 2.1f;
float doorThick = 0.08f;

float doorAngle = 0.0f;        // 0 = zatvorena
float doorTargetAngle = 0.0f;  // 0 ili 90
float doorSpeed = 120.0f;      // deg/sec
float exitZ = roomDepth * 0.5f + 1.0f;

glm::vec3 doorPos = glm::vec3(
    roomWidth * 0.5f - doorThick * 0.5f,  // DESNI ZID
    doorHeight * 0.5f,                    // stoje na podu
    roomDepth * 0.5f - 1.0f               // napred
);


enum PersonState {
    ENTERING,    // ulazi kroz vrata
    TURNING,     // okreće se ka stepeništu
    CLIMBING,    // penje se uz stepenike
    WALKING_ROW, // ide po redu ka sedištu
    SEATED,
    EXIT_ROW,        // ide iz sedišta ka prolazu
    DESCENDING,      // niz stepenice
    EXITING_DOOR     // ka vratima i napolje
};

struct Person {
    glm::vec3 pos;
    int targetRow;
    int targetSeat;
    PersonState state;

};

enum class CinemaState {
    RESERVATION,
    SEATING,
    PLAYING,
    EXITING
};

CinemaState cinemaState = CinemaState::RESERVATION;

glm::vec3 hallLightPos(0.0f, roomHeight - 0.2f, 0.0f);
glm::vec3 hallLightColor(1.0f, 1.0f, 0.95f);
float hallLightIntensity = 1.3f;



enum class SeatStatus { FREE, RESERVED, SOLD };
std::vector<SeatStatus> seatStatus;

int hoveredSeat = -1;

unsigned foxTexture = loadImageToTexture("res/models/fox/fox_texture.png");

Model foxModel("res/models/fox/low-poly-fox.obj");
glm::vec3 screenLightDir(0.0f, 0.0f, -1.0f);
float hallLightIntensityPlaying = 0.0f; // mrak u sali
float screenLightIntensity = 1.8f;       // jako svetlo sa platna


unsigned int preprocessTexture(const char* filepath) {
    unsigned int texture = loadImageToTexture(filepath); // Učitavanje teksture
    glBindTexture(GL_TEXTURE_2D, texture); // Vezujemo se za teksturu kako bismo je podesili

    // Generisanje mipmapa - predefinisani različiti formati za lakše skaliranje po potrebi (npr. da postoji 32 x 32 verzija slike, ali i 16 x 16, 256 x 256...)
    glGenerateMipmap(GL_TEXTURE_2D);

    // Podešavanje strategija za wrap-ovanje - šta da radi kada se dimenzije teksture i poligona ne poklapaju
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // S - tekseli po x-osi
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // T - tekseli po y-osi

    // Podešavanje algoritma za smanjivanje i povećavanje rezolucije: nearest - bira najbliži piksel, linear - usrednjava okolne piksele
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return texture;
}

bool firstMouse = true;
float lastX, lastY = 500.0f; // Ekran nam je 1000 x 1000 piksela, kursor je inicijalno na sredini
float yaw = -90.0f, pitch = 0.0f; // yaw -90: kamera gleda u pravcu z ose; pitch = 0: kamera gleda vodoravno
glm::vec3 cameraFront = glm::vec3(0.0, 0.0, -1.0); // at-vektor je inicijalno u pravcu z ose

bool rayIntersectsAABB(
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& minB,
    const glm::vec3& maxB
) {
    float tMin = (minB.x - rayOrigin.x) / rayDir.x;
    float tMax = (maxB.x - rayOrigin.x) / rayDir.x;
    if (tMin > tMax) std::swap(tMin, tMax);

    float tyMin = (minB.y - rayOrigin.y) / rayDir.y;
    float tyMax = (maxB.y - rayOrigin.y) / rayDir.y;
    if (tyMin > tyMax) std::swap(tyMin, tyMax);

    if ((tMin > tyMax) || (tyMin > tMax)) return false;

    tMin = std::max(tMin, tyMin);
    tMax = std::min(tMax, tyMax);

    float tzMin = (minB.z - rayOrigin.z) / rayDir.z;
    float tzMax = (maxB.z - rayOrigin.z) / rayDir.z;
    if (tzMin > tzMax) std::swap(tzMin, tzMax);

    if ((tMin > tzMax) || (tzMin > tMax)) return false;

    return true;
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

    float sensitivity = 0.06f;
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
float fov = 45.0f;

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    fov -= (float)yoffset;
    if (fov < 1.0f)
        fov = 1.0f;
    if (fov > 45.0f)
        fov = 45.0f;
}


void drawCube(unsigned int cubeVAO, unsigned int shader, unsigned int modelLoc,
    glm::vec3 position, glm::vec3 scale)
{
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    model = glm::scale(model, scale);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glBindVertexArray(cubeVAO);
    for (int i = 0; i < 6; i++) glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
}
float getFloorHeight(float z)
{
    // 0) PARTE R / ISPRED PRVOG REDA (kod platna)
    if (z >= rowsZStart)
        return floorY0;

    // 1) REDOVI (stepenasti pod)
    float y = floorY0;
    float zStart = rowsZStart;

    for (int i = 0; i < seatRows; i++)
    {
        float zEnd = zStart - stepRun; // ide unazad (-Z)

        if (z <= zStart && z >= zEnd)
            return y;

        zStart = zEnd;
        y += stepRise;
    }

    // 2) IZA POSLEDNJEG REDA (kod zadnjeg zida) – ostaje na najvišem nivou
    return floorY0 + seatRows * stepRise;
}


void clampCameraRoom(glm::vec3& pos)
{
    float margin = 0.3f;

    // zidovi
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

    // 🔑 POD + STEPENICE
    float floorY = getFloorHeight(pos.z);
    float minY = floorY + eyeHeight * 0.3f; // malo tolerancije
    float maxY = roomHeight - 0.2f;

    pos.y = glm::clamp(pos.y, minY, maxY);
}





std::vector<Person> people;

int maxPeople = 20;
float personSpeed = 1.2f;
float personWidth = 0.4f;
float personHeight = 1.7f;
float personDepth = 0.35f;

bool peopleSpawned = false;

int countTakenSeats()
{
    int count = 0;
    for (auto s : seatStatus)
        if (s == SeatStatus::RESERVED || s == SeatStatus::SOLD)
            count++;
    return count;
}

void spawnPeople()
{
    people.clear();

    // 1️⃣ Sva sedišta koja imaju kartu (RESERVED ili SOLD)
    std::vector<int> availableSeats;
    for (int i = 0; i < (int)seatStatus.size(); i++)
    {
        if (seatStatus[i] == SeatStatus::RESERVED ||
            seatStatus[i] == SeatStatus::SOLD)
        {
            availableSeats.push_back(i);
        }
    }

    // Nema nikoga ako nema kupljenih/rezervisanih mesta
    if (availableSeats.empty())
        return;

    // 2️⃣ Koliko ljudi ulazi (max = broj dostupnih sedišta)
    int maxPeople = (int)availableSeats.size();
    int count = 1 + rand() % maxPeople;

    // 3️⃣ Za svakog čoveka – uzmi JEDNO sedište i ukloni ga
    for (int i = 0; i < count; i++)
    {
        Person p;

        // start pozicija kod vrata (mali Z offset da se ne sudare)
        p.pos = glm::vec3(
            doorPos.x - 0.15f,
            0.0f,
            doorPos.z - 0.3f - i * 0.4f
        );

        // nasumično izaberi jedno slobodno sedište
        int idx = rand() % availableSeats.size();
        int seatIndex = availableSeats[idx];

        // ukloni sedište iz liste da ga niko drugi ne dobije
        availableSeats.erase(availableSeats.begin() + idx);

        // mapiranje indeksa → red / kolona
        p.targetRow = seatIndex / seatsPerRow;
        p.targetSeat = seatIndex % seatsPerRow;
        p.state = ENTERING;

        people.push_back(p);
    }
}

void resetCinema()
{
    // stanje
    cinemaState = CinemaState::RESERVATION;

    // sedišta → sva FREE (plava)
    for (auto& s : seatStatus)
        s = SeatStatus::FREE;

    // ljudi → niko unutra
    people.clear();

    // vrata → zatvorena
    doorTargetAngle = 0.0f;
    doorAngle = 0.0f;

    // svetlo → normalno
    peopleSpawned = false;
}


void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        glm::vec3 rayOrigin = cameraPos;
        glm::vec3 rayDir = glm::normalize(cameraFront);

        selectedSeat = -1;

        for (int i = 0; i < (int)seats.size(); i++)
        {
            glm::vec3 minB = seats[i].center - seats[i].halfSize;
            glm::vec3 maxB = seats[i].center + seats[i].halfSize;

            if (rayIntersectsAABB(rayOrigin, rayDir, minB, maxB))
            {
                selectedSeat = i;

                // toggle samo ako nije SOLD
                if (i >= 0 && i < (int)seatStatus.size())
                {
                    if (seatStatus[i] == SeatStatus::FREE) seatStatus[i] = SeatStatus::RESERVED;
                    else if (seatStatus[i] == SeatStatus::RESERVED) seatStatus[i] = SeatStatus::FREE;
                }
                break;
            }
        }
    }
}


bool sellNAdjacentFree(int N)
{
    // redovi: od poslednjeg ka prvom
    for (int row = seatRows - 1; row >= 0; row--)
    {
        // kolone: od desna ka levo
        for (int startCol = seatsPerRow - N; startCol >= 0; startCol--)
        {
            bool allFree = true;

            // proveri N susednih
            for (int k = 0; k < N; k++)
            {
                int idx = row * seatsPerRow + (startCol + k);
                if (seatStatus[idx] != SeatStatus::FREE)
                {
                    allFree = false;
                    break;
                }
            }

            // ako smo našli N susednih FREE
            if (allFree)
            {
                for (int k = 0; k < N; k++)
                {
                    int idx = row * seatsPerRow + (startCol + k);
                    seatStatus[idx] = SeatStatus::SOLD;
                }
                return true; // uspešna kupovina
            }
        }
    }

    return false; // nema mesta
}

bool allPeopleSeated()
{
    for (auto& p : people)
        if (p.state != SEATED)
            return false;
    return true;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS) return;

    // === TOGGLE VRATA ===
    if (key == GLFW_KEY_O)
    {
        doorTargetAngle = (doorTargetAngle == 0.0f) ? 90.0f : 0.0f;
    }

    // === KUPOVINA SEDIŠTA 1–9 ===
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9)
    {
        int n = key - GLFW_KEY_0; // 1..9
        sellNAdjacentFree(n);
    }

    if (key == GLFW_KEY_ENTER)
    {
        if (cinemaState == CinemaState::RESERVATION)
        {
            cinemaState = CinemaState::SEATING;
            doorTargetAngle = 90.0f;   // otvori vrata
            spawnPeople();
        }
    }
}



int main(void)
{
    if (!glfwInit())
    {
        std::cout << "GLFW Biblioteka se nije ucitala! :(\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    unsigned int wWidth = mode->width;
    unsigned int wHeight = mode->height;

    GLFWwindow* window = glfwCreateWindow(
        wWidth,
        wHeight,
        "Movie theater",
        monitor,   // ⬅ fullscreen
        NULL
    );

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);


    seatStatus.assign(seatRows * seatsPerRow, SeatStatus::FREE);

    if (window == NULL)
    {
        std::cout << "Prozor nije napravljen! :(\n";
        glfwTerminate();
        return 2;
    }

    glfwMakeContextCurrent(window);
    glViewport(0, 0, wWidth, wHeight);


    if (glewInit() != GLEW_OK)
    {
        std::cout << "GLEW nije mogao da se ucita! :'(\n";
        return 3;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++ PROMJENLJIVE I BAFERI +++++++++++++++++++++++++++++++++++++++++++++++++

    unsigned int unifiedShader = createShader("basic.vert", "basic.frag");
    glUseProgram(unifiedShader);
    GLint ambientStrengthLoc = glGetUniformLocation(unifiedShader, "ambientStrength");
    GLint useScreenFalloffLoc = glGetUniformLocation(unifiedShader, "useScreenFalloff");

    Shader modelShader("model.vert", "model.frag");


    glUniform1f(glGetUniformLocation(unifiedShader, "screenZ"), screenZ);

    glUniform1i(glGetUniformLocation(unifiedShader, "uTex"), 0);

    GLint lightDirLoc = glGetUniformLocation(unifiedShader, "lightDir");
    GLint lightColorLoc = glGetUniformLocation(unifiedShader, "lightColor");
    GLint lightIntensityLoc = glGetUniformLocation(unifiedShader, "lightIntensity");
    GLint lightEnabledLoc = glGetUniformLocation(unifiedShader, "lightEnabled");
    GLint emissiveLoc = glGetUniformLocation(unifiedShader, "emissive");
    GLint emissiveStrengthLoc = glGetUniformLocation(unifiedShader, "emissiveStrength");



    GLint uTexLoc = glGetUniformLocation(unifiedShader, "uTex");
    GLint useTexLoc = glGetUniformLocation(unifiedShader, "useTex");
    GLint transparentLoc = glGetUniformLocation(unifiedShader, "transparent");

    glUniform1i(uTexLoc, 0);            // sampler koristi TEXTURE0
    glUniform1i(useTexLoc, 0);          // default: bez teksture
    glUniform1i(transparentLoc, 0);     // default: ne providno

    float screenVertices[] = {
        -screenWidth / 2, screenY - screenHeight / 2, screenZ,  1,1,1,1,  0,0,  0,0,-1,
        -screenWidth / 2, screenY + screenHeight / 2, screenZ,  1,1,1,1,  0,1,  0,0,-1,
         screenWidth / 2, screenY + screenHeight / 2, screenZ,  1,1,1,1,  1,1,  0,0,-1,
         screenWidth / 2, screenY - screenHeight / 2, screenZ,  1,1,1,1,  1,0,  0,0,-1
    };



    unsigned int screenVAO, screenVBO;
    unsigned int stride = (3 + 4 + 2 + 3) * sizeof(float);

    glGenVertexArrays(1, &screenVAO);
    glGenBuffers(1, &screenVBO);

    glBindVertexArray(screenVAO);
    glBindBuffer(GL_ARRAY_BUFFER, screenVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screenVertices), screenVertices, GL_STATIC_DRAW);

    // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    // color
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // normal
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);


    float roomVertices[] =
    {

        // ==== PLAFON (y = H), normal (0,-1,0)
        -roomWidth / 2, roomHeight,  roomDepth / 2,  0.18f,0.18f,0.18f,1,  0,0,   0,-1,0,
         roomWidth / 2, roomHeight,  roomDepth / 2,  0.18f,0.18f,0.18f,1,  1,0,   0,-1,0,
         roomWidth / 2, roomHeight, -roomDepth / 2,  0.18f,0.18f,0.18f,1,  1,1,   0,-1,0,
        -roomWidth / 2, roomHeight, -roomDepth / 2,  0.18f,0.18f,0.18f,1,  0,1,   0,-1,0,

        // ==== ZADNJI ZID (z = -D/2), normal (0,0,1)
        -roomWidth / 2, 0, -roomDepth / 2,  0.22f,0.22f,0.22f,1,  0,0,   0,0,1,
         roomWidth / 2, 0, -roomDepth / 2,  0.22f,0.22f,0.22f,1,  1,0,   0,0,1,
         roomWidth / 2, roomHeight, -roomDepth / 2, 0.22f,0.22f,0.22f,1,  1,1,   0,0,1,
        -roomWidth / 2, roomHeight, -roomDepth / 2, 0.22f,0.22f,0.22f,1,  0,1,   0,0,1,

        // ==== PREDNJI ZID (z = +D/2), normal (0,0,-1)
         roomWidth / 2, 0, roomDepth / 2,  0.22f,0.22f,0.22f,1,  0,0,   0,0,-1,
        -roomWidth / 2, 0, roomDepth / 2,  0.22f,0.22f,0.22f,1,  1,0,   0,0,-1,
        -roomWidth / 2, roomHeight, roomDepth / 2, 0.22f,0.22f,0.22f,1,  1,1,   0,0,-1,
         roomWidth / 2, roomHeight, roomDepth / 2, 0.22f,0.22f,0.22f,1,  0,1,   0,0,-1,

         // ==== LEVI ZID (x = -W/2), normal (1,0,0)
         -roomWidth / 2, 0,  roomDepth / 2, 0.20f,0.20f,0.20f,1, 0,0,   1,0,0,
         -roomWidth / 2, 0, -roomDepth / 2, 0.20f,0.20f,0.20f,1, 1,0,   1,0,0,
         -roomWidth / 2, roomHeight, -roomDepth / 2,0.20f,0.20f,0.20f,1, 1,1,   1,0,0,
         -roomWidth / 2, roomHeight,  roomDepth / 2,0.20f,0.20f,0.20f,1, 0,1,   1,0,0,

         // ==== DESNI ZID (x = +W/2), normal (-1,0,0)
          roomWidth / 2, 0, -roomDepth / 2, 0.20f,0.20f,0.20f,1, 0,0,  -1,0,0,
          roomWidth / 2, 0,  roomDepth / 2, 0.20f,0.20f,0.20f,1, 1,0,  -1,0,0,
          roomWidth / 2, roomHeight,  roomDepth / 2,0.20f,0.20f,0.20f,1, 1,1,  -1,0,0,
          roomWidth / 2, roomHeight, -roomDepth / 2,0.20f,0.20f,0.20f,1, 0,1,  -1,0,0
    };

    float* vertices = roomVertices;


    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(roomVertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);



    // pos(3), col(4), uv(2), normal(3)  => stride = 12 floatova
    float cubeVertices[] = {
        // +Z (front)
        -0.5f,-0.5f, 0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   0,0,   0,0,1,
         0.5f,-0.5f, 0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   1,0,   0,0,1,
         0.5f, 0.5f, 0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   1,1,   0,0,1,
        -0.5f, 0.5f, 0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   0,1,   0,0,1,

        // -Z (back)
         0.5f,-0.5f,-0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   0,0,   0,0,-1,
        -0.5f,-0.5f,-0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   1,0,   0,0,-1,
        -0.5f, 0.5f,-0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   1,1,   0,0,-1,
         0.5f, 0.5f,-0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   0,1,   0,0,-1,

         // +X
          0.5f,-0.5f, 0.5f, 0.15f, 0.15f, 0.18f, 1.0f,   0,0,   1,0,0,
          0.5f,-0.5f,-0.5f, 0.15f, 0.15f, 0.18f, 1.0f,   1,0,   1,0,0,
          0.5f, 0.5f,-0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   1,1,   1,0,0,
          0.5f, 0.5f, 0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   0,1,   1,0,0,

          // -X
          -0.5f,-0.5f,-0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   0,0,  -1,0,0,
          -0.5f,-0.5f, 0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   1,0,  -1,0,0,
          -0.5f, 0.5f, 0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   1,1,  -1,0,0,
          -0.5f, 0.5f,-0.5f,  0.15f, 0.15f, 0.18f, 1.0f,   0,1,  -1,0,0,

          // +Y
          -0.5f, 0.5f, 0.5f,  0.20f, 0.20f, 0.23f, 1.0f,   0,0,   0,1,0,
           0.5f, 0.5f, 0.5f,  0.20f, 0.20f, 0.23f, 1.0f,   1,0,   0,1,0,
           0.5f, 0.5f,-0.5f,  0.20f, 0.20f, 0.23f, 1.0f,   1,1,   0,1,0,
          -0.5f, 0.5f,-0.5f,  0.20f, 0.20f, 0.23f, 1.0f,   0,1,   0,1,0,

          // -Y
          -0.5f,-0.5f,-0.5f,  0.4,0.4,0.4,1,   0,0,   0,-1,0,
           0.5f,-0.5f,-0.5f,  0.4,0.4,0.4,1,   1,0,   0,-1,0,
           0.5f,-0.5f, 0.5f,  0.4,0.4,0.4,1,   1,1,   0,-1,0,
          -0.5f,-0.5f, 0.5f,  0.4,0.4,0.4,1,   0,1,   0,-1,0
    };

    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);


    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++            UNIFORME            +++++++++++++++++++++++++++++++++++++++++++++++++

    glm::mat4 model = glm::mat4(1.0f); //Matrica transformacija - mat4(1.0f) generise jedinicnu matricu
    unsigned int modelLoc = glGetUniformLocation(unifiedShader, "uM");

    glm::mat4 view; //Matrica pogleda (kamere)
    cameraPos = glm::vec3(0.0f, 1.6f, 0.0f);
    glm::vec3 cameraUp = glm::vec3(0.0, 1.0, 0.0);

    view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp); // lookAt(Gdje je kamera, u sta kamera gleda, jedinicni vektor pozitivne Y ose svijeta  - ovo rotira kameru)
    unsigned int viewLoc = glGetUniformLocation(unifiedShader, "uV");


    glm::mat4 projectionP = glm::perspective(glm::radians(fov), (float)wWidth / (float)wHeight, 0.1f, 100.0f); //Matrica perspektivne projekcije (FOV, Aspect Ratio, prednja ravan, zadnja ravan)
    //glm::mat4 projectionO = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f); //Matrica ortogonalne projekcije (Lijeva, desna, donja, gornja, prednja i zadnja ravan)
    unsigned int projectionLoc = glGetUniformLocation(unifiedShader, "uP");


    // ++++++++++++++++++++++++++++++++++++++++++++++++++++++ RENDER LOOP - PETLJA ZA CRTANJE +++++++++++++++++++++++++++++++++++++++++++++++++
    glUseProgram(unifiedShader); //Slanje default vrijednosti uniformi
    glBindVertexArray(VAO);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model)); //(Adresa matrice, broj matrica koje saljemo, da li treba da se transponuju, pokazivac do matrica)
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    //glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projectionO));
    glBindVertexArray(VAO);



    glClearColor(0.5, 0.5, 0.5, 1.0);
    glCullFace(GL_BACK);//Biranje lica koje ce se eliminisati (tek nakon sto ukljucimo Face Culling)

    std::vector<GLuint> movieFrames;
    movieFrames.reserve(64);



    GLuint seatFreeTex = preprocessTexture("textures/seat_blue.jpg");
    GLuint seatReservedTex = preprocessTexture("textures/seat_yellow.jpg");
    GLuint seatSoldTex = preprocessTexture("textures/seat_red.jpg");


    for (int i = 0; i < 56; i++) {
        char path[256];
        sprintf_s(path, sizeof(path), "textures/%04d.jpg", i);
        std::cout << "Loading: " << path << std::endl;

        movieFrames.push_back(preprocessTexture(path));
    }

    double movieStart = glfwGetTime();
    float movieFps = 8.0f;

    float movieDuration = movieFrames.size() / movieFps;


    while (!glfwWindowShouldClose(window))
    {
        double startTime = glfwGetTime();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }


        float now = (float)glfwGetTime();
        float dt = now - lastFrame;
        lastFrame = now;

        for (auto& p : people)
        {
            float targetZ = rowsZStart - p.targetRow * stepRun;
            float targetY = floorY0 + p.targetRow * stepRise;

            float seatX =
                xStart + p.targetSeat * seatSpacing;

            switch (p.state)
            {
            case ENTERING:
                // 1–2 koraka unutra
                p.pos.z -= personSpeed * dt;
                if (p.pos.z < doorPos.z - 1.2f)
                    p.state = TURNING;
                break;

            case TURNING:
                // samo logička faza (može kasnije animacija)
                p.state = CLIMBING;
                break;

            case CLIMBING:
            {
                // kretanje ka redovima (po Z)
                if (p.pos.z > targetZ)
                    p.pos.z -= personSpeed * dt;

                // AUTOMATSKO PRAĆENJE STEPENICA
                float floorY = getFloorHeight(p.pos.z);
                p.pos.y = floorY;

                // kad stigne do svog reda
                if (p.pos.z <= targetZ + 0.05f)
                    p.state = WALKING_ROW;

                break;
            }


            case WALKING_ROW:
                // horizontalno ka svom sedištu
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
                // stoji mirno
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
            {
                if (p.pos.z < doorPos.z - 1.2f)
                    p.pos.z += personSpeed * dt;

                p.pos.y = getFloorHeight(p.pos.z);

                if (p.pos.z >= rowsZStart + 0.3f)
                    p.state = EXITING_DOOR;

                break;
            }

            case EXITING_DOOR:
            {
                // poravnaj se sa vratima po X
                if (fabs(p.pos.x - doorPos.x) > 0.05f)
                {
                    float dir = (doorPos.x > p.pos.x) ? 1.0f : -1.0f;
                    p.pos.x += dir * personSpeed * dt;
                }
                else
                {
                    // idi NAPOLJE (preko zida)
                    p.pos.z += personSpeed * dt;
                }
                break;
            }

            }
        }


        people.erase(
            std::remove_if(people.begin(), people.end(),
                [&](const Person& p)
                {
                    return p.pos.z > exitZ;
                }),
            people.end()
        );


        if (cinemaState == CinemaState::SEATING && allPeopleSeated())
        {
            doorTargetAngle = 0.0f;
            cinemaState = CinemaState::PLAYING;
            movieStart = glfwGetTime();
        }

        if (cinemaState == CinemaState::RESERVATION ||
            cinemaState == CinemaState::SEATING)
        {
            glUniform1i(lightEnabledLoc, 1);
            glUniform1i(useScreenFalloffLoc, 0);

            glUniform3fv(lightDirLoc, 1,
                glm::value_ptr(glm::vec3(0.0f, -1.0f, 0.0f)));

            glUniform3fv(lightColorLoc, 1,
                glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.97f)));

            glUniform1f(lightIntensityLoc, 1.3f);
            glUniform1f(ambientStrengthLoc, 0.35f);
        }
        else if (cinemaState == CinemaState::PLAYING)
        {
            glUniform1i(lightEnabledLoc, 1);
            glUniform1i(useScreenFalloffLoc, 1);   // ⬅️ KLJUČNO

            glUniform3fv(lightDirLoc, 1,
                glm::value_ptr(glm::normalize(glm::vec3(0.0f, -0.3f, -1.0f))));

            glUniform3fv(lightColorLoc, 1,
                glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));

            glUniform1f(lightIntensityLoc, screenLightIntensity);
            glUniform1f(ambientStrengthLoc, 0.02f);
        }


        if (cinemaState == CinemaState::PLAYING)
        {
            if (glfwGetTime() - movieStart > movieDuration)
            {
                // 1️⃣ PRELAZ STANJA
                cinemaState = CinemaState::EXITING;

                // 2️⃣ ODMAH UPALI OPŠTE SVETLO
                glUniform1i(lightEnabledLoc, 1);
                glUniform1i(useScreenFalloffLoc, 0);   // ⬅️ plafon, ne platno

                glUniform3fv(lightDirLoc, 1,
                    glm::value_ptr(glm::vec3(0.0f, -1.0f, 0.0f)));

                glUniform3fv(lightColorLoc, 1,
                    glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.97f)));

                glUniform1f(lightIntensityLoc, 1.3f);
                glUniform1f(ambientStrengthLoc, 0.35f);

                // 3️⃣ OTVORI VRATA + LJUDI USTAJU
                doorTargetAngle = 90.0f;
                for (auto& p : people)
                    p.state = EXIT_ROW;
            }
        }



        if (cinemaState == CinemaState::EXITING && people.empty())
        {
            resetCinema();
        }




        if (doorAngle < doorTargetAngle) doorAngle += doorSpeed * dt;
        if (doorAngle > doorTargetAngle) doorAngle -= doorSpeed * dt;
        doorAngle = glm::clamp(doorAngle, 0.0f, 90.0f);

        if (doorAngle > 80.0f && !peopleSpawned) {
            peopleSpawned = true;
        }
        if (doorAngle < 5.0f) {
            peopleSpawned = false;
        }
        float speed = baseSpeed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            speed *= sprintMul;

        glm::vec3 forward = glm::normalize(cameraFront);
        glm::vec3 right = glm::normalize(glm::cross(forward, cameraUp));

        glm::vec3 move(0.0f);

        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            move += forward;

        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            move -= forward;

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            move -= right;

        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            move += right;

        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
            if (doorTargetAngle == 0.0f)
                doorTargetAngle = 90.0f;
            else
                doorTargetAngle = 0.0f;
        }




        if (glm::length(move) > 0.0f)
            cameraPos += glm::normalize(move) * speed * dt;
        clampCameraRoom(cameraPos);


        float eyeHeight = 1.6f;  // visina očiju


        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Osvjezavamo i Z bafer i bafer boje

        glUseProgram(unifiedShader);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

        projectionP = glm::perspective(glm::radians(fov), (float)wWidth / (float)wHeight, 0.1f, 100.0f);
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projectionP));


        // ================= BIOSKOPSKA SALA (ZATVORENA) =================

        glBindVertexArray(VAO);
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        // 6 pravougaonika = 24 temena
        for (int i = 0; i < 6; i++)
            glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);


        //PRIKAZ OSVETLJENJA
        glUniform1i(useTexLoc, 0);
        drawCube(
            cubeVAO,
            unifiedShader,
            modelLoc,
            hallLightPos,
            glm::vec3(0.2f, 0.2f, 0.2f)
        );




        // ================= PLATNO =================
        glBindVertexArray(screenVAO);
        glUniform1i(emissiveLoc, 1);              // ⬅ platno je emissive
        glUniform1f(emissiveStrengthLoc, 1.8f);

        bool playing = (cinemaState == CinemaState::PLAYING);

        if (playing && !movieFrames.empty())
        {
            double t = glfwGetTime() - movieStart;
            int frameIndex = (int)(t * movieFps);

            if (frameIndex >= (int)movieFrames.size())
            {
                frameIndex = movieFrames.size() - 1; // ostani na poslednjem
            }

            glUniform1i(useTexLoc, 1);                 // koristi teksturu
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, movieFrames[frameIndex]);
        }
        else
        {
            glUniform1i(useTexLoc, 0);                 // NE koristi teksturu
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        glm::mat4 screenModel = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(screenModel));
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // 🚨 reset (higijena)
        glBindTexture(GL_TEXTURE_2D, 0);
        glUniform1i(useTexLoc, 0);

        glBindVertexArray(0);
        glUniform1i(emissiveLoc, 0);



        // ================= VRATA – DESNI ZID, OTVARANJE KA UNUTRA =================
        glBindVertexArray(cubeVAO);
        glUniform1i(useTexLoc, 0);
        glm::mat4 doorModel = glm::mat4(1.0f);

        // 1. pomeri do šarke (desni zid)
        doorModel = glm::translate(doorModel, doorPos);

        // 2. pivot na IVICU vrata (strana ka zidu)
        doorModel = glm::translate(
            doorModel,
            glm::vec3(0.0f, 0.0f, -doorWidth * 0.5f)
        );

        doorModel = glm::rotate(
            doorModel,
            glm::radians(-doorAngle),
            glm::vec3(0, 1, 0)
        );

        // 4. vrati pivot
        doorModel = glm::translate(
            doorModel,
            glm::vec3(0.0f, 0.0f, doorWidth * 0.5f)
        );

        // 5. skaliranje u vrata
        doorModel = glm::scale(
            doorModel,
            glm::vec3(doorThick, doorHeight, doorWidth)
        );

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(doorModel));
        glDrawArrays(GL_TRIANGLE_FAN, 0, 24);

        glBindVertexArray(0);

        glUniform1i(useTexLoc, 0);

        // ================= LJUDI (FOX MODELI) =================
        modelShader.use();

        // view + projection (mora za ovaj shader!)
        modelShader.setMat4("view", view);
        modelShader.setMat4("projection", projectionP);

        for (auto& p : people)
        {
            glm::mat4 m = glm::mat4(1.0f);

            // POZICIJA — koristi ISTU logiku kao kocka
            m = glm::translate(
                m,
                glm::vec3(
                    p.pos.x,
                    p.pos.y,   // ⬅ ne + personHeight
                    p.pos.z
                )
            );

            // SKALA — LISICA JE MANJA OD ČOVEKA
            m = glm::scale(m, glm::vec3(0.6f));

            // ROTACIJA (opciono, ali lepo)
            m = glm::rotate(m, glm::radians(180.0f), glm::vec3(0, 1, 0));

            modelShader.setMat4("model", m);
            foxModel.Draw(modelShader);
        }

        // ================= RESET STATE POSLE MODELA =================
        glUseProgram(unifiedShader);

        // reset tekstura
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);

        // reset uniformi koje utiču na boje
        glUniform1i(useTexLoc, 0);
        glUniform1i(transparentLoc, 0);
        glUniform1i(emissiveLoc, 0);

        // vrati standardno osvetljenje (bez uticaja model shadera)
        glUniform1i(lightEnabledLoc, 1);



            // ================= STEPENICE UZ ZIDOVE + PLATFORME =================
            //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            float xLeftWall = -roomWidth * 0.5f;
            float xRightWall = roomWidth * 0.5f;

            float xLeftStairs = xLeftWall + stairInset + stairWidth * 0.5f;
            float xRightStairs = xRightWall - stairInset - stairWidth * 0.5f;

            float platformWidth = roomWidth - 2.0f * (stairWidth + stairInset + 0.15f);
            float usableWidth = platformWidth - 0.4f;
            float seatSpacing = usableWidth / seatsPerRow;
            float xStart = -usableWidth * 0.5f + seatSpacing * 0.5f;

            float zCursor = rowsZStart;   // START kod ekrana (+Z)
            float y = floorY0;

            seats.clear();
            for (int i = 0; i < seatRows; i++)
            {
                float zCenter = zCursor - stepRun * 0.5f;

                // JEDAN KONTINUALAN RED (STEP + PLATFORMA)
                drawCube(
                    cubeVAO,
                    unifiedShader,
                    modelLoc,
                    glm::vec3(
                        0.0f,                       // centar sale po X
                        y + stepRise * 0.5f,        // sredina visine reda
                        zCenter                     // centar reda po Z
                    ),
                    glm::vec3(
                        roomWidth,                  // CELOM ŠIRINOM SALE
                        stepRise,                   // VISINA STEPENIKA
                        stepRun                     // DUBINA REDA
                    )
                );

                // ---- STOLICE OSTAJU ISTE ----
                glUniform1i(useTexLoc, 1);
                glBindTexture(GL_TEXTURE_2D, seatFreeTex);

                for (int s = 0; s < seatsPerRow; s++)
                {


                    float xSeat = xStart + s * seatSpacing;
                    int rowStartIndex = seats.size();

                    int seatIndex = i * seatsPerRow + s;

                    GLuint texToUse = seatFreeTex;
                    if (seatStatus[seatIndex] == SeatStatus::RESERVED) texToUse = seatReservedTex;
                    else if (seatStatus[seatIndex] == SeatStatus::SOLD) texToUse = seatSoldTex;

                    glBindTexture(GL_TEXTURE_2D, texToUse);

                    drawCube(
                        cubeVAO,
                        unifiedShader,
                        modelLoc,
                        glm::vec3(
                            xSeat,
                            y + stepRise + 0.04f + seatHeight * 0.5f,
                            zCenter + stepRun * 0.1f
                        ),
                        glm::vec3(seatWidth, seatHeight, seatDepth)
                    );

                    drawCube(
                        cubeVAO,
                        unifiedShader,
                        modelLoc,
                        glm::vec3(
                            xSeat,
                            y + stepRise + seatHeight + backHeight * 0.5f,
                            zCenter + stepRun * 0.1f - seatDepth * 0.5f + backTilt
                        ),
                        glm::vec3(seatWidth * 0.95f, backHeight, 0.12f)

                    );

                    Seat seat;
                    seat.center = glm::vec3(
                        xSeat,
                        y + stepRise + 0.04f + seatHeight * 0.5f,
                        zCenter + stepRun * 0.10f
                    );
                    seat.halfSize = glm::vec3(
                        seatWidth * 0.5f,
                        seatHeight * 0.5f,
                        seatDepth * 0.5f
                    );

                    seats.push_back(seat);



                }

                glUniform1i(useTexLoc, 0);

                // sledeći red
                zCursor -= stepRun;
                y += stepRise;
            }





            while (glfwGetTime() - startTime < 1.0 / 60) {}
            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        // ++++++++++++++++++++++++++++++++++++++++++++++++++++++ POSPREMANJE +++++++++++++++++++++++++++++++++++++++++++++++++


        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
        glDeleteProgram(unifiedShader);

        glfwTerminate();
        return 0;
    }