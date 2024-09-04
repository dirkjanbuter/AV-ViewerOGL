#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <GL/glew.h>
#include <SDL3/SDL_opengl.h>
#include <GL/glu.h>
#include <stdlib.h>
#include <stdio.h>
#include "common.h"

typedef struct CGLTFLOADER_MESH 
{
        unsigned int count;
        float *positions;
	float *texcoords;
	float *weights;
	float *normals;
	float *indices;
	unsigned int indicescount;
	GLuint vbo;
        GLuint ibo;
        int isOK;
} CGLTFLOADER_MESH;

typedef struct CGLTFLOADER 
{
    CGLTFLOADER_MESH *meshes;
    unsigned int meshesnum;
    unsigned int meshespos;
} CGLTFLOADER;



CRESULT cgltfloader_create(CGLTFLOADER *v, char *filename);
void cgltfloader_destroy(CGLTFLOADER *v); 


