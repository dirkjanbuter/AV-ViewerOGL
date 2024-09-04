#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <GL/glew.h>
#include <SDL3/SDL_opengl.h>
#include <GL/glu.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <GL/glut.h>
#include <cglm/cglm.h>

#include "filter.h"
#include "cgltfloader.h"
#include "avatar-obj.h"

#define SCREEN_FPS      30;
#define SCREEN_WIDTH   1280;
#define SCREEN_HEIGHT  720;

int init();
int initGL();
void handleKeys(unsigned char key);
void update();
void render();
void destroy();
void printprogramlog(GLuint program);
void printShaderLog(GLuint shader);
void audiocallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount);

int _width = SCREEN_WIDTH;
int _height = SCREEN_HEIGHT;
float _fps = SCREEN_FPS;
SDL_AudioStream *_audiostream;
SDL_AudioSpec _audiospec;
SDL_Window* _window = NULL;
SDL_GLContext _context;
int _renderquad = 1;
GLuint _programid = 0;
GLint _vertexPos3dloc = -1;
FILTER _filter[16];
int _filterNum = 0;
int _colors[16];
u_int8_t *_backbuffer = NULL;
float _sample[2048];

double _vframe = 0;
double _aframe = (1.0/44100.0)*1024.0; // 0.02322
double _aelapsed = 0.0;
double _velapsed = 0.0;
int _framecount=0;

GLuint _vbo[12];
GLuint _ibo[12];

SDL_GLContext _glcontext;
GLuint _vertexShader;
mat4 _viewmatrix;
mat4 _projectionmatrix;
mat4 _modelmatrix;
unsigned int frametexture;
GLuint _vertexarrayid;

float _distance = 0;

GLuint fboId = 0;


void logc(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
        printf("[Log] %s", message);
}

void audiocallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
        SDL_zero(_sample);
        for(int i = 0; i < _filterNum; i++)
        {
            filter_audio(&_filter[i], _sample, _framecount, _aelapsed);
        }
        for(int i = 0; i < 2048; i++) 
        {
            _sample[i] = i%2 ? _sample[1024+i/2] : _sample[1024+i/2];
        }
        SDL_PutAudioStreamData(stream, _sample, sizeof(_sample));
        _aelapsed += _aframe;
}

CRESULT video(char* argv[], int64_t _framecount)
{
	int x, y, i;


	for(i = 0; i < _filterNum; i++)
	{
		if(filter_video(&_filter[i], _backbuffer, _width, _height, _colors[i], argv[6+(i*3)], _framecount) == CFAILED)
			return CFAILED;
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, frametexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _width, _height, 0, GL_BGRA, GL_UNSIGNED_BYTE, _backbuffer);
	
	_velapsed+=_vframe;
	return CSUCCESS;
}


int init()
{
       
        //Initialize SDL
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
	{
		SDL_Log("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
		return 0;
	}
	
        //Use OpenGL 3.1 core
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	
	/*Set the colour depth (16 bit 565). */
//SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
//SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
//SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5); 

//Make sure we get an accelerated renderer.
SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

//SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     8);
//SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   8);
//SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    8);
//SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   24);
//SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
//SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl"); 

        



        //Create window
	_window = SDL_CreateWindow("AV-ViewerOGL 1.0 | AGPL-3.0, Dirk Jan Buter, https://dirkjanbuter.com/", _width, _height, SDL_WINDOW_OPENGL);
	if(_window == NULL)
	{
		SDL_Log("Window could not be created! SDL Error: %s\n", SDL_GetError());
	        return 0;
	}
	SDL_SetWindowPosition(_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	_glcontext = SDL_GL_CreateContext(_window);
	SDL_GL_MakeCurrent(_window, _glcontext);
	
	
	//SDL_SetWindowFullscreen(_window, true);
	
	SDL_zero(_audiospec);
	_audiospec.freq = 44100;
	_audiospec.channels = 2;
	_audiospec.format = SDL_AUDIO_F32;
	_audiostream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &_audiospec, audiocallback, NULL);
	
        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(_audiostream));
	
	//Create context
	_context = SDL_GL_CreateContext(_window);
	if(_context == NULL)
	{
		SDL_Log("OpenGL context could not be created! SDL Error: %s\n", SDL_GetError());
	        return 0;
	}

        //Initialize GLEW
	glewExperimental = GL_TRUE; 
	GLenum glewError = glewInit();
	if(glewError != GLEW_OK)
	{
		SDL_Log("Error initializing GLEW! %s\n", glewGetErrorString(glewError));
	}
  
	//Use Vsync
	if(SDL_GL_SetSwapInterval(0) < 0)
	{
		SDL_Log("Warning: Unable to set VSync! SDL Error: %s\n", SDL_GetError());
	}

	//Initialize OpenGL
	if(!initGL())
	{
		SDL_Log("Unable to initialize OpenGL!\n");
                return 0;
        }
  
	return 1;
}

