CXX ?= g++
CPPFLAGS += -I../include -I./include -I../rmidevice
CXXFLAGS += -Wall
LDFLAGS += -L.
LIBS =  -lrmidevice
LIBDIR = ../rmidevice
LIBNAME = librmidevice.a
RMI4TOOLSRC = main.cpp
RMI4TOOLOBJ = $(RMI4TOOLSRC:.cpp=.o)
PROGNAME = rmi4tool
STATIC_BUILD ?= y
ifeq ($(STATIC_BUILD),y)
LDFLAGS += -static
endif

all: $(PROGNAME)

$(PROGNAME): $(RMI4TOOLOBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(RMI4TOOLOBJ) -L$(LIBDIR) $(LIBS) -o $(PROGNAME)

clean:
	rm -f $(RMI4TOOLOBJ) $(PROGNAME)
