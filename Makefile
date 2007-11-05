# ltspfs

#SPEC    = $(shell echo -n dist/*.spec)
NAME    = ltsp
VERSION = 5.1

all:
	@echo "SPEC	$(shell basename ${SPEC})"
	@echo "NAME   	${NAME}"
	@echo "VERSION	${VERSION}"
	@echo
	@echo "Valid Commands:"
	@echo "  make dist"
	@echo "    Create a versioned tarball"
	@echo "  make clean"
	@echo "    Clean up tarballs"
	@echo "  make tag"
	@echo "    bzr tag NAME-VERSION"

dist : clean
	# Create tarball NAME-VERSION.tar.bz2
	@cp -a dist ${NAME}-${VERSION}
	@rm -rf ${NAME}-${VERSION}/.bzr
	@tar cfj ${NAME}-${VERSION}.tar.bz2 ${NAME}-${VERSION}
	@tar cfz ${NAME}-${VERSION}.tar.gz ${NAME}-${VERSION}
	@rm -rf ${NAME}-${VERSION}
	@echo "${NAME}-${VERSION}.tar.bz2 created."
	@echo "${NAME}-${VERSION}.tar.gz created."

clean:
	@rm -rf *.tar.bz2

tag:
	# bzr tag NAME-VERSION
	# XXX: this test is broken!
	if test "$(bzr status -S)" != ""; then \
	    echo "ERROR: Changes detected.  Did you forget to checkin?"; \
	    echo; \
	    bzr diff; \
            exit 1; \
	fi

	bzr tag ${NAME}-${VERSION}
