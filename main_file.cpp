#define GLM_FORCE_RADIANS
#define GLM_FORCE_SWIZZLE

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <ctime>
#include "constants.h"
#include "lodepng.h"
#include "shaderprogram.h"
#include "myCube.h"
#include <iostream>
#include <queue>
#include <lib3mf_implicit.hpp>
#include <unordered_set>
#include <utility>


using namespace std;

float aspectRatio = 1;
ShaderProgram* sp;

GLuint tex_ground; // Tekstura dla dolnych kostek
GLuint tex_snake;  // Tekstura dla kostki poruszanej strzałkami
GLuint tex_food;  // Tekstura dla kostki jedzenia
GLuint tex_sky;  // Tekstura dla kostki jedzenia


float* vertices = myCubeVertices;
float* normals = myCubeNormals;
float* texCoords = myCubeTexCoords;
float* colors = myCubeColors;
int vertexCount = myCubeVertexCount;


float center_x = 0.0f; // Pozycja kostki w osi X
float center_y = 0.0f; // Pozycja kostki w osi Y

float random_x; // Losowa pozycja dodatkowej kostki w osi X
float random_y; // Losowa pozycja dodatkowej kostki w osi Y

enum Direction { NONE, LEFT, RIGHT, UP, DOWN };
queue<Direction> moveQueue;
Direction currentDirection;

double lastTime = 0;
const float movementSpeed = 2.0f; // units per second

glm::vec3 cameraPos;
glm::vec3 cameraTarget;
glm::vec3 smoothCameraPos;
glm::vec3 smoothCameraTarget;
const float cameraLerpSpeed = 0.1f;

namespace std {
    template <>
    struct hash<std::pair<int, int>> {
        std::size_t operator()(const std::pair<int, int>& p) const noexcept {
            return std::hash<int>{}(p.first) ^ (std::hash<int>{}(p.second) << 1);
        }
    };
}
std::unordered_set<std::pair<int, int>> snakePositions;


// List of additional cubes following the moving cube
vector<pair<float, float>> additionalCubes;
vector<bool> canMove;
std::vector<pair<float, float>> wallCubes;

typedef std::pair<float, float> FloatPair;

// Define a queue of float pairs
typedef std::queue<FloatPair> QueueOfFloatPairs;

// Define a vector of queues of float pairs
std::vector<QueueOfFloatPairs> vectorOfQueues;

// Vertex and index data for the 3MF model
std::vector<float> modelVertices;
std::vector<unsigned int> modelIndices;
std::vector<float> modelTexCoords;

void error_callback(int error, const char* description) {
    fputs(description, stderr);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (currentDirection == NONE){
            if (key == GLFW_KEY_UP)
                currentDirection = UP;
        }
        else{
        if (key == GLFW_KEY_LEFT) {
 
            switch (currentDirection) {
            case LEFT:
                moveQueue.push(DOWN);
                break;
            case RIGHT:
                moveQueue.push(UP);
                break;
            case UP:
                moveQueue.push(LEFT);
                break;
            case DOWN:
                moveQueue.push(RIGHT);
                break;
            default:
                moveQueue.push(LEFT); // Default to LEFT if no direction is set
                break;
            }
        }
        else if (key == GLFW_KEY_RIGHT) {
            switch (currentDirection) {
            case LEFT:
                moveQueue.push(UP);
                break;
            case RIGHT:
                moveQueue.push(DOWN);
                break;
            case UP:
                moveQueue.push(RIGHT);
                break;
            case DOWN:
                moveQueue.push(LEFT);
                break;
            default:
                moveQueue.push(RIGHT); // Default to RIGHT if no direction is set
                break;
            }
        }
        }
    }
}



