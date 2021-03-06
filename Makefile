PELAGOS_DIR	:= $(shell pwd)
USER		?= $(shell id -un)
VERBOSE		?= 0

SRC_DIR		?= ${PELAGOS_DIR}/src
INSTALL_DIR	?= ${PELAGOS_DIR}/opt
BUILD_DIR	?= ${PELAGOS_DIR}/build

JOBS		?= $$(( $$(grep processor /proc/cpuinfo|tail -1|cut -d: -f2) + 1))

RAW_CHECKOUT   ?= "git clone https://github.com/epfl-dias/proteus"
RAW_REVISION   ?= "-b v1.0"

RAPIDJSON_CHECKOUT ?= "git clone https://github.com/miloyip/rapidjson"
RAPIDJSON_REVISION ?= "-b v1.1.0"

GTEST_CHECKOUT	?= "git clone https://github.com/google/googletest.git"
GTEST_REVISION	?= "-b release-1.7.0"

GLOG_CHECKOUT	?= "git clone https://github.com/google/glog.git"
GLOG_REVISION	?= "-b v0.3.4"

POSTGRES_CHECKOUT ?= "git clone https://github.com/postgres/postgres.git"
POSTGRES_REVISION ?= ""

LLVM_CHECKOUT	?= "svn co http://llvm.org/svn/llvm-project"
LLVM_REVISION	?= "tags/RELEASE_391/final"

all: postgres proteus
	@make --no-print-directory show-config

#######################################################################
# top-level targets, checks if a call to make is required before
# calling it.
#######################################################################

.PHONY: proteus
proteus: proteus.install_done
	# This is the main tree, so do not shortcut it
	rm proteus.install_done
	rm proteus.build_done

.PHONY: postgres
postgres: postgres.install_done
	# We might want to also not shortcut this until the backend is
	# connected?
	#rm postgres.build_done

.PHONY: rapidjson
rapidjson: rapidjson.install_done

.PHONY: gtest
gtest: gtest.install_done

.PHONY: glog
glog: glog.install_done

.PHONY: llvm
llvm: llvm.install_done

#######################################################################
# Install targets
#######################################################################
do-install-gtest: gtest.build_done
	cd ${BUILD_DIR}/gtest && \
		cp *.a ${INSTALL_DIR}/lib && \
		cp -r include/gtest ${INSTALL_DIR}/include

#######################################################################
# Build targets
#######################################################################
do-build-proteus: glog gtest rapidjson

do-build-llvm: llvm.configure_done
	cd ${BUILD_DIR}/llvm && \
		make -j ${JOBS}

#######################################################################
# Configure targets
#######################################################################
# LD_LIBRARY_PATH=${INSTALL_DIR}/lib \

COMMON_ENV := \
 PATH=${INSTALL_DIR}/bin:${PATH} \
 CC=${INSTALL_DIR}/bin/clang \
 CXX=${INSTALL_DIR}/bin/clang++ \
 CPP=${INSTALL_DIR}/bin/clang\ -E

do-conf-gtest: gtest.checkout_done llvm
# Work around broken project
	rm -rf ${BUILD_DIR}/gtest
	cp -r ${SRC_DIR}/gtest ${BUILD_DIR}/gtest
	cd ${BUILD_DIR}/gtest && \
		${COMMON_ENV} \
		cmake .

do-conf-glog: glog.checkout_done llvm
# Work around broken project
	rm -rf ${BUILD_DIR}/glog
	cp -r ${SRC_DIR}/glog ${BUILD_DIR}/glog
	cd ${BUILD_DIR}/glog && autoreconf -fi .
	cd ${BUILD_DIR}/glog && \
		${COMMON_ENV} \
		ac_cv_have_libgflags=no ac_cv_lib_gflags_main=no ./configure --prefix ${INSTALL_DIR}

