FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    g++ \
    python3 \
    python3-pip \
    make \
    libmysqlclient-dev \
    default-libmysqlclient-dev

WORKDIR /app

COPY . .

RUN g++ server.cpp -o server -lmysqlclient

EXPOSE 10000

CMD ["./server"]
