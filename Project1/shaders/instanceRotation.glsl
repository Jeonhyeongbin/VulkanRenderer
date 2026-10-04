// Preserve the existing G-buffer rotation convention in all geometry passes.
mat3 instanceRotation(vec3 rotation)
{
    float s = sin(rotation.x);
    float c = cos(rotation.x);
    mat3 mx = mat3(vec3(c, s, 0), vec3(-s, c, 0), vec3(0, 0, 1));

    s = sin(rotation.y);
    c = cos(rotation.y);
    mat3 my = mat3(vec3(c, 0, s), vec3(0, 1, 0), vec3(-s, 0, c));

    s = sin(rotation.z);
    c = cos(rotation.z);
    mat3 mz = mat3(vec3(1, 0, 0), vec3(0, c, s), vec3(0, -s, c));
    return mz * my * mx;
}
