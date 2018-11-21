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

# If clang is found, we prefer it.
ifeq ($(shell if command -v clang 1> /dev/null; then echo clang; else echo ""; fi),clang)
CC		:= clang
CXX		:= clang++
CPP		:= 'clang -E'

export CC CPP CXX
endif

COMMON_ENV := \
 PATH=${INSTALL_DIR}/bin:${PATH} \
 CC=${INSTALL_DIR}/bin/clang \
 CXX=${INSTALL_DIR}/bin/clang++ \
 CPP=${INSTALL_DIR}/bin/clang\ -E

# List of all the projects / repositories
PROJECTS:= llvm glog gtest rapidjson raw-jit-executor avatica planner SQLPlanner

#FIXME: Currently coordinator.py depends on SQLPlanner to be build, and not planner.
#	Also, it assumes a fixed folder layout, and execution from the src folder.
all: llvm | .panorama.checkout_done raw-jit-executor planner SQLPlanner
	@echo "-----------------------------------------------------------------------"
	@echo ""

run-server: all
	cd ${INSTALL_DIR}/raw && \
		java -jar ${INSTALL_DIR}/bin/SQLPlanner-*.jar \
			--server inputs/plans/schema.json

run-client: avatica
	JAVA_CLASSPATH=${INSTALL_DIR}/lib/avatica-1.12.0.jar \
		sqlline --color=true \
			-u "jdbc:avatica:remote:url=http://localhost:8081;serialization=PROTOBUF"

#######################################################################
# top-level targets, checks if a call to make is required before
# calling it.
#######################################################################

.PHONY: raw-jit-executor
raw-jit-executor: external-libs .raw-jit-executor.install_done
	# This is the main tree, so do not shortcut it
	rm .raw-jit-executor.install_done
	rm .raw-jit-executor.build_done

.PHONY: planner
planner: avatica .planner.install_done

# We don't install it, and just execute it from the source folder.
.PHONY: SQLPlanner
SQLPlanner: .SQLPlanner.build_done

.PHONY: avatica
avatica: .avatica.install_done

.PHONY: external-libs
external-libs: llvm glog gtest rapidjson

.PHONY: rapidjson
rapidjson: llvm glog gtest .rapidjson.install_done

.PHONY: gtest
gtest: llvm glog .gtest.install_done

.PHONY: glog
glog: llvm .glog.install_done

#######################################################################
# Install targets
#######################################################################

do-install-gtest: .gtest.build_done
	cd ${BUILD_DIR}/gtest/lib && \
		cp *.a ${INSTALL_DIR}/lib && \
		cp -r ${BUILD_DIR}/gtest/googletest/include/gtest \
			${BUILD_DIR}/gtest/googlemock/include/gmock \
			${INSTALL_DIR}/include

do-install-planner: .planner.build_done
	[ -d ${INSTALL_DIR}/bin ] || mkdir -p ${INSTALL_DIR}/bin
	cp ${SRC_DIR}/planner/target/scala-*/SQLPlanner-*.jar ${INSTALL_DIR}/bin/

do-install-avatica:
	[ -d ${INSTALL_DIR}/lib ] || mkdir -p ${INSTALL_DIR}/lib
	# Direct downloads will trip in no time blacklisting of servers.
	cd ${INSTALL_DIR}/lib && wget http://central.maven.org/maven2/org/apache/calcite/avatica/avatica/1.12.0/avatica-1.12.0.jar
	#cp ${EXTERNAL_DIR}/avatica-1.12.0.jar ${INSTALL_DIR}/lib

# As we just download the binary, there is no point in checking for all
# the preceding steps.
do-build-avatica do-conf-avatica:
	true

#######################################################################
# Build targets
#######################################################################

# There is no configure step, so depend directly on the checkout.
do-build-SQLPlanner: .SQLPlanner.checkout_done
	cd ${SRC_DIR}/SQLPlanner && sbt assembly

# There is no configure step, so depend directly on the checkout.
do-build-planner: .planner.checkout_done
	cd ${SRC_DIR}/planner && sbt assembly

#######################################################################
# Configure targets
#######################################################################

do-conf-gtest: .gtest.checkout_done llvm
# Work around broken project
	[ -d ${BUILD_DIR} ] || mkdir -p ${BUILD_DIR}
	rm -rf ${BUILD_DIR}/gtest
	cp -r ${BSD_DIR}/gtest ${BUILD_DIR}/gtest
	cd ${BUILD_DIR}/gtest && rm -rf .git*
	cd ${BUILD_DIR}/gtest && \
		${COMMON_ENV} \
		$(CMAKE) .

do-conf-glog: .glog.checkout_done llvm
# Work around broken project
	[ -d ${BUILD_DIR} ] || mkdir -p ${BUILD_DIR}
	rm -rf ${BUILD_DIR}/glog
	cp -r ${BSD_DIR}/glog ${BUILD_DIR}/glog
	cd ${BUILD_DIR}/glog && rm -rf .git*
	cd ${BUILD_DIR}/glog && autoreconf -fi .
	cd ${BUILD_DIR}/glog && \
		${COMMON_ENV} \
		ac_cv_have_libgflags=0 ac_cv_lib_gflags_main=no ./configure --prefix ${INSTALL_DIR}

do-conf-raw-jit-executor: .raw-jit-executor.checkout_done external-libs
	[ -d ${BUILD_DIR}/raw-jit-executor ] || mkdir -p ${BUILD_DIR}/raw-jit-executor
	cd ${BUILD_DIR}/raw-jit-executor && \
		${COMMON_ENV} \
		$(CMAKE) ${SRC_DIR}/raw-jit-executor \
			-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \

# RapidJSON is a head-only library, but it will try to build documentation and
# unit-tests unless explicitly told not to do so.
do-conf-rapidjson: .rapidjson.checkout_done llvm
	[ -d ${BUILD_DIR}/rapidjson ] || mkdir -p ${BUILD_DIR}/rapidjson
	cd ${BUILD_DIR}/rapidjson && \
		${COMMON_ENV} \
		$(CMAKE) ${MIT_DIR}/rapidjson/ \
			-DRAPIDJSON_BUILD_DOC=OFF \
			-DRAPIDJSON_BUILD_TESTS=OFF \
			-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}

do-conf-SQLPlanner: .SQLPlanner.checkout_done
	cd ${SRC_DIR}/SQLPlanner && sbt clean

do-conf-planner: .planner.checkout_done
	[ -d ${SRC_DIR}/planner/target ] || mkdir -p ${SRC_DIR}/planner/target

#######################################################################
# Checkout sources as needed
#######################################################################

.PRECIOUS: ${BSD_DIR}/glog
.PRECIOUS: ${BSD_DIR}/gtest
.PRECIOUS: ${MIT_DIR}/rapidjson

#######################################################################
# Clean targets
#######################################################################

.PHONY: clean
clean: clean-raw-jit-executor

#######################################################################
# Retrieve common definitions and targets.
include Makefile.common
