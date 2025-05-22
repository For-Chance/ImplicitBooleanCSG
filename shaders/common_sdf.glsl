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

// Normal calculation function
vec3 sceneNormal(vec3 p) {
    const float h = 0.0001;
    float dx = sceneSDF(p + vec3(h, 0, 0)) - sceneSDF(p - vec3(h, 0, 0));
    float dy = sceneSDF(p + vec3(0, h, 0)) - sceneSDF(p - vec3(0, h, 0));
    float dz = sceneSDF(p + vec3(0, 0, h)) - sceneSDF(p - vec3(0, 0, h));
    return normalize(vec3(dx, dy, dz));
}