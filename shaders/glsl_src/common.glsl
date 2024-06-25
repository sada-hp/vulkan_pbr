struct SAtmosphere
{
    vec3 L;
    vec3 S;
    vec3 E;
    vec3 T;
};

float remap(float orig, float old_min, float old_max, float new_min, float new_max)
{
    return new_min + (((orig - old_min) / (old_max - old_min)) * (new_max - new_min));
}

float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}