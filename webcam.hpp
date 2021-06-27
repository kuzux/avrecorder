#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/v4l2-common.h>
#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#include <system_error>
#include <stdexcept>

class Webcam {
    int fd = 0;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
    bool streamon = false;

    v4l2_buffer buffer_info = {0};
    uint8_t* buf = nullptr;
    size_t buflen;
public:
    // this function allocates memory
    Webcam(const char* devPath) {
        // here's a simple example for webcam capture https://gist.github.com/mike168m/6dd4eb42b2ec906e064d
        // and the reference documentation for it https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/v4l2.html

        fd = open(devPath, O_RDWR);
        if(fd < 0)
            throw std::system_error(errno, std::generic_category(), "open");

        v4l2_capability capability = {0};
        if(ioctl(fd, VIDIOC_QUERYCAP, &capability) < 0)
            throw std::system_error(errno, std::generic_category(), "VIDIOC_QUERYCAP");
        if(capability.capabilities & V4L2_CAP_VIDEO_CAPTURE == 0)
            throw std::invalid_argument("Device does not have video capture capability");

        // TODO: Actually negotiate with the webcam about format and resolution
        // And refresh rate
        v4l2_format image_format = {0};
        image_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        image_format.fmt.pix.width = 640;
        image_format.fmt.pix.height = 480;
        image_format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        image_format.fmt.pix.field = V4L2_FIELD_NONE;
        if(ioctl(fd, VIDIOC_S_FMT, &image_format) < 0)
            throw std::system_error(errno, std::generic_category(), "VIDIOC_S_FMT");
        if(image_format.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
            throw std::runtime_error("Could not set format to yuyv");

        /* Doesn't seem to do anythiong
        v4l2_streamparm parm;
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = 30;
        if(ioctl(fd, VIDIOC_S_PARM, &parm) < 0)
            throw std::system_error(errno, std::generic_category(), "VIDIOC_S_PARM"); */

        // https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/mmap.html#mmap
        v4l2_requestbuffers buffer_req = {0};
        buffer_req.count = 1;
        buffer_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer_req.memory = V4L2_MEMORY_MMAP;
        if(ioctl(fd, VIDIOC_REQBUFS, &buffer_req) < 0)
            throw std::system_error(errno, std::generic_category(), "VIDIOC_REQBUFS");

        // so we can get the mmap offset
        buffer_info.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer_info.memory = V4L2_MEMORY_MMAP;
        buffer_info.index = 0;
        if(ioctl(fd, VIDIOC_QUERYBUF, &buffer_info) < 0)
            throw std::system_error(errno, std::generic_category(), "VIDIOC_QUERYBUF");

        buf = (uint8_t*)mmap(NULL, buffer_info.length, 
            PROT_READ | PROT_WRITE, MAP_SHARED,
            fd, buffer_info.m.offset);
        if(!buf) 
            throw std::system_error(errno, std::generic_category(), "mmap");
        memset(buf, 0, buffer_info.length);
        buflen = buffer_info.length;
        type = buffer_info.type;
    }

    void start() {
        if(ioctl(fd, VIDIOC_STREAMON, &type) < 0)
            throw std::system_error(errno, std::generic_category(), "VIDIOC_STREAMON");
        streamon = true;
    }

    void stop() {
        if(ioctl(fd, VIDIOC_STREAMOFF, &type) < 0)
            throw std::system_error(errno, std::generic_category(), "VIDIOC_STREAMOFF");
    }

    uint8_t* getNextFrame() {
        if(!streamon) throw std::logic_error("The webcam isn't streaming");

        if(ioctl(fd, VIDIOC_QBUF, &buffer_info) < 0)
            throw std::system_error(errno, std::generic_category(), "VIDIOC_QBUF");
        if(ioctl(fd, VIDIOC_DQBUF, &buffer_info) < 0)
            throw std::system_error(errno, std::generic_category(), "VIDIOC_DQBUF");

        return buf;
    }

    ~Webcam() {
        if(buf) munmap(buf, buflen);
        if(streamon) ioctl(fd, VIDIOC_STREAMOFF, &type);
        if(fd > 0) close(fd);
    }
};
