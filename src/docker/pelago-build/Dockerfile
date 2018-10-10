FROM nvidia/cuda:9.2-devel-ubuntu18.04

COPY requirements.txt /tmp/requirements.txt

# RUN apt update -y
# RUN apt install -y software-properties-common
# RUN apt-add-repository -y ppa:graphics-drivers/ppa

RUN echo "deb https://dl.bintray.com/sbt/debian /" >> /etc/apt/sources.list.d/sbt.list
RUN apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv 2EE0EA64E40A89B84B2DF73499E82A75642AC823

RUN apt update  -y && \
    apt upgrade -y && \
    apt install -y $(cat /tmp/requirements.txt)

RUN ldconfig /usr/local/cuda/lib64

ENV LD_LIBRARY_PATH=/usr/lib/nvidia-396:$LD_LIBRARY_PATH
