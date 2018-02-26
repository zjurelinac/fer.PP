#! /bin/bash

docker build . --rm=true -t ubuntu-mpich

mkdir -p ./code

docker rm mpi-lab2

docker run -v $(pwd)/code:/data/code \
           --name='mpi-lab2' \
           --memory='256m' \
           -it ubuntu-mpich bash