// Main scene function - Custom Scene with smooth operations
float sceneSDF(vec3 p) {
    float sphere1 = sphereSDF(p, vec3(-0.8, 0.3, 0.0), 1.0);
    float sphere2 = sphereSDF(p, vec3(0.8, -0.2, 0.0), 0.8);
    float box = boxSDF(p, vec3(0.0, 0.0, 0.0), vec3(0.6, 0.6, 2.0));
    float cylinder = cylinderSDF(p, vec3(0.0, 0.0, -1.5), vec3(0.0, 0.0, 1.5), 0.4);

    float unionSpheres = smoothUnionOp(sphere1, sphere2, 0.2);
    float diffWithBox = smoothDifferenceOp(unionSpheres, box, 0.1);
    return smoothIntersectionOp(diffWithBox, cylinder, 0.1);
}