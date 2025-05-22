#pragma once

#include "ImplicitSurfaces.h"
#include <vector>
#include <memory>
#include <string> // Add string header
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Renderer for implicit surfaces using ray marching algorithm
class ImplicitRenderer {
private:
    int width, height;
    GLFWwindow* window;
    GLuint programID;
    GLuint vao, vbo;
    GLuint framebufferTexture;

    std::shared_ptr<ImplicitSurface> scene;

    // Camera parameters
    Vec3 cameraPosition;
    Vec3 cameraTarget;
    Vec3 cameraUp;
    float fieldOfView;

    // Light parameters
    Vec3 lightPosition;
    Vec3 lightColor;
    float ambientStrength;

    // Ray marching algorithm parameters
    int maxSteps;
    float maxDistance;
    float epsilon;

    bool setupShaders();
    bool setupBuffers();
    std::string generateSceneSDFCode(); // Declaration of generateSceneSDFCode function

public:
    ImplicitRenderer(int width = 800, int height = 600);
    ~ImplicitRenderer();

    bool initialize();
    void setScene(std::shared_ptr<ImplicitSurface> scene);
    void setCamera(const Vec3& position, const Vec3& target, const Vec3& up, float fov);
    void setLight(const Vec3& position, const Vec3& color, float ambientStrength);
    void setRaymarchingParams(int maxSteps, float maxDistance, float epsilon);

    // Get GLFW window for setting callback functions
    GLFWwindow* getWindow() { return window; }

    void render();
    void run();

    // Scene creation helper functions
    static std::shared_ptr<ImplicitSurface> createSphereScene();
    static std::shared_ptr<ImplicitSurface> createCSGUnionScene();
    static std::shared_ptr<ImplicitSurface> createCSGIntersectionScene();
    static std::shared_ptr<ImplicitSurface> createCSGDifferenceScene();
    static std::shared_ptr<ImplicitSurface> createComplexCSGScene();
};