// Main scene function - CSG Union
float sceneSDF(vec3 p) {
    float sphere1 = sphereSDF(p, vec3(-0.5, 0.0, 0.0), 1.0);
    float sphere2 = sphereSDF(p, vec3(0.5, 0.0, 0.0), 1.0);
    return unionOp(sphere1, sphere2);
}