SUBDIRS = src . tests
ACLOCAL_AMFLAGS = -I m4

doc/doxygen:
	make -C doc doxygen

# Configuration for pkg-config
pcdatadir = $(libdir)/pkgconfig
pcdata_DATA = streamlike.pc

if !ENABLE_TESTS
CHECK_ERROR_MSG="\
===========================================================================\\n\
Tests are disabled. Run configure with --enable-tests flag to enable them.\\n\
==========================================================================="

check-recursive:
	-@echo -e $(CHECK_ERROR_MSG)
	-@exit 1
endif
