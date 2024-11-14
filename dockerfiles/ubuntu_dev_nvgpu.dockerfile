FROM nvidia/cuda:12.6.0-devel-ubuntu24.04
ENV DEBIAN_FRONTEND=noninteractive

RUN add-apt-repository ppa:git-core/ppa
RUN apt-get update
RUN apt-get install -y cmake clang
RUN apt-get install -y libssl-dev libsodium-dev uuid-dev libgnutls28-dev
RUN apt-get install -y valgrind
RUN apt-get install -y ninja-build
RUN apt-get install -y git
RUN touch //.gitconfig && chmod 777 //.gitconfig

