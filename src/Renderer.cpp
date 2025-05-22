#include "../include/Renderer.h"
#include <iostream>
#include <string>
#include <fstream>

// Add a function to get the correct shader directory path
std::string ImplicitRenderer::getShaderPath(const std::string& shaderFile) {
    // Try multiple possible paths to find the shader files
    std::vector<std::string> possiblePaths = {
        // Current directory
        shaderFile,
        // Relative to executable directory
        "shaders/" + shaderFile,
        // One level up (if running from build/bin/Debug)
        "../../../shaders/" + shaderFile,
        // Absolute path based on the project structure
        "e:/毕业论文/ImplicitBooleanCSG/shaders/" + shaderFile
    };

    for (const auto& path : possiblePaths) {
        // Check if file exists by trying to open it
        std::ifstream f(path.c_str());
        if (f.good()) {
            f.close();
            return path;
        }
    }

    // If none of the paths exist, return the original path and log a warning
    std::cerr << "Warning: Could not find shader file in any expected location: " << shaderFile << std::endl;
    return shaderFile;
}

ImplicitRenderer::ImplicitRenderer(int width, int height)
    : width(width), height(height), window(nullptr), programID(0),
      vao(0), vbo(0), framebufferTexture(0), scene(nullptr),
      cameraPosition(0.0f, 0.0f, 5.0f), cameraTarget(0.0f, 0.0f, 0.0f), cameraUp(0.0f, 1.0f, 0.0f),
      fieldOfView(45.0f), lightPosition(3.0f, 5.0f, 5.0f), lightColor(1.0f, 1.0f, 1.0f),
      ambientStrength(0.1f), maxSteps(100), maxDistance(100.0f), epsilon(0.001f)
{
}

ImplicitRenderer::~ImplicitRenderer() {
    if (programID) glDeleteProgram(programID);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (framebufferTexture) glDeleteTextures(1, &framebufferTexture);

    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

// Helper function to load shader from file
std::string ImplicitRenderer::loadShaderFile(const std::string& filePath) {
    std::string shaderCode;
    std::ifstream shaderFile;

    // Ensure ifstream objects can throw exceptions
    shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        // Open files
        shaderFile.open(filePath);
        std::stringstream shaderStream;

        // Read file's buffer contents into streams
        shaderStream << shaderFile.rdbuf();

        // Close file handlers
        shaderFile.close();

        // Convert stream into string
        shaderCode = shaderStream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << filePath << std::endl;
        std::cerr << e.what() << std::endl;
    }

    return shaderCode;
}

bool ImplicitRenderer::initialize() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "Implicit Boolean CSG Renderer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return false;
    }

    if (!setupShaders() || !setupBuffers()) {
        return false;
    }

    glfwSwapInterval(1); // Enable vsync

    return true;
}

bool ImplicitRenderer::setupShaders() {
    // Load shader source code with the correct path finder
    std::string vertexShaderCode = loadShaderFile(getShaderPath("vertex.vert"));
    std::string fragmentShaderCode = loadShaderFile(getShaderPath("fragment.frag"));
    std::string commonSDFCode = loadShaderFile(getShaderPath("common_sdf.glsl"));

    // Get scene-specific code based on scene type
    std::string sceneSpecificCode;

    // If no scene is set, use default empty scene
    if (!scene) {
        sceneSpecificCode = "float sceneSDF(vec3 p) { return 1000.0; }"; // Default empty scene
    }
    else if (dynamic_cast<Sphere*>(scene.get())) {
        sceneSpecificCode = loadShaderFile(getShaderPath("scene_sphere.frag"));
    }
    else if (dynamic_cast<UnionOp*>(scene.get())) {
        sceneSpecificCode = loadShaderFile(getShaderPath("scene_union.frag"));
    }
    else if (dynamic_cast<IntersectionOp*>(scene.get())) {
        sceneSpecificCode = loadShaderFile(getShaderPath("scene_intersection.frag"));
    }
    else if (dynamic_cast<DifferenceOp*>(scene.get())) {
        DifferenceOp* diffOp = dynamic_cast<DifferenceOp*>(scene.get());
        auto spherePtr = dynamic_cast<Sphere*>(diffOp->getLeft().get());
        auto boxPtr = dynamic_cast<Box*>(diffOp->getRight().get());

        if (spherePtr && boxPtr) {
            // Standard difference operation (sphere - box)
            sceneSpecificCode = loadShaderFile(getShaderPath("scene_difference.frag"));
        } else {
            // Complex difference operation (possibly union of shapes - box)
            sceneSpecificCode = loadShaderFile(getShaderPath("scene_complex.frag"));
        }
    }
    else {
        // Custom scene
        sceneSpecificCode = loadShaderFile(getShaderPath("scene_custom.frag"));
    }

    // 编译顶点着色器
    const char* vertexSource = vertexShaderCode.c_str();
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);

    // 检查顶点着色器编译错误
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Error compiling vertex shader: " << infoLog << std::endl;
        return false;
    }

    // 将片段着色器与通用SDF代码和场景特定代码组合
    std::string fullFragmentCode = fragmentShaderCode + "\n" + commonSDFCode + "\n" + sceneSpecificCode;
    const char* fragmentSource = fullFragmentCode.c_str();

    // 编译片段着色器
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    // 检查片段着色器编译错误
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Error compiling fragment shader: " << infoLog << std::endl;
        return false;
    }

    // 链接着色器程序
    programID = glCreateProgram();
    glAttachShader(programID, vertexShader);
    glAttachShader(programID, fragmentShader);
    glLinkProgram(programID);

    // 检查链接错误
    glGetProgramiv(programID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(programID, 512, nullptr, infoLog);
        std::cerr << "Error linking shader program: " << infoLog << std::endl;
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}

