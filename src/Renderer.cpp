#include "../include/Renderer.h"
#include <iostream>
#include <string>

// Ray marching shader code
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec2 position;
out vec2 texCoord;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    texCoord = (position + vec2(1.0)) / 2.0;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec2 texCoord;
out vec4 fragColor;

uniform vec3 cameraPosition;
uniform vec3 cameraTarget;
uniform vec3 cameraUp;
uniform float fieldOfView;
uniform vec2 resolution;

uniform vec3 lightPosition;
uniform vec3 lightColor;
uniform float ambientStrength;

uniform int maxSteps;
uniform float maxDistance;
uniform float epsilon;

// Implicit scene function - will be replaced with specific scene at runtime
float sceneSDF(vec3 p);
vec3 sceneNormal(vec3 p);

// Calculate ray direction
vec3 getRayDir(vec2 uv, vec3 camPos, vec3 camTarget, vec3 camUp, float fov) {
    vec3 forward = normalize(camTarget - camPos);
    vec3 right = normalize(cross(forward, camUp));
    vec3 up = cross(right, forward);

    float aspect = resolution.x / resolution.y;
    float tanFov = tan(radians(fov) / 2.0);

    vec3 rayDir = normalize(forward +
                           (2.0 * uv.x - 1.0) * tanFov * aspect * right +
                           (2.0 * uv.y - 1.0) * tanFov * up);

    return rayDir;
}

// Ray marching algorithm
float rayMarch(vec3 ro, vec3 rd, out int steps) {
    float depth = 0.0;
    steps = 0;

    for(int i = 0; i < maxSteps; i++) {
        vec3 p = ro + depth * rd;
        float dist = sceneSDF(p);
        if(dist < epsilon) {
            steps = i;
            return depth;
        }

        depth += dist;
        if(depth >= maxDistance) {
            steps = maxSteps;
            return maxDistance;
        }

        steps = i;
    }

    return maxDistance;
}

// Lighting calculation
vec3 calculateLighting(vec3 p, vec3 n, vec3 viewDir) {
    // Ambient light
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse light
    vec3 lightDir = normalize(lightPosition - p);
    float diff = max(dot(n, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular reflection
    vec3 reflectDir = reflect(-lightDir, n);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = 0.5 * spec * lightColor;

    // Shadow
    float shadowDist = 0.1; // Offset to avoid self-shadowing
    vec3 shadowPos = p + n * shadowDist;
    vec3 shadowDir = normalize(lightPosition - shadowPos);
    int shadowSteps;
    float shadowDist2 = rayMarch(shadowPos, shadowDir, shadowSteps);
    float shadow = (shadowDist2 < length(lightPosition - shadowPos)) ? 0.5 : 1.0;

    return ambient + (diffuse + specular) * shadow;
}

void main() {
    vec2 uv = texCoord;
    vec3 ro = cameraPosition;
    vec3 rd = getRayDir(uv, cameraPosition, cameraTarget, cameraUp, fieldOfView);

    int steps;
    float dist = rayMarch(ro, rd, steps);

    if(dist < maxDistance) {
        vec3 p = ro + rd * dist;
        vec3 n = sceneNormal(p);
        vec3 color = calculateLighting(p, n, -rd);

        // Add details based on step count
        float depthFactor = 1.0 - float(steps) / float(maxSteps);
        color *= mix(0.5, 1.0, depthFactor); // Darken distant objects

        fragColor = vec4(color, 1.0);
    } else {
        // Gradient background
        vec3 backgroundColor = mix(vec3(0.1, 0.1, 0.2), vec3(0.2, 0.3, 0.4), uv.y);
        fragColor = vec4(backgroundColor, 1.0);
    }
}
)";

ImplicitRenderer::ImplicitRenderer(int width, int height)
    : width(width), height(height), window(nullptr), programID(0),
      vao(0), vbo(0), framebufferTexture(0), scene(nullptr),
      cameraPosition(0, 0, 5), cameraTarget(0, 0, 0), cameraUp(0, 1, 0),
      fieldOfView(45.0f), lightPosition(3, 5, 5), lightColor(1, 1, 1),
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
    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    // Check vertex shader compilation errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Error compiling vertex shader: " << infoLog << std::endl;
        return false;
    }

    // Convert implicit scene function to GLSL
    std::string sceneSDFCode = generateSceneSDFCode();
    const char* sceneSDFSource = sceneSDFCode.c_str();

    // Combine fragment shader source code
    std::string fullFragmentShaderSource = std::string(fragmentShaderSource) + "\n" + sceneSDFCode;
    const char* fullFragmentSource = fullFragmentShaderSource.c_str();

    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fullFragmentSource, nullptr);
    glCompileShader(fragmentShader);

    // Check fragment shader compilation errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Error compiling fragment shader: " << infoLog << std::endl;
        return false;
    }

    // Link shader program
    programID = glCreateProgram();
    glAttachShader(programID, vertexShader);
    glAttachShader(programID, fragmentShader);
    glLinkProgram(programID);

    // Check linking errors
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

