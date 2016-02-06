#Object files path
#OPATH = /tmp
OPATH = tmp

#Compilers
CPP=g++

#Location of LavaVu build/source
LVPATH=../LavaVu
SRCDIR=src

#Default flags
CFLAGS = -I${SRCDIR} -I${LVPATH}/src

#Separate compile options per configuration
ifeq ($(CONFIG),debug)
  CFLAGS += -g -O0
else
  CFLAGS += -O3
endif

#Linux/Mac specific libraries/flags
OS := $(shell uname)

ifeq ($(OS), Darwin)
  #Mac OS X interactive with GLUT
  CFLAGS += -FGLUT -FOpenGL -I/usr/include/malloc
  LIBS=-ldl -lpthread -framework GLUT -framework OpenGL -lobjc -lm -lz
  DEFINES += -DHAVE_GLUT
else
  #Linux interactive with X11 (and optional GLUT, SDL)
  LIBS=-ldl -lpthread -lm -lGL -lz -lX11
  DEFINES += -DHAVE_X11
ifeq ($(GLUT), 1)
  LIBS+= -lglut
  DEFINES += -DHAVE_GLUT
endif
ifeq ($(SDL), 1)
  LIBS+= -lSDL
  DEFINES += -DHAVE_SDL
endif
endif

#Add a libpath (useful for linking specific libGL)
ifdef LIBDIR
  LIBS+= -L$(LIBDIR) -Wl,-rpath=$(LIBDIR)
endif

#Other optional components
ifeq ($(VIDEO), 1)
  CFLAGS += -DHAVE_LIBAVCODEC
  LIBS += -lavcodec -lavutil -lavformat
endif
ifeq ($(PNG), 1)
  CFLAGS += -DHAVE_LIBPNG
  LIBS += -lpng
else
  CFLAGS += -DUSE_ZLIB
endif

#Source search paths
vpath %.cpp ${SRCDIR}

SRC := $(wildcard ${SRCDIR}/*.cpp)
INC := $(wildcard ${SRCDIR}/*.h)
#INC := $(SRC:%.cpp=%.h)
OBJ := $(SRC:%.cpp=%.o)
#Strip paths (src) from sources
OBJS = $(notdir $(OBJ))
#Add object path
OBJS := $(OBJS:%.o=$(OPATH)/%.o)

PROGRAM = FractalVu

default: install

install: paths $(PROGRAM)

paths:
	mkdir -p $(OPATH)

#Rebuild *.cpp
$(OBJS): $(OPATH)/%.o : %.cpp $(INC)
	$(CPP) $(CFLAGS) $(DEFINES) -c $< -o $@

$(PROGRAM): paths $(OBJS)
	$(CPP) -o $(PROGRAM) $(OBJS) $(LIBS) ${LVPATH}/bin/libLavaVu.so

clean:
	/bin/rm -f *~ $(OPATH)/*.o $(PROGRAM)

