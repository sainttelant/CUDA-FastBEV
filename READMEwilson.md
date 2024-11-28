1.docker build
docker build --build-arg TMPDIR=/home/wilson/disklarge/tmp -t wilson-jetson-orin-fastbev .

2. docker run 
docker run -it --rm --runtime nvidia --gpus all --name wilson_fastbev -v /home/wilson/disklarge:/workspace wilson-jetson-orin-fastbev