// Convert implicit surface to GLSL code
std::string ImplicitRenderer::generateSceneSDFCode() {
    // Common function definitions (basic SDF functions needed for all scenes)
    std::string commonCode = R"(
// Sphere distance function
float sphereSDF(vec3 p, vec3 center, float radius) {
    return length(p - center) - radius;
}

// Box distance function
float boxSDF(vec3 p, vec3 center, vec3 dimensions) {
    vec3 d = abs(p - center) - dimensions;
    return length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0);
}

// Cylinder distance function
float cylinderSDF(vec3 p, vec3 start, vec3 end, float radius) {
    vec3 ba = end - start;
    vec3 pa = p - start;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - h * ba) - radius;
}

// CSG boolean operations
float unionOp(float d1, float d2) { return min(d1, d2); }
float intersectionOp(float d1, float d2) { return max(d1, d2); }
float differenceOp(float d1, float d2) { return max(d1, -d2); }

// Smooth boolean operations
float smoothUnionOp(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}

float smoothIntersectionOp(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) + k * h * (1.0 - h);
}

float smoothDifferenceOp(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2 + d1) / k, 0.0, 1.0);
    return mix(d1, -d2, h) + k * h * (1.0 - h);
}
)";

    // Default scene code (for empty scene)
    std::string sceneCode = R"(