int initGL()
{	

glEnable(GL_DEPTH_TEST);
glDepthFunc(GL_LESS);
//glDepthFunc(GL_LEQUAL);
glClearDepth(1.0f);

//glEnable(GL_COLOR_MATERIAL);
//glColorMaterial(GL_FRONT, GL_DIFFUSE);
//glColor3f(0.3, 0.5, 0.6);

//glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

glGenVertexArrays(1, &_vertexarrayid);
glBindVertexArray(_vertexarrayid);



	//Generate program
	_programid = glCreateProgram();

	//Create vertex shader
	_vertexShader = glCreateShader(GL_VERTEX_SHADER);

	//Get vertex source
	const GLchar* vertexShaderSource[] =
	{
		"#version 330\nuniform mat4 projectionMatrix; uniform mat4 modelMatrix; uniform mat4 viewMatrix; layout (location = 0) in vec3 inPosition; layout (location = 1) in vec3 inColor; smooth out vec3 theColor; void main() { 	gl_Position = projectionMatrix*modelMatrix*viewMatrix*vec4(inPosition, 1.0); theColor = inColor; }"
	};

	//Set vertex source
	glShaderSource(_vertexShader, 1, vertexShaderSource, NULL);

	//Compile vertex source
	glCompileShader(_vertexShader);

	//Check vertex shader for errors
	GLint vShaderCompiled = GL_FALSE;
	glGetShaderiv(_vertexShader, GL_COMPILE_STATUS, &vShaderCompiled);
	if(vShaderCompiled != GL_TRUE)
	{
		SDL_Log("Unable to compile vertex shader %d!\n", _vertexShader);
		printShaderLog(_vertexShader);
                return 0;
	}

              //Attach vertex shader to program
	glAttachShader(_programid, _vertexShader);


	//Create fragment shader
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	//Get fragment source
	const GLchar* fragmentShaderSource[] =
	{
		"#version 330\nsmooth in vec3 theColor; out vec4 outputColor; void main() { outputColor = vec4(theColor, 1.0); }"
	};

	//Set fragment source
	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);

	//Compile fragment source
	glCompileShader(fragmentShader);

	//Check fragment shader for errors
	GLint fShaderCompiled = GL_FALSE;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fShaderCompiled);
	if(fShaderCompiled != GL_TRUE)
	{
		SDL_Log("Unable to compile fragment shader %d!\n", fragmentShader);
		printShaderLog(fragmentShader);
		return 0;
	}
	
	//Attach fragment shader to program
	glAttachShader(_programid, fragmentShader);

	//Link program
	glLinkProgram(_programid);

	//Check for errors
	GLint programSuccess = GL_FALSE;
	glGetProgramiv(_programid, GL_LINK_STATUS, &programSuccess);
	if(programSuccess != GL_TRUE)
	{
		SDL_Log("Error linking program %d!\n", _programid);
		printprogramlog(_programid);
		return 0;
	}


/*{
GLfloat gray[] = {0.75f, 0.75f, 0.75f, 1.0f};
GLfloat lightPos[] = {-50.0f, 50.0f, 100.0f, 1.0f};
glEnable (GL_LIGHTING);
glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, gray);
glEnable (GL_LIGHT0);
glLightfv (GL_LIGHT0, GL_POSITION, lightPos);
}*/


