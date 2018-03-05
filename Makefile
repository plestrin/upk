UPK_MOD := pyi
CC		:= gcc
CFLAGS	:= -W -Wall -pedantic -Wextra -O3 -std=c99 -D_GNU_SOURCE

.PHONY: clean

all: build $(foreach mod,$(UPK_MOD),$(mod)_)

build:
	mkdir -p build

define TEMPLATE
SRC := $${wildcard $(1)/*.c}
OBJ := $$(basename build/$$(notdir $$(SRC))).o

$(1)_: build/wrapper.o $$(OBJ)
	$$(CC) -o $$@ $$^

build/%.o: $(1)/%.c
	$$(CC) -o $$@ -c $$< $$(CFLAGS) -Dpyi_rtn=upk_rtn
endef

$(foreach mod,$(UPK_MOD),$(eval $(call TEMPLATE,$(mod))))

build/%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	@ rm -rf build
	@ rm -f $(foreach mod,$(UPK_MOD),$(mod)_)
