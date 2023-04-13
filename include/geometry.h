#ifndef GEOMETRY_H
#define GEOMETRY_H

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/cglm.h>

#include <stdalign.h>  // enforce alignment requirements of shader bindings

typedef struct Vertex {
    vec2 pos;
} Vertex;

// MVP matrices
typedef struct UniformBufferObject {
    alignas(16) mat4 model;
    alignas(16) mat4 view;
    alignas(16) mat4 proj;
} UniformBufferObject;

// Elapsed time since animation begin
typedef struct ParameterBufferObject {
    float deltaTime;    
} ParameterBufferObject;

#define N_PARTICLES 2048  // Note: Assumed to be evenly divisible by 256
typedef struct Particle {
    vec2 position;
    vec2 velocity;
    alignas(16) vec4 color;  // Note: Alignment is important for shaders
    float orientation;
} Particle;

// Constants needed for star
#define GEOM_STAR_INV_PHI_SQ 0.381966011250105151795413165634361882f
#define GEOM_STAR_SIN_36     0.587785252292473129168705954639072769f
#define GEOM_STAR_COS_36     0.809016994374947424102293417182819059f
#define GEOM_STAR_SIN_72     0.951056516295153572116439333379382143f
#define GEOM_STAR_COS_72     0.309016994374947424102293417182819059f

#define N_VERTICES_STAR 11
#define N_INDICES_STAR 30
typedef struct Star {
    Vertex vertices[N_VERTICES_STAR];
    uint16_t indices[N_INDICES_STAR];
} Star;

// @param cx: x-coordinate of star center
// @param cy: y-coordinate of star center
// @param d : distance from star center to tip
Star geomMakeStar(float cx, float cy, float d);

#endif /* GEOMETRY_H */
