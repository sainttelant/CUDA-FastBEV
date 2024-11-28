# 基于 JetPack 6.0
FROM nvcr.io/nvidia/l4t-tensorrt:r8.6.2-devel


# no need to interact with manually press Y
ENV DEBIAN_FRONTEND = noninteractive

RUN apt-get update && apt-get install -y \
    git \
    autoconf \
    automake \
    libtool \
    curl \
    make \
    g++ \
    unzip \
    pkg-config \
    libatlas-base-dev \
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev \
    libv4l-dev \
    libxvidcore-dev \
    libx264-dev \
    libgtk-3-dev \
    libcanberra-gtk3-module \
    libboost-all-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgtkglext1 \
    libgtkglext1-dev \
    #libpython3.6-dev \
    #libpython3.6-numpy \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN git config --global http.postBuffer 1048576000 \
    && git config --global http.lowSpeedLimit 0 \
    && git config --global http.lowSpeedTime 999999 \
    && git config --global http.maxRequestBuffer 100M

# 下载 OpenCV 和 OpenCV Contrib 源代码
RUN git clone --depth 1 https://github.com/opencv/opencv.git /opencv && \
    git clone --depth 1 https://github.com/opencv/opencv_contrib.git /opencv_contrib && \
    cd /opencv && \
    git checkout 4.x && \
    cd /opencv_contrib && \
    git checkout 4.x

# 创建并进入构建目录
RUN cd /opencv && mkdir build && cd build && \
    cmake -D CMAKE_BUILD_TYPE=Release \
          -D CMAKE_INSTALL_PREFIX=/usr/local \
          -D OPENCV_EXTRA_MODULES_PATH=/opencv_contrib/modules \
          -D WITH_CUDA=OFF \
          -D WITH_CUDNN=OFF \
          -D OPENCV_DNN_CUDA=OFF \
          -D ENABLE_FAST_MATH=1 \
          -D CUDA_FAST_MATH=1 \
          -D WITH_CUBLAS=1 \
          -D WITH_GSTREAMER=OFF \
          -D WITH_LIBV4L=OFF \
          -D BUILD_opencv_python3=OFF \
          -D BUILD_opencv_python2=OFF \
          #-D PYTHON3_EXECUTABLE=/usr/bin/python3 \
          #-D PYTHON3_INCLUDE_DIR=/usr/include/python3.6 \
          #-D PYTHON3_LIBRARIES=/usr/lib/x86_64-linux-gnu/libpython3.6m.so \
          #-D PYTHON3_NUMPY_INCLUDE_DIRS=/usr/lib/python3/dist-packages/numpy/core/include \
          .. && \
    make -j16 && \
    make install && \
    ldconfig

RUN git clone https://github.com/protocolbuffers/protobuf.git /protobuf \
    && cd /protobuf \
    && git checkout -b bevbuffer v3.6.1 \
    && git submodule update --init --recursive \
    && ./autogen.sh \
    && ./configure \
    && make -j16 \
    && make check \
    && make install \
    && ldconfig \
    && cd .. \
    && rm -rf /protobuf

# 更新和安装基本工具
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-dev \
    python3-setuptools \
    python3-wheel \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*


# 确认 Python 版本
RUN python --version

# 安装需要的 Python 包
RUN python3 -m pip install --upgrade pip \
    && pip3 list

# 如果有其他 Python 包需要安装，请在下面添加
# RUN pip3 install <other-packages>

# 设置工作目录
WORKDIR /workspace

# 设置环境变量
ENV PATH /usr/local/cuda/bin:${PATH}
ENV LD_LIBRARY_PATH /usr/local/cuda/lib64:${LD_LIBRARY_PATH}
