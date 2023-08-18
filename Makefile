PROJECT_DIR	:= $(shell pwd)
USER		?= $(shell id -un)
JOBS		?= $$(( $$(grep processor /proc/cpuinfo|tail -1|cut -d: -f2) + 1))
VERBOSE		?= 0

SRC_DIR		?= ${PROJECT_DIR}/src
EXTERNAL_DIR	?= ${PROJECT_DIR}/external
BSD_DIR		?= ${EXTERNAL_DIR}/bsd
MIT_DIR		?= ${EXTERNAL_DIR}/mit
APACHE_DIR      ?= ${EXTERNAL_DIR}/apache
INSTALL_DIR	?= ${PROJECT_DIR}/opt
BUILD_DIR	?= ${PROJECT_DIR}/build

CMAKE		?= ${INSTALL_DIR}/bin/cmake
CPACK		?= ${INSTALL_DIR}/bin/cpack

# If clang is found, we prefer it, to build our own clang.
ifeq ($(shell if command -v clang 1> /dev/null; then echo clang; else echo ""; fi),clang)
CC		:= clang
CXX		:= clang++
CPP		:= 'clang -E'

export CC CPP CXX
endif

# List of all the projects / repositories
PROJECTS:= cmake llvm avatica

all: cmake llvm
	@echo "-----------------------------------------------------------------------"
	@echo ""

# Deprecated: run proteus-cli-server from Proteus reposistory.
#run-server: all
#	cd ${INSTALL_DIR}/pelago && \
#		java -jar ${INSTALL_DIR}/lib/clotho.jar \
#			--server inputs/plans/schema.json
#
# Deprecated: run proteus-planner from Proteus reposistory.
#run-client: avatica
#	JAVA_CLASSPATH=${INSTALL_DIR}/lib/avatica-1.13.0.jar \
#		sqlline --color=true \
#			-u "jdbc:avatica:remote:url=http://localhost:8081;serialization=PROTOBUF"

#######################################################################
# top-level targets, checks if a call to make is required before
# calling it.
#######################################################################

.PHONY: avatica
avatica: .avatica.install_done

.PHONY: external-libs
external-libs: cmake llvm

#######################################################################
# Package targets
#######################################################################

.PHONY: do-package-llvm

#######################################################################
# Install targets
#######################################################################

.PHONY: do-install-avatica
do-install-avatica:
	[ -d ${INSTALL_DIR}/lib ] || mkdir -p ${INSTALL_DIR}/lib
	cd ${INSTALL_DIR}/lib && curl -O https://repo1.maven.org/maven2/org/apache/calcite/avatica/avatica/1.13.0/avatica-1.13.0.jar

# As we just download the binary, there is no point in checking for all
# the preceding steps.
do-build-avatica do-conf-avatica:
	true

#######################################################################
# Clean targets
#######################################################################

.PHONY: clean

#######################################################################
# Retrieve common definitions and targets.
include Makefile.common
