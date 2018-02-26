#! /bin/bash

docker build . --rm=true -t ubuntu-mpich

mkdir -p ./code

docker rm mpi-lab

docker run -v $(pwd)/code:/data/code \
           --name='mpi-lab' \
           --memory='256m' \
           --cpus='1' \
           -it ubuntu-mpich bash