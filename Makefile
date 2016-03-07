CC ?= gcc
LD ?= gcc
QEMU = qemu-system-x86_64

RUNFLAGS := -no-reboot -no-shutdown -serial stdio -enable-kvm
# -pedantic is off because I want some GCC extensions
CFLAGS := -g -m64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -ffreestanding \
	-mcmodel=kernel -Wall -Wextra -Werror -std=gnu11 -O2 \
	-Wframe-larger-than=4096 -Wstack-usage=4096 -Wno-unknown-warning-option -Wno-unused-parameter
LFLAGS := -nostdlib -z max-page-size=0x1000

ASM := bootstrap.S videomem.S interrupt-wrappers.S
AOBJ:= $(ASM:.S=.o)
ADEP:= $(ASM:.S=.d)

SRC := main.c pic.c interrupt.c serial.c pit.c print.c
OBJ := $(AOBJ) $(SRC:.c=.o)
DEP := $(ADEP) $(SRC:.c=.d)

all: kernel

kernel: $(OBJ) kernel.ld
	$(LD) $(LFLAGS) -T kernel.ld -o $@ $(OBJ)

%.o: %.S
	$(CC) -D__ASM_FILE__ -g -MMD -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

-include $(DEP)

.PHONY: clean run run-debug
clean:
	rm -f kernel $(OBJ) $(DEP)

run: kernel
	$(QEMU) $(RUNFLAGS) -kernel kernel

run-debug: kernel
	$(QEMU) $(RUNFLAGS) -kernel kernel -s
