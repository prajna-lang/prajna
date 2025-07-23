set -e

cd dockerfiles
docker build . -f ubuntu_dev_gpu.dockerfile --tag nvidia_ubuntu_dev
cd -
docker run --device /dev/kfd --device /dev/dri --security-opt seccomp=unconfined --gpus all -it -v $(pwd):$(pwd) -w $(pwd) --network host nvidia_ubuntu_dev