do-conf-postgres: postgres.checkout_done llvm
	[ -d ${BUILD_DIR}/postgres ] || mkdir -p ${BUILD_DIR}/postgres
	cd ${BUILD_DIR}/postgres && \
		${COMMON_ENV} \
		${SRC_DIR}/postgres/configure --prefix ${INSTALL_DIR}

do-conf-proteus: proteus.checkout_done rapidjson glog gtest llvm
	[ -d ${BUILD_DIR}/proteus ] || mkdir -p ${BUILD_DIR}/proteus
	cd ${BUILD_DIR}/proteus && \
		${COMMON_ENV} \
		cmake ${SRC_DIR}/proteus \
			-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \

do-conf-rapidjson: rapidjson.checkout_done llvm
	[ -d ${BUILD_DIR}/rapidjson ] || mkdir -p ${BUILD_DIR}/rapidjson
	cd ${BUILD_DIR}/rapidjson && \
		${COMMON_ENV} \
		cmake ${SRC_DIR}/rapidjson/ \
			-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}

# LLVM_ENABLE_CXX11: Make sure everything compiles using C++11
# LLVM_ENABLE_EH: required for throwing exceptions
# LLVM_ENABLE_RTTI: required for dynamic_cast
# LLVM_REQUIRE_RTTI: required for dynamic_cast
do-conf-llvm: llvm.checkout_done
	[ -d ${BUILD_DIR}/llvm ] || mkdir -p ${BUILD_DIR}/llvm
	cd ${BUILD_DIR}/llvm && cmake ${SRC_DIR}/llvm \
		-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DLLVM_ENABLE_CXX11=ON \
		-DLLVM_ENABLE_ASSERTIONS=ON \
		-DLLVM_ENABLE_PIC=ON \
		-DLLVM_ENABLE_EH=ON \
		-DLLVM_ENABLE_RTTI=ON \
		-DLLVM_REQUIRES_RTTI=ON \
		-DBUILD_SHARED_LIBS=ON \
		-DLLVM_TARGETS_TO_BUILD="X86" \
		-Wno-dev

#######################################################################
# Checkout sources as needed
#######################################################################
.PRECIOUS: src/proteus
src/proteus:
	eval ${RAW_CHECKOUT} ${RAW_REVISION} src/proteus

.PRECIOUS: src/rapidjson
src/rapidjson:
	eval ${RAPIDJSON_CHECKOUT} ${RAPIDJSON_REVISION} src/rapidjson

.PRECIOUS: src/gtest
src/gtest:
	eval ${GTEST_CHECKOUT} ${GTEST_REVISION} src/gtest

.PRECIOUS: src/glog
src/glog:
	eval ${GLOG_CHECKOUT} ${GLOG_REVISION} src/glog

.PRECIOUS: src/postgres
src/postgres:
	eval ${POSTGRES_CHECKOUT} ${POSTGRES_REVISION} src/postgres

.PRECIOUS: src/llvm
src/llvm:
	eval ${LLVM_CHECKOUT}/llvm/${LLVM_REVISION} src/llvm
	eval ${LLVM_CHECKOUT}/cfe/${LLVM_REVISION} src/llvm/tools/clang
	eval ${LLVM_CHECKOUT}/compiler-rt/${LLVM_REVISION} src/llvm/projects/compiler-rt
	eval ${LLVM_CHECKOUT}/libcxx/${LLVM_REVISION} src/llvm/projects/libcxx
	eval ${LLVM_CHECKOUT}/libcxxabi/${LLVM_REVISION} src/llvm/projects/libcxxabi
	eval ${LLVM_CHECKOUT}/libunwind/${LLVM_REVISION} src/llvm/projects/libunwind

#######################################################################
# Makefile utils / Generic targets
#######################################################################
ifeq (${VERBOSE},0)
# Do not echo the commands before executing them.
.SILENT:
endif

