PROJECT_DIR	:= $(shell pwd)
USER		?= $(shell id -un)
JOBS		?= $$(( $$(grep processor /proc/cpuinfo|tail -1|cut -d: -f2) + 1))
VERBOSE		?= 0

SRC_DIR		?= ${PROJECT_DIR}/src
EXTERNAL_DIR	?= ${PROJECT_DIR}/external
BSD_DIR		?= ${EXTERNAL_DIR}/bsd
MIT_DIR		?= ${EXTERNAL_DIR}/mit
INSTALL_DIR	?= ${PROJECT_DIR}/opt
BUILD_DIR	?= ${PROJECT_DIR}/build

CMAKE		?= cmake

# If clang is found, we prefer it, to build our own clang.
ifeq ($(shell if command -v clang 1> /dev/null; then echo clang; else echo ""; fi),clang)
CC		:= clang
CXX		:= clang++
CPP		:= 'clang -E'

export CC CPP CXX
endif

# List of all the projects / repositories
PROJECTS:= llvm executor avatica

all: llvm | .panorama.checkout_done executor
	@echo "-----------------------------------------------------------------------"
	@echo ""

run-server: all
	cd ${INSTALL_DIR}/pelago && \
		java -jar ${INSTALL_DIR}/lib/clotho.jar \
			--server inputs/plans/schema.json

run-client: avatica
	JAVA_CLASSPATH=${INSTALL_DIR}/lib/avatica-1.13.0.jar \
		sqlline --color=true \
			-u "jdbc:avatica:remote:url=http://localhost:8081;serialization=PROTOBUF"

#######################################################################
# top-level targets, checks if a call to make is required before
# calling it.
#######################################################################

.PHONY: executor
executor: external-libs .executor.install_done
	# This is the main tree, so do not shortcut it
	rm .executor.install_done
	rm .executor.build_done

.PHONY: avatica
avatica: .avatica.install_done

.PHONY: external-libs
external-libs: llvm

#######################################################################
# Install targets
#######################################################################

do-install-avatica:
	[ -d ${INSTALL_DIR}/lib ] || mkdir -p ${INSTALL_DIR}/lib
	cd ${INSTALL_DIR}/lib && curl -O https://repo1.maven.org/maven2/org/apache/calcite/avatica/avatica/1.13.0/avatica-1.13.0.jar

# As we just download the binary, there is no point in checking for all
# the preceding steps.
do-build-avatica do-conf-avatica:
	true

#######################################################################
# Configure targets
#######################################################################

do-conf-executor: .executor.checkout_done external-libs
	[ -d ${BUILD_DIR}/executor ] || mkdir -p ${BUILD_DIR}/executor
	cd ${BUILD_DIR}/executor && \
		${COMMON_ENV} \
		$(CMAKE) ${SRC_DIR}/executor \
			-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
			-DSTANDALONE=ON

#######################################################################
# Clean targets
#######################################################################

.PHONY: clean
clean: clean-executor

#######################################################################
# Retrieve common definitions and targets.
include Makefile.common
