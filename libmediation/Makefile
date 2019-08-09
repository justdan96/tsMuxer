BUILD=

ifeq ($(MAKECMDGOALS),debug) 
    BUILD=
endif 
ifeq ($(MAKECMDGOALS),release) 
    BUILD=-Os
endif 
#ifeq (!$(findstring $(BUILD),debug release)) 
#    $(error BUILD=$(BUILD) is unknown, must be BUILD=release or BUILD=debug) 
#endif

ifdef OS
   INCPATH  = -I.
   DEFINES  = -DWIN32 -DLIBMEDIATION_API= -DDISABLE_LOG_SUPPORT -DDEBUG_NEW12=new -DNO_SYSTEM_EXCEPT
   SOURCES= fs/directory.cpp fs/osdep/file_win32.cpp system/osdep/condvar_win32.cpp system/osdep/mutex_win32.cpp \
system/osdep/terminatablethread_win32.cpp types/types.cpp time/time.cpp
   CFLAGS   = -c -m32
   LFLAGS   = -m32
else
   INCPATH  = -I./
   DEFINES  = -DLINUX -DLIBMEDIATION_API= -DDISABLE_LOG_SUPPORT -DDEBUG_NEW12=new -DNO_SYSTEM_EXCEPT
   SOURCES= fs/directory.cpp fs/osdep/file_unix.cpp system/osdep/condvar_linux.cpp system/osdep/mutex_linux.cpp \
system/osdep/terminatablethread_linux.cpp types/types.cpp time/time.cpp
   UNAME_P := $(shell uname -p)
   ifneq ($(filter arm%,$(UNAME_P)),)
      #arm
      CFLAGS   = -c
      LFLAGS   =
   else
      CFLAGS   = -c -m32
      LFLAGS   = -m32
   endif
endif

CC=gcc

LIBS     = -lpthread
LDFLAGS  =
TARGET   = libmediationl.a

OBJECTS=$(SOURCES:.cpp=.o)

all: $(SOURCES) $(TARGET)
.PHONY : debug release 
debug: all 
release: all

$(TARGET): $(OBJECTS)
	ar cr $@ $(OBJECTS)
        
.cpp.o: 
	$(CC) $(DEFINES) $(BUILD) $(LFLAGS) $(CFLAGS)  $(INCPATH) $? $(LDFLAGS) -o $@ 
	
clean:
	rm -f $(OBJECTS)
	rm -f ./libmediation.so
	rm -f ./libmediationl.a
                
