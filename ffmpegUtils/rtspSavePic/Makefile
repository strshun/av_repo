CXX   =  g++ -std=c++11
CXXFLAGS = -Wall -o2

#包含头文件路徿
SUBDIR   = $(shell ls ./src -R | grep /)
SUBDIRS  = $(subst :,/,$(SUBDIR))
INCPATHS = $(subst ./,-I./,$(SUBDIRS))
INCPATHS += -I./ -I./include/ -I/usr/local/ffmpeg/include/

VPATH = $(subst : ,:,$(SUBDIR))./
SOURCE = $(foreach dir,$(SUBDIRS),$(wildcard $(dir)*.cpp))

#将cpp的路径名称的.cpp后缀换为.o
OBJS = $(patsubst %.cpp,%.o,$(SOURCE))
#取出.o的文件名，不带路徿
OBJFILE  = $(foreach dir,$(OBJS),$(notdir $(dir)))
#.o输出刿OBJ目录丿
OBJSPATH = $(addprefix obj/,$(OBJFILE)) 

LIBPATH = ./lib/ -L/usr/local/ffmpeg/lib/
LIBS = -Wl,-rpath=./:./lib -lhttpget -lavformat -lavcodec -lavutil -lpthread -lswresample
EXE = ./iRtspSavePic

$(EXE):$(OBJFILE)
	$(CXX) -L$(LIBPATH)  -o $(EXE) $(OBJFILE) $(INCPATHS) $(LIBS)

$(OBJFILE):%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<  $(INCPATHS) -pipe

#依赖文件
DPPS = $(patsubst %.cpp,%.dpp,$(SOURCE))
include $(DPPS)
%.dpp: %.cpp	
	g++ $(INCPATHS) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$ 
	

.PHONY:clean
clean:
	rm -rf $(OBJFILE)
	rm -rf $(DPPS)
	rm -rf $(EXE)

