CXX = g++
LD = $(CXX)
CPPFLAGS = -I../include -I./include -I../rmidevice
CXXFLAGS = -g -Wall
LDFLAGS = -static -L.
LIBS =  -lrmidevice
LIBDIR = ../rmidevice
LIBNAME = librmidevice.a
RMIHIDTOOLSRC = main.cpp
RMIHIDTOOLOBJ = $(RMIHIDTOOLSRC:.cpp=.o)
PROGNAME = rmihidtool

all: $(PROGNAME)

$(PROGNAME): $(RMIHIDTOOLOBJ)
	$(LD) $(LDFLAGS) $(RMIHIDTOOLOBJ) -L$(LIBDIR) $(LIBS) -o $(PROGNAME)

clean:
	rm -f $(RMIHIDTOOLOBJ) $(PROGNAME)
