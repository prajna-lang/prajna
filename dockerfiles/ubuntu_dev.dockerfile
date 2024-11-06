
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update
RUN apt-get install -y cmake clang git
RUN apt-get install -y libssl-dev libsodium-dev uuid-dev libgnutls28-dev
RUN apt-get install -y valgrind
RUN apt-get install -y ninja-build
RUN touch //.gitconfig && chmod 777 //.gitconfig

