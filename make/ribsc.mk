ifndef OPTFLAGS
OPTFLAGS=-O3
endif
LDFLAGS+= -pthread -lrt -L../lib $(LIBS:%=-l%)
CFLAGS+= $(OPTFLAGS) -ggdb3 -W -Wall -Werror

OBJ=$(SRC:%.c=../obj/%.o)
DEP=$(SRC:%.c=../obj/%.d)

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

../obj/%.o: %.c ../obj/%.d
	@echo "  (C)    $*.c  [ -c $(CFLAGS) $*.c -o ../obj/$*.o ]"
	@$(CC) -c $(CFLAGS) $*.c -o ../obj/$*.o

../obj/%.d: %.c
	@echo "  (DEP)  $*.c"
	@$(CC) -MM $(INCLUDES) $*.c | sed -e 's|.*:|../obj/$*.o:|' > $@

$(OBJ): $(DIRS)

$(DEP): $(DIRS)

../lib/%: $(LIB_OBJ)
	@echo "  (AR)   $(@:../lib/%=%)  [ rcs $@ $^ ]"
	@ar rcs $@ $^

../bin/%: $(OBJ) $(LIBS:%=../lib/lib%.a)
	@echo "  (LD)   $(@:../bin/%=%)  [ -o $@ $(OBJ) $(LDFLAGS) ]"
	@$(CC) -o $@ $(OBJ) $(LDFLAGS)

$(DIRS:%=RM_%):
	@echo "  (RM)   $(@:RM_%/.dir=%)/*"
	@-rm -f $(@:RM_%/.dir=%)/*

clean: $(DIRS:%=RM_%)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEP)
endif
