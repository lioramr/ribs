ifndef OPTFLAGS
OPTFLAGS=-O3
endif

ifndef DEPTH
DEPTH=..
endif

RIBS_INCLUDES=-I$(DEPTH)/../ribs/include
RIBS_LIB=-L$(DEPTH)/../ribs/lib $(RLIBS:%=-l%)

LDFLAGS+= -pthread -lrt $(RIBS_LIB) -L../lib $(LIBS)
CFLAGS+= $(OPTFLAGS) $(INCLUDES:%=-I%) -gdwarf-2 -W -Wall -Werror $(RIBS_INCLUDES)
CXXFLAGS+= $(OPTFLAGS) $(INCLUDES:%=-I%) -gdwarf-2 -W -Wall -Werror $(RIBS_INCLUDES)

OBJ=$(SRC:%.cpp=../obj/%.o)
DEP=$(SRC:%.cpp=../obj/%.d)

DIRS=../obj/.dir ../bin/.dir ../lib/.dir

ifeq ($(TARGET:%.a=%).a,$(TARGET))
LIB_OBJ:=$(OBJ)
TARGET_FILE=../lib/lib$(TARGET)
else
TARGET_FILE=../bin/$(TARGET)
endif

all: $(TARGET_FILE)

$(DIRS):
	@echo "  (MKDIR) $(@:%/.dir=%)"
	@-mkdir $(@:%/.dir=%)
	@touch $@

../obj/%.o: %.cpp ../obj/%.d
	@echo "  (C++)  $*.cpp  [ -c $(CXXFLAGS) $*.cpp -o ../obj/$*.o ]"
	@$(CXX) -c $(CXXFLAGS) $*.cpp -o ../obj/$*.o

../obj/%.d: %.cpp
	@echo "  (DEP)  $*.cpp"
	@$(CXX) -MM $(INCLUDES:%=-I%) $(RIBS_INCLUDES) $*.cpp | sed -e 's|.*:|../obj/$*.o:|' > $@

$(OBJ): $(DIRS)

$(DEP): $(DIRS)

../lib/%: $(LIB_OBJ)
	@echo "  (AR)   $(@:../lib/%=%)  [ rcs $@ $^ ]"
	@ar rcs $@ $^

../bin/%: $(OBJ) $(RLIBS:%=$(DEPTH)/../ribs/lib/lib%.a) $(EXTRA_DEPS)
	@echo "  (LD)   $(@:../bin/%=%)  [ $(OBJ) -o $@ -o $@ $(LDFLAGS) ]"
	@$(CXX) $(OBJ) -o $@ $(LDFLAGS)

$(DIRS:%=RM_%):
	@echo "  (RM)   $(@:RM_%/.dir=%)/*"
	@-rm -f $(@:RM_%/.dir=%)/*

clean: $(DIRS:%=RM_%)

etags:
	@echo "  (ETAGS)"
	@find . -name "*.[ch]" -o -name "*.cpp" -o -name "*.inl" | cut -c 3- | xargs etags --lang=c++ -I

ifneq ($(MAKECMDGOALS),clean)
-include $(DEP)
endif
