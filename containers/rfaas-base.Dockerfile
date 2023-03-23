# Container version should be same ubuntu version as where `executor`
# was built (due to glibc versioning)

FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update -y && apt-get upgrade -y \
    && apt-get install -y \
    libibverbs-dev librdmacm-dev

RUN mkdir -p /opt/bin
WORKDIR "/opt/bin"

