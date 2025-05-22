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