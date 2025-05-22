// Main scene function - Complex CSG Scene
float sceneSDF(vec3 p) {
    float sphere1 = sphereSDF(p, vec3(-1.0, 0.0, 0.0), 1.2);
    float sphere2 = sphereSDF(p, vec3(1.0, 0.0, 0.0), 1.2);
    float box = boxSDF(p, vec3(0.0, 0.0, 0.0), vec3(0.8, 0.8, 0.8));
    float spheres = unionOp(sphere1, sphere2);
    return differenceOp(spheres, box);
}