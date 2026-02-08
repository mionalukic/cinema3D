
 #include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>  
#include <ctime>  

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "Globals.h"
#include "Camera.h"
#include "Scene.h"
#include "People.h"


//GLM biblioteke
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Util.h"

#include "shader.hpp"
#include "model.hpp"

#include <assimp/Importer.hpp>



int selectedSeat = -1;

bool useTex = false;
bool transparent = false;

float platformHeight = stepRise;     // debljina platforme
float platformInset = stairWidth + stairInset + 0.15f;
float platformHeightExtra = 0.0f;


float platformWidth = roomWidth - 2.0f * (stairWidth + stairInset + 0.15f);

float usableWidth = platformWidth - 0.4f;

float frontClear = 7.5f;   // prazan prostor kod ekrana (parter)
float backClear = 1.0f;   // margina kod zadnjeg zida


float screenWidth = roomWidth * 0.7f;   // ~70% zida
float screenHeight = roomHeight * 0.45f;
float screenZ = roomDepth * 0.5f -0.2f; // malo ispred zida
float screenY = roomHeight * 0.55f ;       // iznad partera


float lastFrame = (float)glfwGetTime();

// finiji osećaj
float baseSpeed = 2.2f;     // m/s (realističnije)
float sprintMul = 2.0f;     // SHIFT
float stepSmooth = 10.0f;   // veće = brže prati pod (8–14 je ok)

// ===== VRATA =====
float doorWidth = 1.0f;   // 90–100 cm realno
float doorHeight = 2.1f;
float doorThick = 0.08f;

float doorAngle = 0.0f;        // 0 = zatvorena
float doorTargetAngle = 0.0f;  // 0 ili 90
float doorSpeed = 120.0f;      // deg/sec



Model* catModel = nullptr;

std::vector<Model*> humanModels;

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



int hoveredSeat = -1;





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









int maxPeople = 20;
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

    resetPeople();
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
    catModel = new Model("res/models/cat/12221_Cat_v1_l3.obj");

    const int HUMAN_COUNT = 1;

    for (int i = 1; i <= HUMAN_COUNT; i++)
    {
        std::string path =
            "res/models/humans/human" +
            std::to_string(i) +
            "/model.obj";

        humanModels.push_back(new Model(path));
    }


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

    view = getViewMatrix();
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

        float screenPulse = 1.0f;

        if (cinemaState == CinemaState::PLAYING)
        {
            float t = (float)(glfwGetTime() - movieStart);
            screenPulse = 0.9f + 0.3f * sin(t * 6.0f);
        }

        glUseProgram(unifiedShader);
        glUniform1f(
            glGetUniformLocation(unifiedShader, "screenPulse"),
            screenPulse
        );




        updatePeople(dt);

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
                glm::value_ptr(glm::normalize(glm::vec3(0.0f, -0.55f, -1.0f))));

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

        view = getViewMatrix();
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
            screenPulse = 0.85f + 0.35f * sin((float)glfwGetTime() * 6.0f);

        }
        else
        {
            glUniform1i(useTexLoc, 0);                 // NE koristi teksturu
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        glUniform1f(
            glGetUniformLocation(unifiedShader, "screenPulse"),
            screenPulse
        );


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

        // ================= LJUDI (MODEL MAČKE) =================
        modelShader.use();
        modelShader.setMat4("view", view);
        modelShader.setMat4("projection", projectionP);

        for (auto& p : people)
        {
            glm::mat4 m = glm::mat4(1.0f);

            m = glm::translate(m, glm::vec3(
                p.pos.x,
                p.pos.y,
                p.pos.z
            ));

            // 1️⃣ Z-up → Y-up (Assimp people modeli su skoro uvek Z-up)
            m = glm::rotate(m, glm::radians(90.0f), glm::vec3(0, 1, 0));

            // 2️⃣ Okreni ka platnu
            m = glm::rotate(m, glm::radians(180.0f), glm::vec3(0, 1, 0));

            // 3️⃣ Duplo manja skala
            m = glm::scale(m, glm::vec3(0.0075f));



            // okretanje ka ekranu
            modelShader.setMat4("model", m);

            // 🔥 OVO JE KLJUČ
            //catModel->Draw(modelShader);
            humanModels[p.modelIndex]->Draw(modelShader);
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

        drawScene(
            cubeVAO,
            unifiedShader,
            modelLoc,
            seatFreeTex,
            seatReservedTex,
            seatSoldTex,
            useTexLoc
        );

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
