# our math library.
TARGET := libm-pi.a

# Note: we only compile a small number of the possible routines in this
# directory. Usually adding another routine is just adding its .o here,
# possibly with whatever helper .o files it needs.
LIB_OBJS := s_cos.o k_cos.o k_sin.o e_rem_pio2.o k_rem_pio2.o \
            s_scalbn.o s_floor.o s_copysign.o s_fabs.o s_sin.o \
            s_atan.o e_atan2.o s_round.o e_sqrt.o e_atan2f.o \
            e_asinf.o s_fabsf.o s_atanf.o e_sqrtf.o s_cosf.o \
            k_cosf.o k_sinf.o

LIB_OBJS += e_powf.o e_pow.o
LIB_OBJS += s_sinf.o
LIB_OBJS += s_scalbnf.o
LIB_OBJS += s_copysignf.o
LIB_OBJS += e_rem_pio2f.o
LIB_OBJS += s_cimag.o s_cimagf.o s_cimagl.o
LIB_OBJS += s_creal.o s_crealf.o s_creall.o
LIB_OBJS += e_exp.o   # k_exp.o  # s_cexp.o

SRC := $(LIB_OBJS:.o=.c)

USE_FP := 1

include $(CS240LX_2026_PATH)/libpi/defs.mk

CFLAGS += -Iinclude

# Rebuild objects when local headers, this Makefile, or libpi change.
DEPS := $(wildcard ./*.h) ./Makefile 

.PHONY: all clean protos
all: $(TARGET)

protos:
	cproto -I. -Iinclude $(SRC) > math-protos.h

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(LIB_OBJS) $(DEPS)
	rm -f $@
	$(AR) crf $@ $(LIB_OBJS)

clean:
	rm -f $(LIB_OBJS) *~ $(TARGET)
