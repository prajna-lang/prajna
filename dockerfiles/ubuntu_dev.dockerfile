
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -y cmake clang
RUN apt-get install -y libssl-dev libsodium-dev
RUN apt-get install -y uuid-dev
RUN apt-get install -y libgnutls28-dev
RUN apt-get install -y software-properties-common
RUN add-apt-repository ppa:git-core/ppa && apt update && apt install -y git
RUN apt-get install -y valgrind
RUN apt-get install libbsd-dev
RUN touch //.gitconfig && chmod 777 //.gitconfig

