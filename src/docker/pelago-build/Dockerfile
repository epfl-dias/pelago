FROM nvidia/cuda:10.1-devel-ubuntu18.04

COPY requirements.txt /tmp/requirements.txt

# RUN apt update -y
# RUN apt install -y software-properties-common
# RUN apt-add-repository -y ppa:graphics-drivers/ppa

RUN echo "deb https://dl.bintray.com/sbt/debian /" >> /etc/apt/sources.list.d/sbt.list
RUN apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv 2EE0EA64E40A89B84B2DF73499E82A75642AC823

# LLVM repository with the version we are building
# used to install clang-format without building LLVM, for style checks
RUN echo "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-8 main" >> /etc/apt/sources.list.d/llvm.list
RUN apt-key adv --fetch-keys https://apt.llvm.org/llvm-snapshot.gpg.key

RUN apt update  -y && \
    apt upgrade -y && \
    apt install -y --no-install-recommends $(sed s/#.*$//g tmp/requirements.txt) && \
    apt autoremove -y && \
    apt autoclean && \
    rm -rf /var/lib/apt/lists/* /var/log/dpkg.log && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-8 100 && \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-8 100 && \
    update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-8 100 && \
    update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-8 100

# For now, we need clang++-9, as it's the only one recognizing CUDA 10.1 (but also keep LLVM 8 for clang-format)
RUN echo "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic main" >> /etc/apt/sources.list.d/llvm.list && \
    apt update  -y && \
    apt upgrade -y && \
    apt install -y --no-install-recommends clang++-9 llvm-9-dev libc++abi-9-dev&& \
    apt autoremove -y && \
    apt autoclean && \
    rm -rf /var/lib/apt/lists/* /var/log/dpkg.log && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-9 500 && \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-9 500 && \
    update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-9 500

RUN ldconfig /usr/local/cuda/lib64

ENV LD_LIBRARY_PATH=/usr/lib/nvidia-396:$LD_LIBRARY_PATH
