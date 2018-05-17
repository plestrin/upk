UPK_MOD := pyi
CC		:= gcc
CFLAGS	:= -W -Wall -pedantic -Wextra -O3 -std=c99 -D_GNU_SOURCE

.PHONY: clean

all: build $(foreach mod,$(UPK_MOD),$(mod)_)

build:
	mkdir -p build

define TEMPLATE
SRC_$(1) := $${wildcard $(1)/*.c}
OBJ_$(1) := $$(basename build/$$(notdir $$(SRC_$(1)))).o
LIB_$(1) := $$(shell cat $(1)/lib.lnk)

$(1)_: build/wrapper.o build/dump.o $$(OBJ_$(1))
	$$(CC) -o $$@ $$^ $$(LIB_$(1))

build/%.o: $(1)/%.c
	$$(CC) -o $$@ -c $$< $$(CFLAGS) -D$(1)_rtn=upk_rtn
endef

$(foreach mod,$(UPK_MOD),$(eval $(call TEMPLATE,$(mod))))

build/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	@ rm -rf build
	@ rm -f $(foreach mod,$(UPK_MOD),$(mod)_)
