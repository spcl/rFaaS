# rfaas-executor-enriched container
# FROM ubuntu:22.04
# 
# ARG DEBIAN_FRONTEND=noninteractive
# 
# RUN apt-get update -y && apt-get upgrade -y \
#     && apt-get install -y \
#     libibverbs-dev librdmacm-dev software-properties-common 
# 
# RUN add-apt-repository ppa:ubuntu-toolchain-r/test -y
# 
# RUN apt-get install -y \
#     libstdc++6 gcc g++
# 
# RUN apt-get dist-upgrade -y
# 
# WORKDIR "/opt"

FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update -y && apt-get upgrade -y \
    && apt-get install -y \
    libibverbs-dev librdmacm-dev

WORKDIR "/opt"