//Initialize clear color
glClearColor(0.f, 0.f, 0.f, 1.f);

    	glEnable(GL_TEXTURE_2D);
	

        for(int i = 0; i < 11; i++)
        {
	          glGenBuffers(1, &_vbo[i]);
	          glBindBuffer(GL_ARRAY_BUFFER, _vbo[i]);
                  glBufferData(GL_ARRAY_BUFFER, sizeof(_vertices[i]), _vertices[i], GL_STATIC_DRAW);
	          glGenBuffers(1, &_ibo[i]);
                  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo[i]);
	          glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(_indices[i]), _indices[i], GL_STATIC_DRAW);
        }	

	//glBlendFunc(GL_DST_COLOR,GL_ONE);
        //glEnable(GL_BLEND);



	// start init render from texture
	glViewport(0, 0, _width, _height);
	
	glGenTextures(1, &frametexture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, frametexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _width, _height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);	

 	glGenFramebuffers(1, &fboId);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboId);
	glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, frametexture, 0);
	
	
	if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
 	{
 	
 	
 	}
 	
 	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
 
 	// end init render from texture
 	
 /*	
 	GLfloat colors[24] = { // Random colors
    1.0f,  1.0f, 1.0f, 1.0f,
    1.0f,  0.0f, 1.0f, 1.0f,
    1.0f,  1.0f, 0.0f, 1.0f,
    0.0f,  1.0f, 0.0f, 1.0f,
    1.0f,  1.0f, 1.0f, 1.0f,
    0.0f,  0.0f, 1.0f, 1.0f,
};

GLuint color_buffer;
glGenBuffers(1, &color_buffer);
glBindBuffer(GL_ARRAY_BUFFER, color_buffer);
glBufferData(GL_ARRAY_BUFFER, 6 * 4 * sizeof(float), colors, GL_STATIC_DRAW);
glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4, 0); //vertex colors
glEnableVertexAttribArray(1);
 */	
 	
	return 1;
}

void handleKeys(unsigned char key)
{
	//Toggle quad
	if(key == 'a')
	{
		_renderquad = !_renderquad;
	}
	if(key == 'w')
	{
		_distance -= 10.f;
	}
	if(key == 's')
	{
		_distance += 10.f;
	}
}

void update()
{
	//No per frame update needed
}

