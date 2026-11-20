PROGS = check.c
OPT_LEVEL = -Ofast
RUN = 1
LIBVM   := $(CS340LX_2026_PATH)/lib/libvm-ident0.0/
INC     += -I$(LIBVM) -I$(LIBVM)/includes
LIBS    += $(LIBVM)/libvm-ident.a
# prebuilt staff flip-check: the actual hammer lives here, not in check.c
STAFF_OBJS += ./flip-check.o
include $(CS340LX_2026_PATH)/libpi/mk/Makefile.robust-v3
