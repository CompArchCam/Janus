FROM ubuntu:latest
ENV DEBIAN_FRONTEND=noninteractive
RUN apt update
RUN apt install --yes sudo pkg-config
RUN apt install gcc-7 -y
RUN apt install g++-7 -y
RUN apt install python3 -y
RUN apt install -y python3-pip python-dev build-essential
RUN pip3 install regex
RUN apt install cmake -y


RUN adduser --ingroup sudo janus
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 10
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/g++-7 10
RUN echo 'janus ALL=(ALL) NOPASSWD:ALL'>/etc/sudoers
USER janus

COPY Janus /home/janus/workspace
