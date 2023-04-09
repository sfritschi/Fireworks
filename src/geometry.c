#include "geometry.h"

Star geomMakeStar(float cx, float cy, float d, float r, float g, float b)
{
    const float s = GEOM_STAR_INV_PHI_SQ * d;  // interior pentagon distance
    
    // Note: Could use primitive topology TRIANGLE_FAN for star instead, since
    //       all triangles share central vertex. 
    //       This might not be supported however...
    Star star = {0};
    star.vertices[0]  = (Vertex) {{cx, cy - d}, {r, g, b}};
    star.vertices[1]  = (Vertex) {{cx - d * GEOM_STAR_SIN_72, cy - d * GEOM_STAR_COS_72}, {r, g, b}};
    star.vertices[2]  = (Vertex) {{cx - d * GEOM_STAR_SIN_36, cy + d * GEOM_STAR_COS_36}, {r, g, b}};
    star.vertices[3]  = (Vertex) {{cx + d * GEOM_STAR_SIN_36, cy + d * GEOM_STAR_COS_36}, {r, g, b}};
    star.vertices[4]  = (Vertex) {{cx + d * GEOM_STAR_SIN_72, cy - d * GEOM_STAR_COS_72}, {r, g, b}};
    star.vertices[5]  = (Vertex) {{cx - s * GEOM_STAR_SIN_36, cy - s * GEOM_STAR_COS_36}, {r, g, b}};
    star.vertices[6]  = (Vertex) {{cx - s * GEOM_STAR_SIN_72, cy + s * GEOM_STAR_COS_72}, {r, g, b}};
    star.vertices[7]  = (Vertex) {{cx, cy + s}, {r, g, b}};
    star.vertices[8]  = (Vertex) {{cx + s * GEOM_STAR_SIN_72, cy + s * GEOM_STAR_COS_72}, {r, g, b}};
    star.vertices[9]  = (Vertex) {{cx + s * GEOM_STAR_SIN_36, cy - s * GEOM_STAR_COS_36}, {r, g, b}};
    // Note: Changed color of central vertex
    star.vertices[10] = (Vertex) {{cx, cy}, {0.0f, 0.1f, 0.8f}};
    
    star.indices[0]  = 10;
    star.indices[1]  = 5;
    star.indices[2]  = 0;
    
    star.indices[3]  = 10;
    star.indices[4]  = 1;
    star.indices[5]  = 5;
    
    star.indices[6]  = 10;
    star.indices[7]  = 6;
    star.indices[8]  = 1;
    
    star.indices[9]  = 10;
    star.indices[10] = 2;
    star.indices[11] = 6;
    
    star.indices[12] = 10;
    star.indices[13] = 7;
    star.indices[14] = 2;
    
    star.indices[15] = 10;
    star.indices[16] = 3;
    star.indices[17] = 7;
    
    star.indices[18] = 10;
    star.indices[19] = 8;
    star.indices[20] = 3;
    
    star.indices[21] = 10;
    star.indices[22] = 4;
    star.indices[23] = 8;
    
    star.indices[24] = 10;
    star.indices[25] = 9;
    star.indices[26] = 4;
    
    star.indices[27] = 10;
    star.indices[28] = 0;
    star.indices[29] = 9;
    
    return star;
}
