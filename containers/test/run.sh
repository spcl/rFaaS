#!/bin/bash

#DOCKER=docker
DOCKER=docker_rdma_sriov

sudo $DOCKER run --rm --net=host -i --volume /home/ubuntu/rfaas/containers/opt:/opt rfaas-base /opt/executor $1

#sudo docker exec -it base /bin/bash
