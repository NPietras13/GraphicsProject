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


using namespace std;

float aspectRatio = 1;
ShaderProgram* sp;

GLuint tex_ground; // Tekstura dla dolnych kostek
GLuint tex_snake;  // Tekstura dla kostki poruszanej strzałkami
GLuint tex_food;  // Tekstura dla kostki jedzenia

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

// List of additional cubes following the moving cube
vector<pair<float, float>> additionalCubes;

void error_callback(int error, const char* description) {
    fputs(description, stderr);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_LEFT && (currentDirection != RIGHT || currentDirection == NONE)) {
            moveQueue.push(LEFT);
            if (currentDirection == NONE) currentDirection = LEFT;
        }
        if (key == GLFW_KEY_RIGHT && (currentDirection != LEFT || currentDirection == NONE)) {
            moveQueue.push(RIGHT);
            if (currentDirection == NONE) currentDirection = RIGHT;
        }
        if (key == GLFW_KEY_UP && (currentDirection != DOWN || currentDirection == NONE)) {
            moveQueue.push(UP);
            if (currentDirection == NONE) currentDirection = UP;
        }
        if (key == GLFW_KEY_DOWN && (currentDirection != UP || currentDirection == NONE)) {
            moveQueue.push(DOWN);
            if (currentDirection == NONE) currentDirection = DOWN;
        }
    }
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
    tex_food = readTexture("textures/food.png");
    tex_snake = readTexture("textures/snake.png");

    // Inicjalizacja losowych współrzędnych dodatkowej kostki
    srand(time(0));
    random_x = (rand() % 11 - 5) * 2.0f; // Losowa wartość od -10 do 10, ale tylko parzyste liczby (czyli -10, -8, ..., 8, 10)
    random_y = (rand() % 11 - 5) * 2.0f; // Losowa wartość od -10 do 10, ale tylko parzyste liczby (czyli -10, -8, ..., 8, 10)
}

void freeOpenGLProgram(GLFWwindow* window) {
    delete sp;
    glDeleteTextures(1, &tex_ground);
    glDeleteTextures(1, &tex_food);
    glDeleteTextures(1, &tex_snake);
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

void updateTailPositions() {
    for (size_t i = additionalCubes.size(); i > 0; i--) {
        if (i == 1) {
            additionalCubes[i - 1].first = center_x;
            additionalCubes[i - 1].second = center_y;
        }
        else {
            additionalCubes[i - 1] = additionalCubes[i - 2];
        }
    }
}


void printPositions() {
    cout << "Main cube position: (" << center_x << ", " << center_y << ")" << endl;
    for (size_t i = 0; i < additionalCubes.size(); i++) {
        cout << "Additional cube " << i + 1 << " position: (" << additionalCubes[i].first << ", " << additionalCubes[i].second << ")" << endl;
    }
}


void updatePosition() {
    if (currentDirection == NONE) return;

    if (!moveQueue.empty()) {
        currentDirection = moveQueue.front();
        moveQueue.pop();
    }

    // Move the main cube
    if (currentDirection == LEFT) center_x -= 2.0f;
    if (currentDirection == RIGHT) center_x += 2.0f;
    if (currentDirection == UP) center_y += 2.0f;
    if (currentDirection == DOWN) center_y -= 2.0f;

    // Check for collision
    if (center_x == random_x && center_y == random_y) {
        // Add the current random cube to the list of additional cubes
        if (additionalCubes.empty()) {
            additionalCubes.push_back(make_pair(center_x, center_y));
        }
        else {
            additionalCubes.push_back(additionalCubes.back());
        }

        // Print the position of the new cube and all cubes
        cout << "Collision detected. New cube added at position: (" << center_x << ", " << center_y << ")" << endl;
        printPositions();

        // Generate new random position for the food cube
        bool positionOccupied;
        do {
            positionOccupied = false;
            random_x = (rand() % 11 - 5) * 2.0f;
            random_y = (rand() % 11 - 5) * 2.0f;

            // Check if the new position is occupied by the main cube or any additional cubes
            if (random_x == center_x && random_y == center_y) {
                positionOccupied = true;
            }
            else {
                for (const auto& cube : additionalCubes) {
                    if (random_x == cube.first && random_y == cube.second) {
                        positionOccupied = true;
                        break;
                    }
                }
            }
        } while (positionOccupied);
    }
}



void drawScene(GLFWwindow* window, float angle_x = 0, float angle_y = 0) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glm::mat4 V = glm::lookAt(
        glm::vec3(0, -20, 25),
        glm::vec3(0, 0, 0),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 P = glm::perspective(50.0f * PI / 180.0f, aspectRatio, 0.01f, 50.0f);

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
    drawCube(M_random, V, P, tex_food);

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
        if (currentTime - lastTime >= 0.2) {
            updateTailPositions();
            updatePosition();
            lastTime = currentTime;
        }

        drawScene(window, angle_x, angle_y);
        glfwPollEvents();
    }
    freeOpenGLProgram(window);
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}



