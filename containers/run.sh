#!/bin/bash

sudo docker run -i --volume /home/ubuntu/rfaas/containers/opt:/opt rfaas-base /opt/executor
#sudo docker exec -it base /bin/bash