.PHONY: show-config
show-config:
	@echo "-----------------------------------------------------------------------"
	@echo "Configuration:"
	@echo "-----------------------------------------------------------------------"
	@echo "PELAGOS_DIR		:= ${PELAGOS_DIR}"
	@echo "SRC_DIR			:= ${SRC_DIR}"
	@echo "BUILD_DIR		:= ${BUILD_DIR}"
	@echo "INSTALL_DIR		:= ${INSTALL_DIR}"
	@echo "JOBS			:= ${JOBS}"
	@echo "USER			:= ${USER}"
	@echo "VERBOSE			:= ${VERBOSE}"

.PHONY: show-versions
show-versions:
	@echo "-----------------------------------------------------------------------"
	@echo "Projects:"
	@echo "-----------------------------------------------------------------------"
	@echo "LLVM_REVISION		:= ${LLVM_REVISION}"
	@echo "LLVM_CHECKOUT		:= ${LLVM_CHECKOUT}"
	@echo "POSTGRES_CHECKOUT	:= ${POSTGRES_CHECKOUT}"
	@echo "POSTGRES_REVISION	:= ${POSTGRES_REVISION}"
	@echo "GLOG_CHECKOUT		:= ${GLOG_CHECKOUT}"
	@echo "GLOG_REVISION		:= ${GLOG_REVISION}"
	@echo "GTEST_CHECKOUT		:= ${GTEST_CHECKOUT}"
	@echo "GTEST_REVISION		:= ${GTEST_REVISION}"
	@echo "RAPIDJSON_CHECKOUT	:= ${RAPIDJSON_CHECKOUT}"
	@echo "RAPIDJSON_REVISION	:= ${RAPIDJSON_REVISION}"
	@echo "RAW_CHECKOUT		:= ${RAW_CHECKOUT}"
	@echo "RAW_REVISION		:= ${RAW_REVISION}"

.PHONY: showvars
showvars:
	@make --no-print-directory show-config
	@echo
	@make --no-print-directory show-versions
	@echo "-----------------------------------------------------------------------"

.PHONY: dist-clean
dist-clean: clean
	-for f in glog gtest rapidjson llvm; \
	do \
		rm -rf ${SRC_DIR}/$${f}; \
	done

.PHONY: clean
clean:
	git clean -dxf .

%: %.install_done

.PHONY: do-install-%
do-install-%: %.build_done
	[ -d ${INSTALL_DIR} ] || mkdir -p ${INSTALL_DIR}
	cd ${BUILD_DIR}/$$(echo $@ | sed -e 's,do-install-,,') && \
		make install

.PHONY: do-build-%
do-build-%: %.configure_done
	cd ${BUILD_DIR}/$$(echo $@ | sed -e 's,do-build-,,') && \
		make -j ${JOBS}

.PHONY: do-conf-%

.PRECIOUS: %.install_done
%.install_done: %.build_done
	@echo "-----------------------------------------------------------------------"
	@echo "-- $$(echo $@ | sed -e 's,_done,,')..."
	make do-install-$$(echo $@ | sed -e 's,.install_done,,')
	@echo "-- $$(echo $@ | sed -e 's,_done,,') done."
	touch $@

.PRECIOUS: %.build_done
%.build_done: %.configure_done
	@echo "-----------------------------------------------------------------------"
	@echo "-- $$(echo $@ | sed -e 's,_done,,')..."
	make do-build-$$(echo $@ | sed -e 's,.build_done,,')
	@echo "-- $$(echo $@ | sed -e 's,_done,,') done."
	touch $@

.PRECIOUS: %.configure_done
%.configure_done: %.checkout_done
	@echo "-----------------------------------------------------------------------"
	@echo "-- $$(echo $@ | sed -e 's,_done,,')..."
	make do-conf-$$(echo $@ | sed -e 's,.configure_done,,')
	@echo "-- $$(echo $@ | sed -e 's,_done,,') done."
	touch $@

.PRECIOUS: %.checkout_done
%.checkout_done: src/%
	@echo "-----------------------------------------------------------------------"
	@echo "$@ done."
	touch $@
