llFROM nvidia/cuda:12.6.0-devel-ubuntu24.04
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
RUN apt-get update && apt-get install -y curl
RUN apt-get update && apt-get install -y gnupg2
RUN touch //.gitconfig && chmod 777 //.gitconfig

# --- 添加 AMD ROCm 软件源并安装 ROCm 用户态包 ---
# ROCm 6.4.2，Ubuntu 24.04，官方地址见 https://github.com/ROCm/ROCm-docker/blob/master/dev/Dockerfile-ubuntu-24.04-complete
ARG ROCM_VERSION=6.4.2

# 添加 ROCm apt key 和 noble 源
RUN curl -sL https://repo.radeon.com/rocm/rocm.gpg.key | gpg --dearmor > /usr/share/keyrings/rocm-archive-keyring.gpg && \
    printf "deb [arch=amd64 signed-by=/usr/share/keyrings/rocm-archive-keyring.gpg] https://repo.radeon.com/rocm/apt/${ROCM_VERSION}/ noble main" > /etc/apt/sources.list.d/rocm.list

# 强制 ROCm 包优先用 radeon.com noble 源
RUN printf "Package: *\nPin: origin repo.radeon.com\nPin-Priority: 1001\n" > /etc/apt/preferences.d/rocm

RUN apt-get update && \
    apt-get install -y --no-install-recommends rocm-dev rocm-libs hipblas rocblas lld

RUN groupadd -g 109 render

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
