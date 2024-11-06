cd dockerfiles
docker build . -f ubuntu_dev_nvgpu.dockerfile --tag nvidia_ubuntu_dev
cd -
docker run --gpus all -it -v $(pwd):$(pwd) -w $(pwd) --network host nvidia_ubuntu_dev
