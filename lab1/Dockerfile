FROM ubuntu:latest

ENV PACKAGES="mpich \
              gcc \
              g++ \
              htop \
              build-essential"

RUN apt-get update\
    && apt-get install -y $PACKAGES \
    && rm -rf /var/lib/apt/lists/*

WORKDIR "/data/code"

CMD ["bash"]