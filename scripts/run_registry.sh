#!/bin/bash

# Run this script on the server where the docker registry should run

PORT=5000
NAME="rfaas-registry"
if [ $# -gt 0 ]; then
    PORT="$1"
fi
sudo docker run -d -p $PORT:$PORT --restart=always --name $NAME registry:2
echo "started docker registry $NAME on port $PORT"
