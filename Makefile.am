SUBDIRS = src . tests
ACLOCAL_AMFLAGS = -I m4

doc/doxygen:
	make -C doc doxygen

if !ENABLE_TESTS
check-recursive:
	@>&2 echo "==========================================="
	@>&2 echo "Tests are not enabled. Run configure with"
	@>&2 echo "--enable-tests flag to enable tests."
	@>&2 echo "==========================================="
	@exit 1
endif
