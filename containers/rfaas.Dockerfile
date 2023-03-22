FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive

# Install tools to compile latest version of rFaas
RUN apt-get update -y && apt-get upgrade -y \
    && apt-get install -y \
    git g++ cmake cmake-data pkg-config lsb-release \
    software-properties-common \
    libibverbs-dev librdmacm-dev

# Install pistache
RUN add-apt-repository ppa:pistache+team/unstable -y && \
    apt update -y && apt install -y libpistache-dev

# Build rFaas
WORKDIR "/opt"
RUN git clone https://github.com/spcl/rfaas
WORKDIR "/opt/rfaas"
RUN cmake -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release .
RUN cmake --build . --target executor

RUN cp bin/executor /opt
