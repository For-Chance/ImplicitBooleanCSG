#include "ImplicitSurfaces.h"
#include "Renderer.h"
#include <iostream>
#include <memory>
#include <string>

// Forward declarations
void switchScene(ImplicitRenderer& renderer, int sceneIndex);
std::shared_ptr<ImplicitSurface> createCustomScene();

// Global renderer pointer for callback access
ImplicitRenderer* g_renderer = nullptr;

// Key callback function for scene switching
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // 使用(void)参数名的方式来标记未使用的参数，消除编译器警告
    (void)scancode; // 标记scancode参数为有意未使用
    (void)mods;     // 标记mods参数为有意未使用

    if (action == GLFW_PRESS && g_renderer) {
        int sceneIndex = -1;

        // Select scene based on pressed key
        switch (key) {
            case GLFW_KEY_1:
                sceneIndex = 0; // Single sphere
                break;
            case GLFW_KEY_2:
                sceneIndex = 1; // CSG union
                break;
            case GLFW_KEY_3:
                sceneIndex = 2; // CSG intersection
                break;
            case GLFW_KEY_4:
                sceneIndex = 3; // CSG difference
                break;
            case GLFW_KEY_5:
                sceneIndex = 4; // Complex CSG scene
                break;
            case GLFW_KEY_C:
                std::cout << "Display scene: Custom CSG Scene" << std::endl;
                g_renderer->setScene(createCustomScene());
                return;
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                return;
        }

        // Switch to the selected scene
        if (sceneIndex >= 0) {
            switchScene(*g_renderer, sceneIndex);
        }
    }
}

// Scene switching function demonstrating different CSG operations
void switchScene(ImplicitRenderer& renderer, int sceneIndex) {
    switch (sceneIndex) {
        case 0: // Single sphere
            std::cout << "Display scene: Single Sphere" << std::endl;
            renderer.setScene(renderer.createSphereScene());
            break;
        case 1: // CSG union
            std::cout << "Display scene: CSG Union Operation" << std::endl;
            renderer.setScene(renderer.createCSGUnionScene());
            break;
        case 2: // CSG intersection
            std::cout << "Display scene: CSG Intersection Operation" << std::endl;
            renderer.setScene(renderer.createCSGIntersectionScene());
            break;
        case 3: // CSG difference
            std::cout << "Display scene: CSG Difference Operation" << std::endl;
            renderer.setScene(renderer.createCSGDifferenceScene());
            break;
        case 4: // Complex CSG scene
            std::cout << "Display scene: Complex CSG Scene (Union then Difference)" << std::endl;
            renderer.setScene(renderer.createComplexCSGScene());
            break;
        default:
            std::cout << "Unknown scene index" << std::endl;
            break;
    }
}

// Custom scene creation function
std::shared_ptr<ImplicitSurface> createCustomScene() {
    // Create a complex CSG scene showcasing various boolean operations

    // Create basic shapes
    auto sphere1 = std::make_shared<Sphere>(Vec3<double>(-0.8, 0.3, 0), 1.0);
    auto sphere2 = std::make_shared<Sphere>(Vec3<double>(0.8, -0.2, 0), 0.8);
    auto box = std::make_shared<Box>(Vec3<double>(0, 0, 0), Vec3<double>(0.6, 0.6, 2.0));
    auto cylinder = std::make_shared<Cylinder>(
        Vec3<double>(0, 0, -1.5),
        Vec3<double>(0, 0, 1.5),
        0.4
    );

    // Perform CSG operations
    // First, create a union of two spheres
    auto unionSpheres = std::make_shared<SmoothUnionOp>(sphere1, sphere2, 0.2);

    // Then, subtract a box from the union
    auto diffWithBox = std::make_shared<SmoothDifferenceOp>(unionSpheres, box, 0.1);

    // Finally, intersect with a cylinder
    auto result = std::make_shared<SmoothIntersectionOp>(diffWithBox, cylinder, 0.1);

    return result;
}

// Main function
int main() {
    // Create renderer instance
    ImplicitRenderer renderer(800, 600);

    // Set global pointer for callback use
    g_renderer = &renderer;

    // Initialize
    if (!renderer.initialize()) {
        std::cerr << "Renderer initialization failed!" << std::endl;
        return -1;
    }

    // Set keyboard callback function
    glfwSetKeyCallback(renderer.getWindow(), keyCallback);

    // Set camera
    renderer.setCamera(Vec3<float>(0, 0, 5), Vec3<float>(0, 0, 0), Vec3<float>(0, 1, 0), 45.0f);

    // Set lighting
    renderer.setLight(Vec3<float>(4, 4, 4), Vec3<float>(1, 1, 1), 0.2f);

    // Set ray marching parameters
    renderer.setRaymarchingParams(100, 50.0f, 0.001f);

    // Default scene: Complex CSG operation
    renderer.setScene(renderer.createCSGIntersectionScene());

    std::cout << "Implicit Boolean CSG Demonstration" << std::endl;
    std::cout << "------------------------" << std::endl;
    std::cout << "Use number keys to switch between different CSG operation scenes:" << std::endl;
    std::cout << "1: Single Sphere" << std::endl;
    std::cout << "2: CSG Union Operation" << std::endl;
    std::cout << "3: CSG Intersection Operation" << std::endl;
    std::cout << "4: CSG Difference Operation" << std::endl;
    std::cout << "5: Complex CSG Scene (Union then Difference)" << std::endl;
    std::cout << "C: Custom CSG Scene" << std::endl;
    std::cout << "ESC: Exit Program" << std::endl;

    // Run main loop
    renderer.run();

    // Clean up global pointer
    g_renderer = nullptr;

    return 0;
}