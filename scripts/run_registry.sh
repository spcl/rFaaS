#!/bin/bash

# Run this script on the server where you want the docker registry to be hosted
# Recommended to host the registry on the same server as the executor manager

# NOTE: Run this script from the repo root

PORT=5000
NAME="rfaas-registry"

set -e

# Make password file if it doesn't already exist
cfg=containers/config
mkdir -p $cfg
if [ -s $cfg/htpasswd ]; then
    echo "htpasswd exists"
else
    sudo htpasswd -Bc $cfg/htpasswd $USER
    echo "created htpasswd file"
fi

# Generate certs to use TLS (if they dont already exist)
if [ -s $cfg/certs/domain.key ]; then
    echo "using certs in $cfg/certs"
else
    mkdir -p $cfg/certs
    openssl genpkey -algorithm RSA -out $cfg/certs/domain.key -aes256

    openssl req -new               \
        -key $cfg/certs/domain.key \
        -out $cfg/certs/domain.csr \
        -addext 'subjectAltName = IP:172.31.82.200'

    openssl x509 -req -days 365        \
        -in $cfg/certs/domain.csr      \
        -signkey $cfg/certs/domain.key \
        -out $cfg/certs/domain.crt
    
    openssl rsa -in $cfg/certs/domain.key -out $cfg/certs/domain.unencrypted.key
    echo "generated certs in $cfg/certs"
fi

# Start registry
sudo docker-compose -f scripts/registry.yaml up -d
echo "started docker registry $NAME on port $PORT"

