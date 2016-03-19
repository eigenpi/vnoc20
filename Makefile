CC = g++

CLASSDIR = /home/cristinel/noc/vnoc2
INCDIRS = $(CLASSDIR)/include
POWER_RELEASE = orion3

LIB_DIR = -L/usr/X11R6/lib
LIB = -lm -lX11
X11_INCLUDE = -I/usr/X11R6/include

WARN_FLAGS = -Wall -Wpointer-arith -Wcast-qual -Wstrict-prototypes -O -D__USE_FIXED_PROTOTYPES__ -ansi -pedantic -Wmissing-prototypes -Wshadow -Wcast-align -D_POSIX_SOURCE
DEBUG_FLAGS = -g -I./$(POWER_RELEASE)
OPT_FLAGS = -O3 -I./$(POWER_RELEASE)


#FLAGS = $(WARN_FLAGS)
#FLAGS = $(DEBUG_FLAGS) 
FLAGS = $(OPT_FLAGS)
FLAGS += $(addprefix -I, $(INCDIRS))

LINKFLAGS = -L./$(POWER_RELEASE) -lm -lpower 

EXE = vnoc
PEXE = power_model

OBJ = vnoc_topology.o vnoc_utils.o vnoc_event.o vnoc.o vnoc_router.o vnoc_main.o vnoc_gui.o 
SRC = vnoc_topology.cpp vnoc_utils.cpp vnoc_event.cpp vnoc_router.cpp vnoc.cpp vnoc_main.cpp vnoc_gui.cpp 
H = include/vnoc_topology.h include/vnoc_utils.h include/vnoc_event.h include/vnoc_router.h \
	include/vnoc.h include/vnoc_gui.h include/vnoc_predictor.h include/vnoc_pareto.h 


$(EXE): $(OBJ) $(PEXE)
	$(CC) $(FLAGS) $(OBJ) -o $(EXE) $(LIB_DIR) $(LIB) $(LINKFLAGS)

$(PEXE):
	cd ./$(POWER_RELEASE); $(MAKE)

vnoc_topology.o: vnoc_topology.cpp $(H)
	$(CC) -c $(FLAGS) vnoc_topology.cpp

vnoc_event.o: vnoc_event.cpp $(H)
	$(CC) -c $(FLAGS) vnoc_event.cpp

vnoc_utils.o: vnoc_utils.cpp $(H)
	$(CC) -c $(FLAGS) vnoc_utils.cpp

vnoc_router.o: vnoc_router.cpp $(H)
	$(CC) -c $(FLAGS) vnoc_router.cpp

vnoc.o: vnoc.cpp $(H)
	$(CC) -c $(FLAGS) vnoc.cpp

vnoc_main.o: vnoc_main.cpp $(H)
	$(CC) -c $(FLAGS) vnoc_main.cpp

vnoc_gui.o: vnoc_gui.cpp $(H)
	$(CC) -c $(FLAGS) $(X11_INCLUDE) vnoc_gui.cpp