bool load3MFModel(const std::string& filename, std::vector<float>& vertices, std::vector<unsigned int>& indices, std::vector<float>& texCoords) {
    Lib3MF::PWrapper wrapper = Lib3MF::CWrapper::loadLibrary();
    Lib3MF::PModel model = wrapper->CreateModel();

    Lib3MF::PReader reader = model->QueryReader("3mf");
    reader->ReadFromFile(filename);

    auto meshObjectIterator = model->GetMeshObjects();
    while (meshObjectIterator->MoveNext()) {
        auto meshObject = meshObjectIterator->GetCurrentMeshObject();

        std::vector<Lib3MF::sPosition> vertexArray;
        std::vector<Lib3MF::sTriangle> triangleArray;

        meshObject->GetVertices(vertexArray);
        meshObject->GetTriangleIndices(triangleArray);

        std::cout << "Vertices: " << std::endl;
        for (const auto& vertex : vertexArray) {
            vertices.push_back(vertex.m_Coordinates[0]);
            vertices.push_back(vertex.m_Coordinates[1]);
            vertices.push_back(vertex.m_Coordinates[2]);


            // Manually define texture coordinates (example: projecting vertices onto the XY plane)
            texCoords.push_back(vertex.m_Coordinates[0]); // U coordinate
            texCoords.push_back(vertex.m_Coordinates[1]); // V coordinate
        }

        std::cout << "Indices: " << std::endl;
        for (const auto& triangle : triangleArray) {
            indices.push_back(triangle.m_Indices[0]);
            indices.push_back(triangle.m_Indices[1]);
            indices.push_back(triangle.m_Indices[2]);

        }
    }

    return true;
}


void windowResizeCallback(GLFWwindow* window, int width, int height) {
    if (height == 0) return;
    aspectRatio = (float)width / (float)height;
    glViewport(0, 0, width, height);
}

