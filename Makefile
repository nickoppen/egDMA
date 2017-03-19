
CC = gcc
DEFS += -DDEBUG
ESDK=${EPIPHANY_HOME}/tools/host

CCFLAGS += -g $(DEFS)
INCS = -I. -I/usr/local/browndeer/coprthr2/include -I${ESDK}/include
LIBS = -L/usr/local/browndeer/coprthr2/lib -lcoprthr -lcoprthrcc -lm -ldl -L${ESDK}/lib

COPRCC = /usr/local/browndeer/coprthr2/bin/coprcc
COPRCC_FLAGS = -g --info
COPRCC_DEFS = $(DEFS) -DCOPRTHR_MPI_COMPAT
COPRCC_INCS = $(INCS)
COPRCC_LIBS = -L/usr/local/browndeer/coprthr2/lib \
	-lcoprthr_hostcall -lcoprthr_mpi -lcoprthr2_dev -lesyscall

TARGET = egdma.x egdma.e32

all: $(TARGET)
Debug: egdma.x
COPRTHR2: egdma.e32

.PHONY: clean install uninstall $(SUBDIRS)

.SUFFIXES: .c .o .x .e32

egdma.x: egdma.o
	$(CC) -o egdma.x egdma.o $(LIBS)

egdma.e32: e_egdma.c egdma.h 
	$(COPRCC) $(COPRCC_FLAGS) $(COPRCC_LIBS) $(COPRCC_INCS) $(COPRCC_DEFS)  -o egdma.e32 $<

egdma.o: egdma.c egdma.h
	$(CC) $(CCFLAGS) $(INCS) -c egdma.c -o egdma.o

cleanDebug: $(SUBDIRS)
	rm -f *.o
	rm -f *.x

cleanCL: $(SUBDIRS)
	rm -f *.e32

cleanall: cleanDebug cleanCL
distclean: clean


