#include "cgltfloader.h"

#define CGLTF_IMPLEMENTATION
#include "../cgltf/cgltf.h"

CRESULT cgltfloader_allocatemeshes(CGLTFLOADER *v, unsigned int num) 
{
      if(v->meshes == NULL)
      {
          v->meshesnum = num;
          v->meshespos = 0;
          v->meshes = malloc((v->meshesnum+1)*sizeof(CGLTFLOADER_MESH));
          memset(v->meshes, 0, v->meshesnum*sizeof(CGLTFLOADER_MESH));
      }
      else if(v->meshespos+num >= v->meshesnum)
      {
          unsigned int tmp = v->meshesnum;
          v->meshesnum += num;
          v->meshes = realloc(v->meshes, (v->meshesnum+1)*sizeof(CGLTFLOADER_MESH));
          memset(&(v->meshes[tmp]), 0, num*sizeof(CGLTFLOADER_MESH));
      }
      
}

CRESULT cgltfloader_getscalarvalues(CGLTFLOADER *v, float *outScalars, unsigned int inComponentCount, const cgltf_accessor *inAccessor) 
{
	for (cgltf_size i = 0; i < inAccessor->count; i++) {
		cgltf_accessor_read_float(inAccessor, i, &outScalars[i * inComponentCount], inComponentCount);
	}
}


CRESULT cgltfloader_meshfromattribute(CGLTFLOADER *v, cgltf_attribute *attribute, cgltf_skin *skin, cgltf_node *nodes, unsigned int nodeCount) 
{
	cgltf_attribute_type attribType = attribute->type;
	cgltf_accessor accessor = *attribute->data;

	unsigned int componentCount = 0;
	if (accessor.type == cgltf_type_vec2) 
	{
		componentCount = 2;
	}
	else if (accessor.type == cgltf_type_vec3) 
	{
		componentCount = 3;
	}
	else if (accessor.type == cgltf_type_vec4) 
	{
		componentCount = 4;
	}	
	unsigned int acessorCount = (unsigned int)accessor.count;
	
	switch (attribType) 
	{
		case cgltf_attribute_type_position:
	                if(v->meshes[v->meshespos].positions)
	                    break;
	                v->meshes[v->meshespos].positions = malloc(sizeof(float)*componentCount*acessorCount);
	                cgltfloader_getscalarvalues(v, v->meshes[v->meshespos].positions, componentCount, &accessor);
	                break;  
		case cgltf_attribute_type_texcoord:
	                if(!v->meshes[v->meshespos].texcoords)
	                    break;
	                v->meshes[v->meshespos].texcoords = malloc(sizeof(float)*componentCount*acessorCount);
	                cgltfloader_getscalarvalues(v, v->meshes[v->meshespos].texcoords, componentCount, &accessor);
      			break;
		case cgltf_attribute_type_weights:
	                if(!v->meshes[v->meshespos].weights)
	                    break;
	                v->meshes[v->meshespos].weights = malloc(sizeof(float)*componentCount*acessorCount);
	                cgltfloader_getscalarvalues(v, v->meshes[v->meshespos].weights, componentCount, &accessor);
                        break;
		case cgltf_attribute_type_normal:
	                if(!v->meshes[v->meshespos].normals)
	                    break;
                        v->meshes[v->meshespos].normals = malloc(sizeof(float)*componentCount*acessorCount);
	                cgltfloader_getscalarvalues(v, v->meshes[v->meshespos].normals, componentCount, &accessor);
	                break;
		case cgltf_attribute_type_joints:
		        
		        break;
	}
	v->meshes[v->meshespos].count = acessorCount * componentCount;
}




CRESULT cgltfloader_loadmeshes(CGLTFLOADER *v, cgltf_data* data) 
{
	int loop = 1;
	cgltf_node* nodes = data->nodes;
	unsigned int nodeCount = (unsigned int)data->nodes_count;

	for (unsigned int i = 0; i < nodeCount && loop; i++) 
	{
	        cgltf_node* node = &nodes[i];
                unsigned int numPrims = (unsigned int)node->mesh->primitives_count;
		cgltfloader_allocatemeshes(v, numPrims);
		for (unsigned int j = 0; j < numPrims && loop; j++) 
		{
			cgltf_primitive* primitive = &(node->mesh->primitives[j]);

			unsigned int numAttributes = (unsigned int)primitive->attributes_count;
			for(unsigned int k = 0; k < numAttributes; k++) 
			{
				cgltf_attribute* attribute = &(primitive->attributes[k]);
				cgltfloader_meshfromattribute(v, attribute, node->skin, node, nodeCount);
			}
			if(primitive->indices != 0) 
			{
				v->meshes[v->meshespos].indicescount = (unsigned int)primitive->indices->count;
			        v->meshes[v->meshespos].indices = malloc(v->meshes[v->meshespos].indicescount * sizeof(unsigned int));
				for (unsigned int k = 0; k < v->meshes[v->meshespos].indicescount; k++) 
				{
                                      v->meshes[v->meshespos].indices[k] = cgltf_accessor_read_index(primitive->indices, k);       
				}
			}
			else
			{
			  v->meshes[v->meshespos].indicescount = 0;
			}
		    
		        v->meshespos++;
			if(v->meshespos >= v->meshesnum)
			{
			    loop = 0;
			}
		}
	      

	}
}



CRESULT cgltfloader_create(CGLTFLOADER *v, char *filename)
{
	cgltf_options options;
	memset(&options, 0, sizeof(cgltf_options));
	cgltf_data* data = NULL;
	cgltf_result result = cgltf_parse_file(&options, filename, &data);

	v->meshes = NULL;

	if (result == cgltf_result_success)
		result = cgltf_load_buffers(&options, data, filename);

	if (result == cgltf_result_success)
		result = cgltf_validate(data);

	printf("Result: %d\n", result);

        if (result == cgltf_result_success)
	{
		printf("Type: %u\n", data->file_type);
		printf("Meshes: %u\n", (unsigned)data->meshes_count);
		
		cgltfloader_loadmeshes(v, data);
	}
	
	return result == cgltf_result_success ? CSUCCESS : CFAILED;
}

void cgltfloader_destroy(CGLTFLOADER *v)
{
    if(v->meshes)
    {
            for(int i = 0; i < v->meshesnum; i++)
            {
                if(v->meshes[i].positions)
                {
                    free(v->meshes[i].positions);
                    v->meshes[i].positions = NULL;
                }
                if(v->meshes[i].texcoords)
                {
                    free(v->meshes[i].texcoords);
                    v->meshes[i].texcoords = NULL;
                }
                if(v->meshes[i].weights)
                {
                    free(v->meshes[i].weights);
                    v->meshes[i].weights = NULL;
                }
                if(v->meshes[i].normals)
                {
                    free(v->meshes[i].normals);
                    v->meshes[i].normals = NULL;
                }
            }
            free(v->meshes);
            v->meshes = NULL;
    }
}