void render()
{
        GLfloat MaterialColor[] = {0.1f, 0.5f, 1.0f, 1.0f};
  	float verts[] = {
	    0, 0,
	    1.0f, 0,
	    0, 1.0f,
	    1.0f, 1.0f,
	};

	const float texcoord[] = {
	    0, 0,
	    1.f, 0,
	    0 , 1.f,
	    1.f, 1.f
	};
  	
  	//glClearColor(0.f, 1.f, 0.f, 1.f);
  	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  	
  	// start draw frame texture
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  	if(_renderquad)
  	{
		glBlitFramebuffer(0, 0, _width, _height, 0, _height, _width, 0, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
  		
  	}
 	// end draw frame texture
 	
 	
	{

             glm_mat4_identity(_viewmatrix);              
             glm_lookat((vec3){0.0f, 0.0f, _distance+3.0f}, (vec3){0.0f, 0.0f, _distance}, (vec3){0.0f, 1.0f, 0.0f}, _viewmatrix);
              
              glm_mat4_identity(_projectionmatrix);
	      glm_perspective(45.0f, _width / _height, 0.1f, 1000.0f, _projectionmatrix);
	     
              
              glUniformMatrix4fv(glGetUniformLocation(_programid, "viewMatrix"), 1, GL_FALSE, (const GLfloat *)_viewmatrix); 				
	      glUniformMatrix4fv(glGetUniformLocation(_programid, "projectionMatrix"), 1, GL_FALSE, (const GLfloat *)_projectionmatrix); 				


	      glm_mat4_identity(_modelmatrix);
	      glm_scale(_modelmatrix, (vec3){5.f, 5.f, 5.f});
	      //glm_rotate_y(_modelmatrix, _framecount/1.f, _modelmatrix);
              //glm_rotate_x(_modelmatrix, _framecount/1.f, _modelmatrix);
              //glm_rotate_z(_modelmatrix, _framecount/1.f, _modelmatrix);
              glm_translate(_modelmatrix, (vec3){0.f, 0.f, -10.f});

 		glUniformMatrix4fv(glGetUniformLocation(_programid, "modelMatrix"), 1, GL_FALSE, (const GLfloat *)_modelmatrix); 				


	     glUseProgram(_programid);

	
		//Enable vertex position
		glEnableVertexAttribArray(0);
   
	   
      glColor3f(255.0, 0.0,0.0);
      glutSolidSphere(1.0, 22, 22);
      glutSolidTeapot(1);
   /*
                for(int i = 0; i < 11; i++)
                {
		    glBindBuffer(GL_ARRAY_BUFFER, _vbo[i]);
		    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		    //Set index data and render
		    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo[i]);
		    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
                }
     */           
		//Disable vertex position
		glDisableVertexAttribArray(0);

		//Unbind program
		glUseProgram(0);
	}
 }

void destroy()
{
	//Deallocate program
	glDeleteProgram(_programid);

	glDeleteVertexArrays(1, &_vertexarrayid);

	glDeleteTextures(1, &frametexture);
	glDeleteFramebuffers(1, &fboId);  

	//Destroy window	
	SDL_DestroyWindow(_window);
	_window = NULL;
	
	SDL_Quit();
}

void printprogramlog( GLuint program )
{
	//Make sure name is shader
	if(glIsProgram(program))
	{
		//Program log length
		int infoLogLength = 0;
		int maxLength = infoLogLength;
		
		//Get info string length
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
		
		//Allocate string
		char* infoLog = malloc(maxLength);
		
		//Get info log
		glGetProgramInfoLog(program, maxLength, &infoLogLength, infoLog);
		if(infoLogLength > 0)
		{
			//Print Log
			SDL_Log("%s\n", infoLog);
		}
		
		//Deallocate string
		if(infoLog) free(infoLog);
		infoLog = NULL;
	}
	else
	{
		SDL_Log("Name %d is not a program\n", program);
	}
}

void printShaderLog(GLuint shader)
{
	//Make sure name is shader
	if(glIsShader(shader))
	{
		//Shader log length
		int infoLogLength = 0;
		int maxLength = infoLogLength;
		
		//Get info string length
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
		
		//Allocate string
		char* infoLog = malloc(maxLength);
		
		//Get info log
		glGetShaderInfoLog(shader, maxLength, &infoLogLength, infoLog);
		if( infoLogLength > 0 )
		{
			//Print Log
			SDL_Log("%s\n", infoLog);
		}

		//Deallocate string
		if(infoLog) free(infoLog);
		infoLog = NULL;
	}
	else
	{
		SDL_Log("Name %d is not a shader\n", shader);
	}
}

int main(int argc, char* argv[])
{
	SDL_SetLogOutputFunction(&logc, NULL);
	
	if(argc < 6)
	{
		printf("Usage: %s [WIDTH] [HEIGHT] [FPS] [filter1.so] [COLOR1] [ARGUMENT1] [filter2.so] [COLOR2] [ARGUMENT2]...\r\n", argv[0]);
		return 1;
	}
	
	_width = atoi(argv[1]);
	_height = atoi(argv[2]);
	_fps = (float)atoi(argv[3]);

	_vframe = 1.0/_fps;

	_filterNum = (argc-4)/3;

	for(int i = 0; i < _filterNum; i++)
	{
		_colors[i] = (int)strtol(argv[5+(i*3)], NULL, 16);
		if(filter_create(&_filter[i], argv[4+(i*3)], _fps) == CFAILED)
		{
			printf("Error: opening filter failed\r\n");
			return 1;
		}
	}
	_backbuffer = (Uint8*)malloc(_width * _height * 4);
		
	//Start up SDL and create window
	if(!init())
	{
            SDL_Log("Failed to initialize!\n");
            return 1;
        }
      
        glutInit(&argc, argv);
        
	int quit = 0;

	SDL_Event e;
	
	SDL_StartTextInput(_window);

	while(!quit)
	{
		//Handle events on queue
		while(SDL_PollEvent(&e) != 0)
		{
			//User requests quit
			if(e.type == SDL_EVENT_QUIT)
			{
				quit = 1;
			}
			//Handle keypress
			else if(e.type == SDL_EVENT_TEXT_INPUT)
			{
				handleKeys(e.text.text[0]);
			}
		}

  
                if(_velapsed < _aelapsed)
		{
		    if(video(argv, _framecount++) == CFAILED)
		    {
		        quit = 1;
		    }
		    
		    render();

		    //Update screen
		    SDL_GL_SwapWindow(_window);

		}
	}
	
	//Disable text input
	SDL_StopTextInput(_window);

        destroy();
        
	for(int i = 0; i < _filterNum; i++)
	{
		filter_destroy(&_filter[i]);
	}
	if(_backbuffer) 
		free(_backbuffer);

	return 0;
}
