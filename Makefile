#Object files path
OPATH = tmp

#Compilers
CPP=g++

#Location of LavaVu build/source
LVINC?=${HOME}/Dropbox/LavaVu/src
LVLIB?=.
LV_LIB=$(realpath $(LVLIB))
SRCDIR=src

#Default flags
CFLAGS = -std=c++11 -I${SRCDIR} -I${LVINC} -DUSE_MIDI

#Separate compile options per configuration
ifeq ($(CONFIG),debug)
  CFLAGS += -g -O0
else
  CFLAGS += -O3
endif

#Linux/Mac specific libraries/flags
OS := $(shell uname)

LIBS=-ldl -lpthread -lm -lGL -lz

#Add a libpath (useful for linking specific libGL)
ifdef LIBDIR
  LIBS+= -L$(LIBDIR) -Wl,-rpath=$(LIBDIR)
endif

#Other optional components
ifeq ($(VIDEO), 1)
  CFLAGS += -DHAVE_LIBAVCODEC -DHAVE_SWSCALE
  LIBS += -lavcodec -lavutil -lavformat -lswscale
endif
#Default libpng disabled, use built in png support
LIBPNG ?= 0
ifeq ($(LIBPNG), 1)
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
	$(CPP) -o $(PROGRAM) $(OBJS) $(LIBS) -L${LV_LIB} -Wl,-rpath=${LV_LIB} -lLavaVu

clean:
	-rm -f *~ $(OPATH)/*.o $(PROGRAM)