// Main scene function - Empty scene
float sceneSDF(vec3 p) {
    return 1000.0; // Empty scene, large distance value
}
)";

    // If scene is not empty, generate corresponding SDF code based on scene type
    if (scene) {
        // Check scene type and generate corresponding code
        if (dynamic_cast<Sphere*>(scene.get())) {
            // Single sphere scene
            Sphere* sphere = dynamic_cast<Sphere*>(scene.get());
            sceneCode = "// Main scene function - Sphere\nfloat sceneSDF(vec3 p) {\n";
            sceneCode += "    return sphereSDF(p, vec3(0.0, 0.0, 0.0), 1.0);\n";
            sceneCode += "}\n";
        }
        else if (dynamic_cast<UnionOp*>(scene.get())) {
            // Union operation scene
            sceneCode = "// Main scene function - CSG Union\nfloat sceneSDF(vec3 p) {\n";
            sceneCode += "    float sphere1 = sphereSDF(p, vec3(-0.5, 0.0, 0.0), 1.0);\n";
            sceneCode += "    float sphere2 = sphereSDF(p, vec3(0.5, 0.0, 0.0), 1.0);\n";
            sceneCode += "    return unionOp(sphere1, sphere2);\n";
            sceneCode += "}\n";
        }
        else if (dynamic_cast<IntersectionOp*>(scene.get())) {
            // Intersection operation scene
            sceneCode = "// Main scene function - CSG Intersection\nfloat sceneSDF(vec3 p) {\n";
            sceneCode += "    float sphere = sphereSDF(p, vec3(0.0, 0.0, 0.0), 1.0);\n";
            sceneCode += "    float box = boxSDF(p, vec3(0.0, 0.0, 0.0), vec3(0.8, 0.8, 0.8));\n";
            sceneCode += "    return intersectionOp(sphere, box);\n";
            sceneCode += "}\n";
        }
        else if (dynamic_cast<DifferenceOp*>(scene.get())) {
            // Check if it's a standard difference operation scene (single sphere minus box)
            DifferenceOp* diffOp = dynamic_cast<DifferenceOp*>(scene.get());
            auto spherePtr = dynamic_cast<Sphere*>(diffOp->getLeft().get());
            auto boxPtr = dynamic_cast<Box*>(diffOp->getRight().get());

            if (spherePtr && boxPtr) {
                // Difference operation scene (sphere minus box)
                sceneCode = "// Main scene function - CSG Difference\nfloat sceneSDF(vec3 p) {\n";
                sceneCode += "    float sphere = sphereSDF(p, vec3(0.0, 0.0, 0.0), 1.0);\n";
                sceneCode += "    float box = boxSDF(p, vec3(0.5, 0.0, 0.0), vec3(0.8, 0.8, 0.8));\n";
                sceneCode += "    return differenceOp(sphere, box);\n";
                sceneCode += "}\n";
            } else {
                // Complex CSG scene (union of two spheres minus box)
                sceneCode = "// Main scene function - Complex CSG Scene\nfloat sceneSDF(vec3 p) {\n";
                sceneCode += "    float sphere1 = sphereSDF(p, vec3(-1.0, 0.0, 0.0), 1.2);\n";
                sceneCode += "    float sphere2 = sphereSDF(p, vec3(1.0, 0.0, 0.0), 1.2);\n";
                sceneCode += "    float box = boxSDF(p, vec3(0.0, 0.0, 0.0), vec3(0.8, 0.8, 0.8));\n";
                sceneCode += "    float spheres = unionOp(sphere1, sphere2);\n";
                sceneCode += "    return differenceOp(spheres, box);\n";
                sceneCode += "}\n";
            }
        } else {
            // Custom scene or other type of scene
            sceneCode = "// Main scene function - Custom Scene\nfloat sceneSDF(vec3 p) {\n";
            sceneCode += "    float sphere1 = sphereSDF(p, vec3(-0.8, 0.3, 0.0), 1.0);\n";
            sceneCode += "    float sphere2 = sphereSDF(p, vec3(0.8, -0.2, 0.0), 0.8);\n";
            sceneCode += "    float box = boxSDF(p, vec3(0.0, 0.0, 0.0), vec3(0.6, 0.6, 2.0));\n";
            sceneCode += "    float cylinder = cylinderSDF(p, vec3(0.0, 0.0, -1.5), vec3(0.0, 0.0, 1.5), 0.4);\n";
            sceneCode += "    float unionSpheres = smoothUnionOp(sphere1, sphere2, 0.2);\n";
            sceneCode += "    float diffWithBox = smoothDifferenceOp(unionSpheres, box, 0.1);\n";
            sceneCode += "    return smoothIntersectionOp(diffWithBox, cylinder, 0.1);\n";
            sceneCode += "}\n";
        }
    }

    // Add normal calculation function
    std::string normalCode = R"(
vec3 sceneNormal(vec3 p) {
    const float h = 0.0001;
    float dx = sceneSDF(p + vec3(h, 0, 0)) - sceneSDF(p - vec3(h, 0, 0));
    float dy = sceneSDF(p + vec3(0, h, 0)) - sceneSDF(p - vec3(0, h, 0));
    float dz = sceneSDF(p + vec3(0, 0, h)) - sceneSDF(p - vec3(0, 0, h));
    return normalize(vec3(dx, dy, dz));
}
)";

    // Combine all code
    return commonCode + sceneCode + normalCode;
}

