CXX := g++ -pthread
CXXFLAGS := -std=c++1z -ggdb -Wall -pedantic
SRCDIR := src
BUILDDIR := build
TARGETDIR := bin

EXECUTABLE := ospfd 
TARGET := $(TARGETDIR)/$(EXECUTABLE)

SRCEXT := cpp
SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o))

INCDIRS := $(shell find $(SRCDIR)/inc/* -name '*.hpp' -exec dirname {} \; | sort | uniq)
INCLIST := $(patsubst $(SRCDIR)/inc/%,-I $(SRCDIR)/inc/%,$(INCDIRS))
BUILDLIST := $(patsubst $(SRCDIR)/inc/%,$(BUILDDIR)/%,$(INCDIRS))

$(TARGET): $(OBJECTS)
	@mkdir -p $(TARGETDIR)
	@echo "Linking..."
	@echo "  Linking $(TARGET)"; $(CXX) $^ -o $(TARGET) $(LIB)
	@chmod +s $(TARGET)
	@echo " Setting Capabilities"; sudo setcap cap_net_raw=ep $(TARGET) 

$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIR)
	@echo "Compiling $<..."; $(CXX) $(CXXFLAGS) $(INC) -c -o $@ $<

clean:
	@echo "Cleaning $(TARGET)..."; $(RM) -r $(BUILDDIR) $(TARGET)

.PHONY: clean
