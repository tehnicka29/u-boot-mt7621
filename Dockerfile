FROM --platform=linux/i386 debian:bullseye-slim

ADD toolchain/mips-2012.03.tar.xz /opt/toolchain

RUN apt update && apt install -y libc6 libstdc++6 \
        make libncurses-dev gcc zlib1g-dev \
        xxd bc

WORKDIR /src
RUN umask 0002

#ADD profiles toolchain uboot-5.x.x.x ./


