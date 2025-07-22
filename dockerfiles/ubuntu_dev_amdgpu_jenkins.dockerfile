FROM rocm/dev-ubuntu-24.04:6.4.2-complete
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -y cmake clang
RUN apt-get install -y libssl-dev libsodium-dev
RUN apt-get install -y uuid-dev
RUN apt-get install -y libgnutls28-dev
RUN apt-get install -y software-properties-common
RUN add-apt-repository ppa:git-core/ppa && apt update && apt install -y git
RUN apt-get install -y valgrind
RUN apt-get install -y ninja-build
RUN apt-get install -y libbsd-dev
RUN apt-get install -y clang-format
RUN apt-get install -y libstdc++-14-dev
RUN apt-get install -y git-lfs
RUN touch //.gitconfig && chmod 777 //.gitconfig

# Add jenkins:jenkins user
ARG GID
ARG UID
ARG UNAME
ENV GROUP_ID=$GID
ENV USER_ID=$UID
ENV USERNAME=$UNAME
RUN mkdir -p /var/lib/jenkins/workspace
RUN groupadd -g $GROUP_ID $USERNAME
RUN useradd -r -u $USER_ID -g $USERNAME -d /home/$USERNAME $USERNAME
RUN chown $USERNAME:$USERNAME /var/lib/jenkins/workspace
USER $USERNAME
WORKDIR /var/lib/jenkins/workspace