bool ImplicitRenderer::setupBuffers() {
    // Screen rectangle vertex data
    float vertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

// This function can be simplified or removed since we're now loading shader code from files
std::string ImplicitRenderer::generateSceneSDFCode() {
    // Return empty string since we're now loading shader code from files
    return "";
}

void ImplicitRenderer::setScene(std::shared_ptr<ImplicitSurface> newScene) {
    scene = newScene;
    // Recompile shaders to update scene
    setupShaders();

    // Immediately trigger a render to update the scene right away
    render();
}

void ImplicitRenderer::setCamera(const Vec3<float>& position, const Vec3<float>& target, const Vec3<float>& up, float fov) {
    cameraPosition = position;
    cameraTarget = target;
    cameraUp = up;
    fieldOfView = fov;
}

void ImplicitRenderer::setLight(const Vec3<float>& position, const Vec3<float>& color, float ambient) {
    lightPosition = position;
    lightColor = color;
    ambientStrength = ambient;
}

void ImplicitRenderer::setRaymarchingParams(int steps, float distance, float eps) {
    maxSteps = steps;
    maxDistance = distance;
    epsilon = eps;
}

void ImplicitRenderer::render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(programID);

    // Set uniform variables without explicit casts since we're using float types
    glUniform3f(glGetUniformLocation(programID, "cameraPosition"),
                cameraPosition.x,
                cameraPosition.y,
                cameraPosition.z);
    glUniform3f(glGetUniformLocation(programID, "cameraTarget"),
                cameraTarget.x,
                cameraTarget.y,
                cameraTarget.z);
    glUniform3f(glGetUniformLocation(programID, "cameraUp"),
                cameraUp.x,
                cameraUp.y,
                cameraUp.z);
    glUniform1f(glGetUniformLocation(programID, "fieldOfView"), fieldOfView);
    glUniform2f(glGetUniformLocation(programID, "resolution"), width, height);

    glUniform3f(glGetUniformLocation(programID, "lightPosition"),
                lightPosition.x,
                lightPosition.y,
                lightPosition.z);
    glUniform3f(glGetUniformLocation(programID, "lightColor"),
                lightColor.x,
                lightColor.y,
                lightColor.z);
    glUniform1f(glGetUniformLocation(programID, "ambientStrength"), ambientStrength);

    glUniform1i(glGetUniformLocation(programID, "maxSteps"), maxSteps);
    glUniform1f(glGetUniformLocation(programID, "maxDistance"), maxDistance);
    glUniform1f(glGetUniformLocation(programID, "epsilon"), epsilon);

    // Draw fullscreen rectangle
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glfwSwapBuffers(window);
    glfwPollEvents();
}

void ImplicitRenderer::run() {
    // Initial angle and previous frame time
    float angle = 0.0f;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        // Calculate frame time delta
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        // Process input events - this line is crucial to ensure key callbacks are executed
        glfwPollEvents();

        // Camera rotation logic
        angle += 0.5f * static_cast<float>(deltaTime); // Control rotation speed
        float radius = 5.0f;
        cameraPosition.x = sin(angle) * radius;
        cameraPosition.z = cos(angle) * radius;
        cameraTarget = Vec3<float>(0.0f, 0.0f, 0.0f);

        // Update camera position uniform variable
        glUseProgram(programID);
        glUniform3f(glGetUniformLocation(programID, "cameraPosition"),
                   cameraPosition.x,
                   cameraPosition.y,
                   cameraPosition.z);

        // Render scene
        render();
    }
}

// Predefined scene creation functions
std::shared_ptr<ImplicitSurface> ImplicitRenderer::createSphereScene() {
    return std::make_shared<Sphere>(Vec3<double>(0.0, 0.0, 0.0), 1.0);
}

std::shared_ptr<ImplicitSurface> ImplicitRenderer::createCSGUnionScene() {
    auto sphere1 = std::make_shared<Sphere>(Vec3<double>(-0.5, 0.0, 0.0), 1.0);
    auto sphere2 = std::make_shared<Sphere>(Vec3<double>(0.5, 0.0, 0.0), 1.0);
    return std::make_shared<UnionOp>(sphere1, sphere2);
}

std::shared_ptr<ImplicitSurface> ImplicitRenderer::createCSGIntersectionScene() {
    auto sphere = std::make_shared<Sphere>(Vec3<double>(0.0, 0.0, 0.0), 1.0);
    auto box = std::make_shared<Box>(Vec3<double>(0.0, 0.0, 0.0), Vec3<double>(0.8, 0.8, 0.8));
    return std::make_shared<IntersectionOp>(sphere, box);
}

std::shared_ptr<ImplicitSurface> ImplicitRenderer::createCSGDifferenceScene() {
    auto sphere = std::make_shared<Sphere>(Vec3<double>(0.0, 0.0, 0.0), 1.0);
    auto box = std::make_shared<Box>(Vec3<double>(0.5, 0.0, 0.0), Vec3<double>(0.8, 0.8, 0.8));
    return std::make_shared<DifferenceOp>(sphere, box);
}

std::shared_ptr<ImplicitSurface> ImplicitRenderer::createComplexCSGScene() {
    auto sphere1 = std::make_shared<Sphere>(Vec3<double>(-1.0, 0.0, 0.0), 1.2);
    auto sphere2 = std::make_shared<Sphere>(Vec3<double>(1.0, 0.0, 0.0), 1.2);
    auto sphereUnion = std::make_shared<UnionOp>(sphere1, sphere2);

    auto box = std::make_shared<Box>(Vec3<double>(0.0, 0.0, 0.0), Vec3<double>(0.8, 0.8, 0.8));

    return std::make_shared<DifferenceOp>(sphereUnion, box);
}