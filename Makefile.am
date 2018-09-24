SUBDIRS = src . tests
ACLOCAL_AMFLAGS = -I m4

doc/doxygen:
	make -C doc doxygen

if !HAVE_LIBCHECK
check-recursive:
	@>&2 echo "==========================================="
	@>&2 echo "libcheck should be installed for testing..."
	@>&2 echo "==========================================="
	@exit 1
endif
