#!/bin/bash

# Run this script on the server where you want the docker registry to be hosted
# Recommended to host the registry on the same server as the executor manager

# NOTE: Run this script from the repo root

PORT=5000
NAME="rfaas-registry"

cfg=containers/config
mkdir -p $cfg
sudo -Bc htpasswd $cfg/htpasswd htpasswd $USER

docker-compose up -d -f registry.yaml
echo "started docker registry $NAME on port $PORT"

