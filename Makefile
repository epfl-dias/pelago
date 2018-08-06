PROJECT_DIR	:= $(shell pwd)
USER		?= $(shell id -un)
JOBS		?= $$(( $$(grep processor /proc/cpuinfo|tail -1|cut -d: -f2) + 1))
VERBOSE		?= 0

SRC_DIR		?= ${PROJECT_DIR}/src
EXTERNAL_DIR	?= ${PROJECT_DIR}/external
BSD_DIR		?= ${PROJECT_DIR}/src
BSD_DIR		?= ${EXTERNAL_DIR}/bsd
INSTALL_DIR	?= ${PROJECT_DIR}/opt
BUILD_DIR	?= ${PROJECT_DIR}/build

CMAKE		?= cmake


all: llvm | raw-jit-executor
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
		$(CMAKE) .

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
		$(CMAKE) ${SRC_DIR}/raw-jit-executor \
			-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \

do-conf-rapidjson: rapidjson.checkout_done llvm
	[ -d ${BUILD_DIR}/rapidjson ] || mkdir -p ${BUILD_DIR}/rapidjson
	cd ${BUILD_DIR}/rapidjson && \
		${COMMON_ENV} \
		$(CMAKE) ${SRC_DIR}/rapidjson/ \
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
	cd ${BUILD_DIR}/llvm && $(CMAKE) ${BSD_DIR}/llvm \
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

.PRECIOUS: ${BSD_DIR}/clang
.PRECIOUS: ${BSD_DIR}/compiler-rt
.PRECIOUS: ${BSD_DIR}/libcxx
.PRECIOUS: ${BSD_DIR}/libcxxabi
.PRECIOUS: ${BSD_DIR}/libunwind
.PRECIOUS: ${BSD_DIR}/llvm
.PRECIOUS: ${BSD_DIR}/glog
.PRECIOUS: ${BSD_DIR}/gtest
.PRECIOUS: ${BSD_DIR}/rapidjson

do-checkout-llvm:
	# No way of adding from a top level submodules within sub-
	# modules, so stickying to this method.
	git submodule update --init --recursive ${BSD_DIR}/llvm ${BSD_DIR}/clang ${BSD_DIR}/compiler-rt ${BSD_DIR}/libcxx ${BSD_DIR}/libcxxabi ${BSD_DIR}/libunwind
	ln -sf ../../clang ${BSD_DIR}/llvm/tools/clang
	ln -sf ../../compiler-rt ${BSD_DIR}/llvm/projects/compiler-rt
	ln -sf ../../libcxx ${BSD_DIR}/llvm/projects/libcxx
	ln -sf ../../libcxxabi ${BSD_DIR}/llvm/projects/libcxxabi
	ln -sf ../../libunwind ${BSD_DIR}/llvm/projects/libunwind
	# for CUDA 9.1+ support on LLVM 6:
	#   git cherry-pick ccacb5ddbcbb10d9b3a4b7e2780875d1e5537063
	cd ${BSD_DIR}/llvm/tools/clang && git cherry-pick ccacb5ddbcbb10d9b3a4b7e2780875d1e5537063
	# for CUDA 9.2 support on LLVM 6:
	#   git cherry-pick 5f76154960a51843d2e49c9ae3481378e09e61ef
	cd ${BSD_DIR}/llvm/tools/clang && git cherry-pick 5f76154960a51843d2e49c9ae3481378e09e61ef

#######################################################################
# Clean targets
#######################################################################

#######################################################################
# Makefile utils / Generic targets
#######################################################################
ifeq (${VERBOSE},0)
# Do not echo the commands before executing them.
.SILENT:
endif

.PHONY: help
help:
	@echo "-----------------------------------------------------------------------"
	@echo "The general commands are available:"
	@echo " * show-config		Display configuration variables such as paths,"
	@echo " 			number of jobs and other tunable options."
	@echo " * clean 		Remove trireme object files and binaries."
	@echo " * dist-clean		Cleans the repository to a pristine state,"
	@echo " 			just like after a new clone of the sources."
	@echo "-----------------------------------------------------------------------"
	@echo " In the following targets, '%' can be replaced by one of the external"
	@echo " project among the following list: llvm, rapidjson, glog, gtest"
	@echo ""
	@echo " * clean-%		Removes the object files of '%'"
	@echo " * dist-clean-%		Removes everything from project '%', forcing a"
	@echo " 			build from scratch of '%'."
	@echo "-----------------------------------------------------------------------"

.PHONY: show-config
show-config:
	@echo "-----------------------------------------------------------------------"
	@echo "Configuration:"
	@echo "-----------------------------------------------------------------------"
	@echo "PROJECT_DIR		:= ${PROJECT_DIR}"
	@echo "SRC_DIR			:= ${SRC_DIR}"
	@echo "EXTERNAL_DIR		:= ${EXTERNAL_DIR}"
	@echo "BSD_DIR			:= ${BSD_DIR}"
	@echo "BUILD_DIR		:= ${BUILD_DIR}"
	@echo "INSTALL_DIR		:= ${INSTALL_DIR}"
	@echo "JOBS			:= ${JOBS}"
	@echo "USER			:= ${USER}"
	@echo "VERBOSE			:= ${VERBOSE}"
	@echo "-----------------------------------------------------------------------"

.PHONY: dist-clean
dist-clean:
	-rm -rf ${EXTERNAL_DIR}
	-git clean -dxf .

.PHONY: clean
clean: clean-raw-jit-executor

PHONY: dist-clean-%
dist-clean-%: clean-%
	-rm -rf  ${EXTERNAL_DIR}/*/$$(echo $@ | sed -e 's,dist-clean-,,')

.PHONY: clean-%
clean-%:
	-rm .$$(echo $@ | sed -e 's,clean-,,').*_done
	-rm -rf  ${BUILD_DIR}/$$(echo $@ | sed -e 's,clean-,,')

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
