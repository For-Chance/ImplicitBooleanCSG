#pragma once

#include "ImplicitSurfaces.h"
#include <vector>
#include <memory>
#include <string> // Add string header
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <iostream>

// Renderer for implicit surfaces using ray marching algorithm
class ImplicitRenderer {
private:
    int width, height;
    GLFWwindow* window;
    GLuint programID;
    GLuint vao, vbo;
    GLuint framebufferTexture;

    std::shared_ptr<ImplicitSurface> scene;

    // Camera parameters - using float instead of double
    Vec3<float> cameraPosition;
    Vec3<float> cameraTarget;
    Vec3<float> cameraUp;
    float fieldOfView;

    // Light parameters - using float instead of double
    Vec3<float> lightPosition;
    Vec3<float> lightColor;
    float ambientStrength;

    // Ray marching algorithm parameters
    int maxSteps;
    float maxDistance;
    float epsilon;

    bool setupShaders();
    bool setupBuffers();
    std::string loadShaderFile(const std::string& filePath); // New helper function
    std::string getShaderPath(const std::string& shaderFile); // Helper function to find shader paths
    std::string generateSceneSDFCode();

public:
    ImplicitRenderer(int width = 800, int height = 600);
    ~ImplicitRenderer();

    bool initialize();
    void setScene(std::shared_ptr<ImplicitSurface> scene);
    void setCamera(const Vec3<float>& position, const Vec3<float>& target, const Vec3<float>& up, float fov);
    void setLight(const Vec3<float>& position, const Vec3<float>& color, float ambientStrength);
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