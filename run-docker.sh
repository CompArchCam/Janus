git clone -b master-to-be https://github.com/CompArchCam/Janus.git
sudo docker build -t janus-docker .
sudo docker run -it janus-docker bash
