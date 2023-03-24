#!/bin/bash

sudo docker network create -d sriov --subnet=172.31.80.0/20 -o netdevice=eth0 mynet
