PELAGOS_DIR	:= $(shell pwd)
USER		?= $(shell id -un)
VERBOSE		?= 0

SRC_DIR		?= ${PELAGOS_DIR}/src
INSTALL_DIR	?= ${PELAGOS_DIR}/opt
BUILD_DIR	?= ${PELAGOS_DIR}/build

# CMAKE3		?= ~/cmake/bin/cmake
CMAKE3		?= cmake

JOBS		?= $$(( $$(grep processor /proc/cpuinfo|tail -1|cut -d: -f2) + 1))

all: raw-jit-executor
	@make --no-print-directory show-config

#######################################################################
# top-level targets, checks if a call to make is required before
# calling it.
#######################################################################

.PHONY: raw-jit-executor
raw-jit-executor: raw-jit-executor.install_done
	# This is the main tree, so do not shortcut it
	rm raw-jit-executor.install_done
	rm raw-jit-executor.build_done

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
	cd ${BUILD_DIR}/gtest/googlemock/gtest && \
		cp *.a ${INSTALL_DIR}/lib && \
		cp -r ${BUILD_DIR}/gtest/googletest/include/gtest \
			${BUILD_DIR}/gtest/googlemock/include/gmock \
			${INSTALL_DIR}/include

#######################################################################
# Build targets
#######################################################################
do-build-raw-jit-executor: glog gtest rapidjson

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
	cd ${BUILD_DIR}/gtest && rm -rf .git*
	cd ${BUILD_DIR}/gtest && \
		${COMMON_ENV} \
		$(CMAKE3) .

do-conf-glog: glog.checkout_done llvm
# Work around broken project
	rm -rf ${BUILD_DIR}/glog
	cp -r ${SRC_DIR}/glog ${BUILD_DIR}/glog
	cd ${BUILD_DIR}/glog && rm -rf .git*
	cd ${BUILD_DIR}/glog && autoreconf -fi .
	cd ${BUILD_DIR}/glog && \
		${COMMON_ENV} \
		ac_cv_have_libgflags=0 ac_cv_lib_gflags_main=no ./configure --prefix ${INSTALL_DIR}
# sed -i 's/^#define HAVE_LIB_GFLAGS 1/#undef HAVE_LIB_GFLAGS/g' ${BUILD_DIR}/glog/src/config.h

do-conf-raw-jit-executor: raw-jit-executor.checkout_done rapidjson glog gtest llvm
	[ -d ${BUILD_DIR}/raw-jit-executor ] || mkdir -p ${BUILD_DIR}/raw-jit-executor
	cd ${BUILD_DIR}/raw-jit-executor && \
		${COMMON_ENV} \
		$(CMAKE3) ${SRC_DIR}/raw-jit-executor \
			-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \

do-conf-rapidjson: rapidjson.checkout_done llvm
	[ -d ${BUILD_DIR}/rapidjson ] || mkdir -p ${BUILD_DIR}/rapidjson
	cd ${BUILD_DIR}/rapidjson && \
		${COMMON_ENV} \
		$(CMAKE3) ${SRC_DIR}/rapidjson/ \
			-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}

# LLVM_ENABLE_CXX11: Make sure everything compiles using C++11
# LLVM_ENABLE_EH: required for throwing exceptions
# LLVM_ENABLE_RTTI: required for dynamic_cast
# LLVM_REQUIRE_RTTI: required for dynamic_cast
LLVM_TARGETS_TO_BUILD:= \
$$(case $$(uname -m) in \
	x86|x86_64) echo "X86;NVPTX";; \
	ppc64le) echo "PowerPC;NVPTX";; \
esac)

do-conf-llvm: llvm.checkout_done
	[ -d ${BUILD_DIR}/llvm ] || mkdir -p ${BUILD_DIR}/llvm
	cd ${BUILD_DIR}/llvm && \
		$(CMAKE3) ${SRC_DIR}/llvm \
		-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DLLVM_ENABLE_CXX11=ON \
		-DLLVM_ENABLE_ASSERTIONS=ON \
		-DLLVM_ENABLE_PIC=ON \
		-DLLVM_ENABLE_EH=ON \
		-DLLVM_ENABLE_RTTI=ON \
		-DLLVM_REQUIRES_RTTI=ON \
		-DBUILD_SHARED_LIBS=ON \
		-DLLVM_USE_INTEL_JITEVENTS:BOOL=ON \
		-DLLVM_TARGETS_TO_BUILD="${LLVM_TARGETS_TO_BUILD}" \
		-Wno-dev 

#######################################################################
# Checkout sources as needed
#######################################################################

.PRECIOUS: clang
.PRECIOUS: compiler-rt
.PRECIOUS: glog
.PRECIOUS: gtest
.PRECIOUS: libcxx
.PRECIOUS: libcxxabi
.PRECIOUS: libunwind
.PRECIOUS: llvm
.PRECIOUS: rapidjson

do-checkout-llvm:
	# No way of adding from a top level submodules within sub-
	# modules, so stickying to this method.
	git submodule update --init --recursive src/llvm src/clang src/compiler-rt src/libcxx src/libcxxabi src/libunwind
	ln -sf ../../clang src/llvm/tools/clang
	ln -sf ../../compiler-rt src/llvm/projects/compiler-rt
	ln -sf ../../libcxx src/llvm/projects/libcxx
	ln -sf ../../libcxxabi src/llvm/projects/libcxxabi
	ln -sf ../../libunwind src/llvm/projects/libunwind
	# for CUDA 9.1+ support on LLVM 6:
	#   git cherry-pick ccacb5ddbcbb10d9b3a4b7e2780875d1e5537063
	cd src/llvm/tools/clang && git cherry-pick ccacb5ddbcbb10d9b3a4b7e2780875d1e5537063
	# for CUDA 9.2 support on LLVM 6:
	#   git cherry-pick 5f76154960a51843d2e49c9ae3481378e09e61ef
	cd src/llvm/tools/clang && git cherry-pick 5f76154960a51843d2e49c9ae3481378e09e61ef

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
		make -j ${JOBS} install

.PHONY: do-build-%
do-build-%: %.configure_done
	cd ${BUILD_DIR}/$$(echo $@ | sed -e 's,do-build-,,') && \
		make -j ${JOBS}

.PHONY: do-conf-%

.PHONY: do-checkout-%
do-checkout-%:
	git submodule update --init --recursive src/$$(echo $@ | sed -e 's,do-checkout-,,')

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
%.checkout_done:
	@echo "-----------------------------------------------------------------------"
	@echo "-- $$(echo $@ | sed -e 's,_done,,')..."
	make do-checkout-$$(echo $@ | sed -e 's,.checkout_done,,')
	@echo "-- $$(echo $@ | sed -e 's,_done,,') done."
	touch $@