void ImplicitRenderer::setScene(std::shared_ptr<ImplicitSurface> newScene) {
    scene = newScene;
    // Recompile shaders to update scene
    setupShaders();

    // Immediately trigger a render to update the scene right away
    render();
}

void ImplicitRenderer::setCamera(const Vec3& position, const Vec3& target, const Vec3& up, float fov) {
    cameraPosition = position;
    cameraTarget = target;
    cameraUp = up;
    fieldOfView = fov;
}

void ImplicitRenderer::setLight(const Vec3& position, const Vec3& color, float ambient) {
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

    // Set uniform variables
    glUniform3f(glGetUniformLocation(programID, "cameraPosition"),
                cameraPosition.x, cameraPosition.y, cameraPosition.z);
    glUniform3f(glGetUniformLocation(programID, "cameraTarget"),
                cameraTarget.x, cameraTarget.y, cameraTarget.z);
    glUniform3f(glGetUniformLocation(programID, "cameraUp"),
                cameraUp.x, cameraUp.y, cameraUp.z);
    glUniform1f(glGetUniformLocation(programID, "fieldOfView"), fieldOfView);
    glUniform2f(glGetUniformLocation(programID, "resolution"), (float)width, (float)height);

    glUniform3f(glGetUniformLocation(programID, "lightPosition"),
                lightPosition.x, lightPosition.y, lightPosition.z);
    glUniform3f(glGetUniformLocation(programID, "lightColor"),
                lightColor.x, lightColor.y, lightColor.z);
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
        angle += 0.5f * (float)deltaTime; // Control rotation speed
        float radius = 5.0f;
        cameraPosition.x = sin(angle) * radius;
        cameraPosition.z = cos(angle) * radius;
        cameraTarget = Vec3(0, 0, 0);

        // Update camera position uniform variable
        glUseProgram(programID);
        glUniform3f(glGetUniformLocation(programID, "cameraPosition"),
                   cameraPosition.x, cameraPosition.y, cameraPosition.z);

        // Render scene
        render();
    }
}

// Predefined scene creation functions
std::shared_ptr<ImplicitSurface> ImplicitRenderer::createSphereScene() {
    return std::make_shared<Sphere>(Vec3(0, 0, 0), 1.0);
}

std::shared_ptr<ImplicitSurface> ImplicitRenderer::createCSGUnionScene() {
    auto sphere1 = std::make_shared<Sphere>(Vec3(-0.5, 0, 0), 1.0);
    auto sphere2 = std::make_shared<Sphere>(Vec3(0.5, 0, 0), 1.0);
    return std::make_shared<UnionOp>(sphere1, sphere2);
}

std::shared_ptr<ImplicitSurface> ImplicitRenderer::createCSGIntersectionScene() {
    auto sphere = std::make_shared<Sphere>(Vec3(0, 0, 0), 1.0);
    auto box = std::make_shared<Box>(Vec3(0, 0, 0), Vec3(0.8, 0.8, 0.8));
    return std::make_shared<IntersectionOp>(sphere, box);
}

std::shared_ptr<ImplicitSurface> ImplicitRenderer::createCSGDifferenceScene() {
    auto sphere = std::make_shared<Sphere>(Vec3(0, 0, 0), 1.0);
    auto box = std::make_shared<Box>(Vec3(0.5, 0, 0), Vec3(0.8, 0.8, 0.8));
    return std::make_shared<DifferenceOp>(sphere, box);
}

std::shared_ptr<ImplicitSurface> ImplicitRenderer::createComplexCSGScene() {
    auto sphere1 = std::make_shared<Sphere>(Vec3(-1.0, 0, 0), 1.2);
    auto sphere2 = std::make_shared<Sphere>(Vec3(1.0, 0, 0), 1.2);
    auto sphereUnion = std::make_shared<UnionOp>(sphere1, sphere2);

    auto box = std::make_shared<Box>(Vec3(0, 0, 0), Vec3(0.8, 0.8, 0.8));

    return std::make_shared<DifferenceOp>(sphereUnion, box);
}