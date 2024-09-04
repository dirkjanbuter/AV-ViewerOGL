CC = gcc -g 
SRCS = main.c log.c filter.c cgltfloader.c
FILES = $(addprefix src/, $(SRCS))
OBJS = $(FILES:.c=.o)
LIBS = -lSDL2 -lGL -lGLEW -ldl -lm -lglut
CFLAGS = -I/usr/include/SDL2 

%.o: %.c $(FILES)
	$(CC) -c -o $@ $< $(CFLAGS) -fPIC 

build: $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o ./../build/av-viewerogl

dist: build
		rm $(OBJS)

clean:
		rm $(OBJS)