GLuint readTexture(const char* filename) {
    GLuint tex;
    glActiveTexture(GL_TEXTURE0);
    std::vector<unsigned char> image;
    unsigned width, height;
    unsigned error = lodepng::decode(image, width, height, filename);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, (unsigned char*)image.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

void initOpenGLProgram(GLFWwindow* window) {
    glClearColor(0, 0, 0, 1);
    glEnable(GL_DEPTH_TEST);
    glfwSetWindowSizeCallback(window, windowResizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    sp = new ShaderProgram("v_simplest.glsl", NULL, "f_simplest.glsl");
    tex_ground = readTexture("textures/ground.png");
    tex_food = readTexture("textures/snake.png");
    tex_snake = readTexture("textures/snake.png");
    tex_sky = readTexture("textures/sky.png");

    if (!load3MFModel("models/sd.3mf", modelVertices, modelIndices, modelTexCoords)) {
        std::cerr << "Failed to load 3MF model" << std::endl;
        exit(EXIT_FAILURE);
    }

    srand(time(0));
    random_x = (rand() % 11 - 5) * 2.0f;
    random_y = (rand() % 11 - 5) * 2.0f;

    for (int x = -50; x <= 50; x += 2) {
        wallCubes.push_back(make_pair((float)x, 20.0f ));
    }

    cameraPos = glm::vec3(center_x, center_y - 10.0f, 5.0f);
    cameraTarget = glm::vec3(center_x, center_y, 2.0f);
    smoothCameraPos = cameraPos;
    smoothCameraTarget = cameraTarget;
}



void freeOpenGLProgram(GLFWwindow* window) {
    delete sp;
    glDeleteTextures(1, &tex_ground);
    glDeleteTextures(1, &tex_food);
    glDeleteTextures(1, &tex_snake);
    glDeleteTextures(1, &tex_sky);
}

void drawCube(glm::mat4 M, glm::mat4 V, glm::mat4 P, GLuint texture) {
    sp->use();
    glUniformMatrix4fv(sp->u("P"), 1, false, glm::value_ptr(P));
    glUniformMatrix4fv(sp->u("V"), 1, false, glm::value_ptr(V));
    glUniformMatrix4fv(sp->u("M"), 1, false, glm::value_ptr(M));
    glEnableVertexAttribArray(sp->a("vertex"));
    glVertexAttribPointer(sp->a("vertex"), 4, GL_FLOAT, false, 0, vertices);
    glEnableVertexAttribArray(sp->a("texCoord"));
    glVertexAttribPointer(sp->a("texCoord"), 2, GL_FLOAT, false, 0, texCoords);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(sp->u("tex"), 0);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glDisableVertexAttribArray(sp->a("vertex"));
    glDisableVertexAttribArray(sp->a("texCoord"));
}

void draw3MFModel(glm::mat4 M, glm::mat4 V, glm::mat4 P, GLuint texture) {
    sp->use();

    float scalingFactor = 0.02f;
    M = glm::scale(M, glm::vec3(scalingFactor, scalingFactor, scalingFactor));

    glUniformMatrix4fv(sp->u("P"), 1, GL_FALSE, glm::value_ptr(P));
    glUniformMatrix4fv(sp->u("V"), 1, GL_FALSE, glm::value_ptr(V));
    glUniformMatrix4fv(sp->u("M"), 1, GL_FALSE, glm::value_ptr(M));

    GLuint vbo, ebo, tbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, modelVertices.size() * sizeof(float), modelVertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, modelIndices.size() * sizeof(unsigned int), modelIndices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &tbo);
    glBindBuffer(GL_ARRAY_BUFFER, tbo);
    glBufferData(GL_ARRAY_BUFFER, modelTexCoords.size() * sizeof(float), modelTexCoords.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(sp->a("vertex"));
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(sp->a("vertex"), 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glEnableVertexAttribArray(sp->a("texCoord"));
    glBindBuffer(GL_ARRAY_BUFFER, tbo);
    glVertexAttribPointer(sp->a("texCoord"), 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(sp->u("tex"), 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glDrawElements(GL_TRIANGLES, modelIndices.size(), GL_UNSIGNED_INT, 0);

    glDisableVertexAttribArray(sp->a("vertex"));
    glDisableVertexAttribArray(sp->a("texCoord"));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &tbo);
}



bool isAtFullPosition(float x, float y) {
    return fmod(x, 2.0f) == 0.0f && fmod(y, 2.0f) == 0.0f;
}

std::deque<pair<float, float>> positionQueue;
const float positionUpdateInterval = 2.0f; // Distance interval to store positions
float positionUpdateAccumulator = 0.0f;


void updateTailPositions() {
    for (size_t i = additionalCubes.size(); i > 0; i--) {
        if (i == 1) {

            if (vectorOfQueues.size() > 0) {
                vectorOfQueues[i - 1].push(make_pair(center_x, center_y));
            }

            if(vectorOfQueues[i - 1].size() > 0 && canMove[i -1]) {
            additionalCubes[i - 1].first = vectorOfQueues[i - 1].front().first;
            additionalCubes[i - 1].second = vectorOfQueues[i - 1].front().second;
            vectorOfQueues[i - 1].pop();
            }
        }
        else {

            if (vectorOfQueues.size() > 0) {
                vectorOfQueues[i - 1].push(additionalCubes[i - 2]);
            }

            if (vectorOfQueues[i - 1].size() > 0 && canMove[i - 1]) {
                additionalCubes[i - 1].first = vectorOfQueues[i - 1].front().first;
                additionalCubes[i - 1].second = vectorOfQueues[i - 1].front().second;
                vectorOfQueues[i - 1].pop();
            }
        }
        snakePositions.insert({ static_cast<int>(additionalCubes[i - 1].first), static_cast<int>(additionalCubes[i - 1].second) });
    }
}


void printPositions() {
    cout << "Main cube position: (" << center_x << ", " << center_y << ")" << endl;
    for (size_t i = 0; i < additionalCubes.size(); i++) {
        cout << "Additional cube " << i + 1 << " position: (" << additionalCubes[i].first << ", " << additionalCubes[i].second << ")" << endl;
    }
}


float moveAmount = 0.05f;
float moveAcumulator = 0.0f;

void updatePosition(float deltaTime) {
    if (currentDirection == NONE) return;

    if (canMove.size() > 0)
        moveAcumulator += moveAmount;

    if (canMove.size() > additionalCubes.size() - 1 && moveAcumulator >= 1.8f)
        canMove[additionalCubes.size() - 1] = true;

    // Previous position
    int prev_x = static_cast<int>(center_x);
    int prev_y = static_cast<int>(center_y);

    // Move the main cube based on the current direction
    if (currentDirection == LEFT) center_x -= moveAmount;
    if (currentDirection == RIGHT) center_x += moveAmount;
    if (currentDirection == UP) center_y += moveAmount;
    if (currentDirection == DOWN) center_y -= moveAmount;

    // Round to the nearest grid position
    center_x = round(center_x * 100.0f) / 100.0f;
    center_y = round(center_y * 100.0f) / 100.0f;

    // New position
    int new_x = static_cast<int>(center_x);
    int new_y = static_cast<int>(center_y);

    // Update snake positions set
    snakePositions.erase({ prev_x, prev_y });
    snakePositions.insert({ new_x, new_y });

    positionUpdateAccumulator += moveAmount;

    if (positionUpdateAccumulator >= positionUpdateInterval) {
        positionQueue.push_front({ center_x, center_y });
        positionUpdateAccumulator = 0.0f;

        if (positionQueue.size() > additionalCubes.size() + 1) {
            positionQueue.pop_back();
        }
    }

    if (isAtFullPosition(center_x, center_y) && !moveQueue.empty()) {
        currentDirection = moveQueue.front();
        moveQueue.pop();
    }

    // Check if the main cube goes out of the board
    if (center_x < -10.0f || center_x > 10.0f || center_y < -10.0f || center_y > 10.0f) {
        std::cout << "Main cube is out of the board!" << std::endl;
    }

    // Check for collision with additional cubes
    for (const auto& cube : additionalCubes) {
        if (new_x == static_cast<int>(cube.first) && new_y == static_cast<int>(cube.second)) {
            std::cout << "Collision detected with additional cube at position: (" << cube.first << ", " << cube.second << ")" << std::endl;
        }
    }

    // Check for collision with the food cube
    if (new_x == static_cast<int>(random_x) && new_y == static_cast<int>(random_y)) {
        if (additionalCubes.empty()) {
            additionalCubes.push_back(make_pair(center_x, center_y));
            QueueOfFloatPairs queue1;
            queue1.push(make_pair(center_x, center_y));
            vectorOfQueues.push_back(queue1);
            canMove.push_back(false);
            moveAcumulator = 0.0f;
        }
        else {
            additionalCubes.push_back(additionalCubes.back());
            QueueOfFloatPairs queue1;
            queue1.push(additionalCubes.back());
            vectorOfQueues.push_back(queue1);
            canMove.push_back(false);
            moveAcumulator = 0.0f;
        }

        // Print the position of the new cube and all cubes
        std::cout << "Collision detected. New cube added at position: (" << center_x << ", " << center_y << ")" << std::endl;
        printPositions();

        // Generate new random position for the food cube
        bool positionOccupied;
        do {
            positionOccupied = false;
            random_x = (rand() % 11 - 5) * 2.0f;
            random_y = (rand() % 11 - 5) * 2.0f;

            // Check if the new position is occupied by the main cube or any additional cubes
            if (snakePositions.count({ static_cast<int>(random_x), static_cast<int>(random_y) }) > 0) {
                positionOccupied = true;
            }
        } while (positionOccupied);
    }
}




void drawSky(float angle_x, float angle_y, glm::mat4 V, glm::mat4 P) {
    glm::mat4 M_wall = glm::translate(glm::mat4(1.0f), glm::vec3(0, 70, 0));
    float scalingFactor = 50.0f; // Scale down to half the size
    M_wall = glm::scale(M_wall, glm::vec3(scalingFactor, scalingFactor, scalingFactor));
    M_wall = glm::rotate(M_wall, angle_y, glm::vec3(1.0f, 0.0f, 0.0f));
    M_wall = glm::rotate(M_wall, angle_x, glm::vec3(0.0f, 1.0f, 0.0f));
    drawCube(M_wall, V, P, tex_sky);

    M_wall = glm::translate(glm::mat4(1.0f), glm::vec3(0, -70, 0));

    M_wall = glm::scale(M_wall, glm::vec3(scalingFactor, scalingFactor, scalingFactor));
    M_wall = glm::rotate(M_wall, angle_y, glm::vec3(1.0f, 0.0f, 0.0f));
    M_wall = glm::rotate(M_wall, angle_x, glm::vec3(0.0f, 1.0f, 0.0f));
    drawCube(M_wall, V, P, tex_sky);

    M_wall = glm::translate(glm::mat4(1.0f), glm::vec3(70, 0, 0));

    M_wall = glm::scale(M_wall, glm::vec3(scalingFactor, scalingFactor, scalingFactor));
    M_wall = glm::rotate(M_wall, angle_y, glm::vec3(1.0f, 0.0f, 0.0f));
    M_wall = glm::rotate(M_wall, angle_x, glm::vec3(0.0f, 1.0f, 0.0f));
    drawCube(M_wall, V, P, tex_sky);

    M_wall = glm::translate(glm::mat4(1.0f), glm::vec3(-70, 0, 0));

    M_wall = glm::scale(M_wall, glm::vec3(scalingFactor, scalingFactor, scalingFactor));
    M_wall = glm::rotate(M_wall, angle_y, glm::vec3(1.0f, 0.0f, 0.0f));
    M_wall = glm::rotate(M_wall, angle_x, glm::vec3(0.0f, 1.0f, 0.0f));
    drawCube(M_wall, V, P, tex_sky);

    M_wall = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -70));

    M_wall = glm::scale(M_wall, glm::vec3(scalingFactor, scalingFactor, scalingFactor));
    M_wall = glm::rotate(M_wall, angle_y, glm::vec3(1.0f, 0.0f, 0.0f));
    M_wall = glm::rotate(M_wall, angle_x, glm::vec3(0.0f, 1.0f, 0.0f));
    drawCube(M_wall, V, P, tex_sky);
 
}

void drawScene(GLFWwindow* window, float angle_x = 0, float angle_y = 0) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Camera settings
    glm::vec3 newCameraPos;
    glm::vec3 newCameraTarget(center_x, center_y, 2.0f);
    glm::vec3 up(0.0f, 0.0f, 1.0f); // Up direction

    // Adjust camera position based on the direction of movement
    float cameraDistance = 10.0f;
    float cameraHeight = 5.0f;

    switch (currentDirection) {
    case LEFT:
        newCameraPos = glm::vec3(center_x + cameraDistance, center_y, cameraHeight);
        break;
    case RIGHT:
        newCameraPos = glm::vec3(center_x - cameraDistance, center_y, cameraHeight);
        break;
    case UP:
        newCameraPos = glm::vec3(center_x, center_y - cameraDistance, cameraHeight);
        break;
    case DOWN:
        newCameraPos = glm::vec3(center_x, center_y + cameraDistance, cameraHeight);
        break;
    default:
        newCameraPos = glm::vec3(center_x, center_y - cameraDistance, cameraHeight); // Default to UP
        break;
    }

    // Smoothly interpolate the camera position and target
    smoothCameraPos = glm::mix(smoothCameraPos, newCameraPos, cameraLerpSpeed);
    smoothCameraTarget = glm::mix(smoothCameraTarget, newCameraTarget, cameraLerpSpeed);

    glm::mat4 V = glm::lookAt(
        smoothCameraPos,
        smoothCameraTarget,
        up
    );

    glm::mat4 P = glm::perspective(50.0f * PI / 180.0f, aspectRatio, 0.01f, 50.0f);

    drawSky(angle_x, angle_y, V, P);

    for (int i = -5; i <= 5; i++) {
        for (int j = -5; j <= 5; j++) {
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(i * 2.0f, j * 2.0f, 0.0f));
            M = glm::rotate(M, angle_y, glm::vec3(1.0f, 0.0f, 0.0f));
            M = glm::rotate(M, angle_x, glm::vec3(0.0f, 1.0f, 0.0f));
            drawCube(M, V, P, tex_ground);
        }
    }

    glm::mat4 M_center = glm::translate(glm::mat4(1.0f), glm::vec3(center_x, center_y, 2.0f));
    M_center = glm::rotate(M_center, angle_y, glm::vec3(1.0f, 0.0f, 0.0f));
    M_center = glm::rotate(M_center, angle_x, glm::vec3(0.0f, 1.0f, 0.0f));
    drawCube(M_center, V, P, tex_snake);

    for (auto& cube : additionalCubes) {
        glm::mat4 M_additional = glm::translate(glm::mat4(1.0f), glm::vec3(cube.first, cube.second, 2.0f));
        M_additional = glm::rotate(M_additional, angle_y, glm::vec3(1.0f, 0.0f, 0.0f));
        M_additional = glm::rotate(M_additional, angle_x, glm::vec3(0.0f, 1.0f, 0.0f));
        drawCube(M_additional, V, P, tex_snake);
    }

    glm::mat4 M_random = glm::translate(glm::mat4(1.0f), glm::vec3(random_x, random_y, 2.0f));
    M_random = glm::rotate(M_random, angle_y, glm::vec3(1.0f, 0.0f, 0.0f));
    M_random = glm::rotate(M_random, angle_x, glm::vec3(0.0f, 1.0f, 0.0f));
    draw3MFModel(M_random, V, P, tex_food); // Draw the 3MF model with the food texture

    glfwSwapBuffers(window);
}


int main(void) {
    GLFWwindow* window;
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Nie można zainicjować GLFW.\n");
        exit(EXIT_FAILURE);
    }
    window = glfwCreateWindow(500, 500, "OpenGL", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Nie można utworzyć okna.\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Nie można zainicjować GLEW.\n");
        exit(EXIT_FAILURE);
    }
    initOpenGLProgram(window);
    float angle_x = 0;
    float angle_y = 0;
    glfwSetTime(0);
    lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        updateTailPositions();
        updatePosition(deltaTime);

        drawScene(window, angle_x, angle_y);
        glfwPollEvents();
    }
    freeOpenGLProgram(window);
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}
