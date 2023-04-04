#!/bin/bash

# This script builds the rfaas-base docker container, pushes it to the
# registry, sets up a docker volume, and generates example configuration 
# json which should then be updated in config/executor_manager.json.
# The script also configures a docker network for suitable use with
# docker_rdma_sriov

# NOTE: Run this script from repo root, and make sure the sriov docker plugin
# is installed

set -e

if [ $# -lt 2 ]; then
    echo "usage: ./init_docker.sh <REGISTRY IP> <REGISTRY PORT> <NETWORK MODE> [<NETWORK DEVICE> <SUBNET>]"
    exit
fi

REG_IP=$1    # IP or name of the docker registry
REG_PORT=$2  # Port of the docker registry
NET_MODE=$3  # Docker networking mode -- sriov or host
DEVICE=$4    # The RDMA adapter to use for networking
SUBNET=$5    # Subnet for the docker network

IMG_NAME=rfaas-base
REG_IMG=$REG_IP:$REG_PORT/$IMG_NAME

# Build the docker container, login and push to the registry
docker build -t $IMG_NAME - < containers/rfaas-base.Dockerfile
echo "built rfaas-base image"
docker login $REG_IP:$REG_PORT
echo "logged into docker daemon"

if docker push $REG_IMG; then
    echo "ERROR: make sure a docker registry is actually running on $REG_IP:$REG_PORT.
    Start one with scripts/run_registry.sh"
    exit
else
    echo "pushed rfaas-base image to $REG_IMG"
fi

# Set up docker network
net_name=testnet
if ["$NET_MODE" = "sriov"]; then
    docker network create -d sriov --subnet=$SUBNET -o netdevice=$DEVICE $net_name
elif ["$NET_MODE" = "host"]; then
    net_name="host"
else
    echo "ERROR: invalid networking mode $NET_MODE. Valid options are sriov and host."
    exit
fi
echo "set up docker network"

# Configure volume
volume=$(pwd)/volumes/rfaas-test/opt # Do not put a trailing slash
mkdir -p $volume/bin
cp bin/executor $volume/bin
cp examples/libfunctions.so $volume

# Print json to be updated
config=$(jq -n --arg use_docker "true" \
    --arg image "$REG_IMG" \
    --arg network "$net_name" \
    --arg ip "<ip of container (any available ip within $SUBNET)>" \
    --arg volume $volume \
    --arg registry_ip "$REG_IP" \
    --arg registry_port "$REG_PORT" \
    '{
        "image": $image,
        "network": $network,
        "ip": $ip,
        "volume": $volume,
        "registry_ip": $registry_ip,
        "registry_port": $registry_port
    }'
)

echo "Update config/executor_manager.json with"
echo "$config"

