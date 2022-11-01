# Makefile

MODULES = pgsiftorder
PGXS := $(shell pg_config --pgxs)
#PGXS := $(shell /usr/pgsql-9.4/bin/pg_config --pgxs)
#CFLAGS:=$(filter-out -Wdeclaration-after-statement,$(CPPFLAGS))
include $(PGXS)
