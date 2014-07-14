CXX = g++
CPPFLAGS = -I../include -I./include -I../rmidevice
CXXFLAGS = -g -Wall
LDFLAGS = -static -L.
LIBS =  -lrmidevice
LIBDIR = ../rmidevice
LIBNAME = librmidevice.a
RMI4UPDATESRC = main.cpp firmware_image.cpp rmi4update.cpp updateutil.cpp
RMI4UPDATEOBJ = $(RMI4UPDATESRC:.cpp=.o)
PROGNAME = rmi4update

all: $(PROGNAME)

$(PROGNAME): $(RMI4UPDATEOBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(RMI4UPDATEOBJ) -L$(LIBDIR) $(LIBS) -o $(PROGNAME)

clean:
	rm -f $(RMI4UPDATEOBJ) $(PROGNAME)
