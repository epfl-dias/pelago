PELAGOS_DIR	:= $(shell pwd)
USER		?= $(shell id -un)

SRC_DIR		?= ${PELAGOS_DIR}/src
INSTALL_DIR	?= ${PELAGOS_DIR}/opt
BUILD_DIR	?= ${PELAGOS_DIR}/build

JOBS		?= $$(( $$(grep processor /proc/cpuinfo|tail -1|cut -d: -f2) + 1))

RAW_CHECKOUT	?= "git clone https://${USER}@bitbucket.org/manolee/raw-jit-executor.git"
RAW_REVISION	?= ""

RAPIDJSON_CHECKOUT ?= "git clone https://github.com/miloyip/rapidjson"
RAPIDJSON_REVISION ?= "-b v1.0.2"

GTEST_CHECKOUT	?= "git clone https://github.com/google/googletest.git"
GTEST_REVISION	?= "-b release-1.7.0"

GLOG_CHECKOUT	?= "git clone https://github.com/google/glog.git"
GLOG_REVISION	?= "-b v0.3.4"

POSTGRES_CHECKOUT ?= "git clone https://github.com/postgres/postgres.git"
POSTGRES_REVISION ?= ""

LLVM_CHECKOUT	?= "svn co http://llvm.org/svn/llvm-project"
LLVM_REVISION	?= "tags/RELEASE_34/final"

all: postgres raw-jit-executor
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

do-install-raw-jit-executor: raw-jit-executor.build_done
	@[ -e ${INSTALL_DIR}/raw-jit-executor ] || ln -s ${BUILD_DIR}/raw-jit-executor ${INSTALL_DIR}/raw-jit-executor

#######################################################################
# Build targets
#######################################################################
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
		./configure --prefix ${INSTALL_DIR}

do-conf-postgres: postgres.checkout_done llvm
	[ -d ${BUILD_DIR}/postgres ] || mkdir -p ${BUILD_DIR}/postgres
	cd ${BUILD_DIR}/postgres && \
		${COMMON_ENV} \
		${SRC_DIR}/postgres/configure --prefix ${INSTALL_DIR}

do-conf-raw-jit-executor: raw-jit-executor.checkout_done rapidjson glog gtest llvm
	[ -d ${BUILD_DIR}/raw-jit-executor ] || mkdir -p ${BUILD_DIR}/raw-jit-executor
	cd ${BUILD_DIR}/raw-jit-executor && \
		${COMMON_ENV} \
		cmake ${SRC_DIR}/raw-jit-executor \
			-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \

do-conf-rapidjson: rapidjson.checkout_done llvm
	[ -d ${BUILD_DIR}/rapidjson ] || mkdir -p ${BUILD_DIR}/rapidjson
	cd ${BUILD_DIR}/rapidjson && \
		${COMMON_ENV} \
		cmake ${SRC_DIR}/rapidjson/ \
			-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}

do-conf-llvm: llvm.checkout_done
	[ -d ${BUILD_DIR}/llvm ] || mkdir -p ${BUILD_DIR}/llvm
	cd ${BUILD_DIR}/llvm && cmake ${SRC_DIR}/llvm \
		-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DLLVM_ENABLE_ASSERTIONS=On \
		-Wno-dev

#######################################################################
# Checkout sources as needed
#######################################################################
.PRECIOUS: src/raw-jit-executor
src/raw-jit-executor:
	eval ${RAW_CHECKOUT} ${RAW_REVISION} src/raw-jit-executor

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
	eval ${LLVM_CHECKOUT}/compiler-rt/${LLVM_REVISION} src/llvm/tools/compiler-rt
	eval ${LLVM_CHECKOUT}/cfe/${LLVM_REVISION} src/llvm/tools/clang
	eval ${LLVM_CHECKOUT}/libcxx/${LLVM_REVISION} src/llvm/tools/libcxx
	eval ${LLVM_CHECKOUT}/libcxx/${LLVM_REVISION} src/llvm/tools/libcxxabi

#######################################################################
# Makefile utils / Generic targets
#######################################################################
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
	-@for f in glog gtest rapidjson llvm; \
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
	make do-install-$$(echo $@ | sed -e 's,.install_done,,')
	touch $@

.PRECIOUS: %.build_done
%.build_done: %.configure_done
	make do-build-$$(echo $@ | sed -e 's,.build_done,,')
	touch $@

.PRECIOUS: %.configure_done
%.configure_done: %.checkout_done
	make do-conf-$$(echo $@ | sed -e 's,.configure_done,,')
	touch $@

.PRECIOUS: %.checkout_done
%.checkout_done: src/%
	touch $@
