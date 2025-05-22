// Main scene function - CSG Intersection
float sceneSDF(vec3 p) {
    float sphere = sphereSDF(p, vec3(0.0, 0.0, 0.0), 1.0);
    float box = boxSDF(p, vec3(0.0, 0.0, 0.0), vec3(0.8, 0.8, 0.8));
    return intersectionOp(sphere, box);
}