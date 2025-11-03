CXX	= g++
LD	= g++
LD	= g++
WXCONFIG	= wx-config

CXXFLAGS	= `$(WXCONFIG) --cxxflags` -std=c++2a -Wall -Wextra
CPPFLAGS	= `$(WXCONFIG) --cppflags`

EXEC		:= $(notdir $(CURDIR)).a
LIBS		:= `$(WXCONFIG) --libs xrc,propgrid,aui,adv,core,base,xml` -lpng -lyaml-cpp
SRCDIR  	:= landstalker/src
BUILDDIR	:= build
BINDIR		:= bin
INC_DIRS	:= landstalker/include
INCS		:= $(SRCDIR) $(INC_DIRS)

SRC		:= $(foreach sdir,$(SRCDIR),$(wildcard $(sdir)/*/*.cpp))
OBJ		:= $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRC))
INCLUDES	:= $(addprefix -I,$(INCS))

vpath %.cpp $(SRCDIR) $(EXEC_SDIR)

DEBUG=no
ifeq ($(DEBUG),yes)
    CXXFLAGS += -O0 -g
else
    CXXFLAGS += -O3 -DNDEBUG
endif

.PHONY: all checkdirs clean clean-all

all: checkdirs $(BUILDDIR)/$(EXEC)

checkdirs: $(BUILDDIR)

$(BUILDDIR):
	@mkdir -p $@

clean:
	@rm -rf $(BUILDDIR)

clean-all: clean
	@rm -rf $(EXEC)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(INCLUDES) -c $< -o $@

$(BUILDDIR)/$(EXEC): $(OBJ) $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(wildcard $(SRCDIR)/*.cpp))
	$(AR) rcs $@ $^

