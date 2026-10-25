CC=gcc
LD=ld
DIR="$(CURDIR)"
AS=as
BUILD_DIR="$(CURDIR)/build"

all:
	@python3 "$(CURDIR)/scripts/confdir.py"
	@$(MAKE) -C "$(CURDIR)/boot"

clean:
	@rm -rf build

export CC
export LD
export AS
export DIR
export BUILD_DIR