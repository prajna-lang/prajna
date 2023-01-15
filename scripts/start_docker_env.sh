cd dockerfiles
docker build . -f ubuntu_dev.dockerfile --tag ubuntu_dev
cd -
docker run -it -v $(pwd):$(pwd) -w $(pwd) --network host ubuntu_dev
