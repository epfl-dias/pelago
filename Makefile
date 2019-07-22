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
PROJECTS:= llvm glog gtest rapidjson executor oltp avatica planner SQLPlanner

#FIXME: Currently coordinator.py depends on SQLPlanner to be build, and not planner.
#	Also, it assumes a fixed folder layout, and execution from the src folder.
all: llvm | .panorama.checkout_done executor oltp htap planner SQLPlanner
	@echo "-----------------------------------------------------------------------"
	@echo ""

run-server: all
	cd ${INSTALL_DIR}/pelago && \
		java -jar ${INSTALL_DIR}/bin/clotho-*.jar \
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

.PHONY: oltp
oltp: external-libs .oltp.install_done
	# This is the main tree, so do not shortcut it
	rm .oltp.install_done
	rm .oltp.build_done

htap: external-libs .htap.install_done
	# This is the main tree, so do not shortcut it
	rm .htap.install_done
	rm .htap.build_done

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
	cp ${SRC_DIR}/planner/target/scala-*/clotho-*.jar ${INSTALL_DIR}/bin/

do-install-avatica:
	[ -d ${INSTALL_DIR}/lib ] || mkdir -p ${INSTALL_DIR}/lib
	cd ${INSTALL_DIR}/lib && curl -O http://central.maven.org/maven2/org/apache/calcite/avatica/avatica/1.13.0/avatica-1.13.0.jar

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

do-conf-executor: .executor.checkout_done external-libs
	[ -d ${BUILD_DIR}/executor ] || mkdir -p ${BUILD_DIR}/executor
	cd ${BUILD_DIR}/executor && \
		${COMMON_ENV} \
		$(CMAKE) ${SRC_DIR}/executor \
			-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
			-DSTANDALONE=OFF

do-conf-oltp: .oltp.checkout_done external-libs
	[ -d ${BUILD_DIR}/oltp ] || mkdir -p ${BUILD_DIR}/oltp
	cd ${BUILD_DIR}/oltp && \
		${COMMON_ENV} \
		$(CMAKE) ${SRC_DIR}/oltp \
			-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
			-DSTANDALONE=OFF \
			-DPROTEUS_SOURCE_DIR=${SRC_DIR}/executor

do-conf-htap: .htap.checkout_done external-libs
	[ -d ${BUILD_DIR}/htap ] || mkdir -p ${BUILD_DIR}/htap
	cd ${BUILD_DIR}/htap && \
		${COMMON_ENV} \
		$(CMAKE) ${SRC_DIR}/htap \
			-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
			-DPROTEUS_SOURCE_DIR=${SRC_DIR}/executor \
			-DAEOLUS_SOURCE_DIR=${SRC_DIR}/oltp



# RapidJSON is a head-only library, but it will try to build documentation,
# examples and unit-tests unless explicitly told not to do so.
do-conf-rapidjson: .rapidjson.checkout_done llvm
	[ -d ${BUILD_DIR}/rapidjson ] || mkdir -p ${BUILD_DIR}/rapidjson
	cd ${BUILD_DIR}/rapidjson && \
		${COMMON_ENV} \
		$(CMAKE) ${MIT_DIR}/rapidjson/ \
			-DRAPIDJSON_BUILD_DOC=OFF \
			-DRAPIDJSON_BUILD_EXAMPLES=OFF \
			-DRAPIDJSON_BUILD_TESTS=OFF \
			-DRAPIDJSON_BUILD_CXX11=ON \
			-DRAPIDJSON_HAS_CXX11_RVALUE_REFS=ON \
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

do-checkout-rapidjson:
	git submodule update ${SHALLOW_LLVM_SUBMODULES} --init --recursive ${MIT_DIR}/rapidjson
	# Local patch to fix empty statement error
	cd ${MIT_DIR}/rapidjson && git apply ${PROJECT_DIR}/patches/0001-rapidjson-empty-statements.patch

#######################################################################
# Clean targets
#######################################################################

.PHONY: clean
clean: clean-executor clean-oltp clean-htap

#######################################################################
# Retrieve common definitions and targets.
include Makefile.common
