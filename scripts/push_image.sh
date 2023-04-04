#!/bin/bash

# Push an image to the local docker registry

if [ $# -lt 3 ]; then
    echo "usage: ./push_image.sh <IMAGE NAME> <REGISTRY NAME or IP> <REGISTRY PORT>";
    exit
fi

IMAGE=$1
IP=$2
PORT=$3
docker push $IP:$PORT/$IMAGE
