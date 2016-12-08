/**
 * This file is released is under the Patented Algorithm with Software released
 * Agreement. See LICENSE.md for more information, and view the original repo
 * at https://github.com/tbl3rd/Pyramids
 */

#include <exception>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#include <opencv2/opencv.hpp>

#include "VideoSource.hpp"

#define NUM_BUFFERS 2

static inline int
check_return(int ret) {
    if (ret < 0)
        throw std::system_error(errno, std::system_category());
    return ret;
}

mmap_buffer::mmap_buffer(int fd, off_t offset, size_t byte_size) : address(nullptr), bytes(byte_size)
{
    address = mmap (nullptr, byte_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
    if (address == nullptr)
        throw std::system_error(errno, std::system_category());
}

mmap_buffer::~mmap_buffer() {
    if (address == nullptr)
        return;
    munmap(address, bytes);
}

mmap_buffer::mmap_buffer(mmap_buffer&& from) : address(from.address), bytes(from.bytes) {
    from.address = nullptr;
    from.bytes = 0;
}

cv::Size
VideoSource::frameSize() const
{
    if (isFile()) {
        cv::VideoCapture &capture = const_cast<cv::VideoCapture &>(itsFileCapture);
        const int w = capture.get(CV_CAP_PROP_FRAME_WIDTH);
        const int h = capture.get(CV_CAP_PROP_FRAME_HEIGHT);
        return cv::Size(w, h);
    } else {
        return cv::Size(itsWidth, itsHeight);
    }
}

static int
openCamera(int cameraId) {
    char devName[22];
    snprintf(devName, sizeof(devName), "/dev/video%d", cameraId);

    return check_return(open(devName, O_RDWR | O_NOCTTY | O_CLOEXEC));
}

static void
switchToInput(int fd) {
    struct v4l2_input input;
    memset (&input, 0, sizeof(struct v4l2_input));

    input.index = 0;
    if (ioctl (fd, VIDIOC_ENUMINPUT, &input) < 0) {
        if (errno == EINVAL)
            throw std::runtime_error("Cannot select video input 0");
        else
            throw std::system_error(errno, std::system_category());
    }

    int selected = 0;
    check_return(ioctl (fd, VIDIOC_S_INPUT, &selected));
}

void
VideoSource::negotiateFormat() {
    struct v4l2_format format;
    memset (&format, 0, sizeof(struct v4l2_format));

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = itsWidth;
    format.fmt.pix.height = itsHeight;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

    // this colorspace is the sRGB with default whitepoint
    // and full range quantization
    // it's what opencv assumes when it does rgb<->ycbcr
    // transformation, and it's the dumb default colorspace
    // if you don't know anything about colorspaces
    // which is probably right for us
    format.fmt.pix.colorspace = V4L2_COLORSPACE_JPEG;

    check_return(ioctl (itsCameraFd, VIDIOC_S_FMT, &format));

    // the ioctl can return success but the driver is free
    // not to change the format (in which case it must set
    // format to whatever the current format is)
    if (format.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
        throw new std::runtime_error("Failed to set the image format: the driver rejected it.");

    itsWidth = format.fmt.pix.width;
    itsHeight = format.fmt.pix.height;
    itsStride = format.fmt.pix.bytesperline;
}

static void
checkCapabilities(int fd) {
    struct v4l2_capability cap;
    memset (&cap, 0, sizeof(struct v4l2_capability));

    check_return(ioctl (fd, VIDIOC_QUERYCAP, &cap));

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        throw std::runtime_error("Selected device is not a camera");

    if (!(cap.capabilities & V4L2_CAP_STREAMING))
        throw std::runtime_error("Selected device does not support streaming");
}

static void
setCameraFps(int fd, int fps) {
    struct v4l2_streamparm setfps;
    memset (&setfps, 0, sizeof(struct v4l2_streamparm));

    setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    setfps.parm.capture.timeperframe.numerator = 1;
    setfps.parm.capture.timeperframe.denominator = fps;

    check_return(ioctl (fd, VIDIOC_S_PARM, &setfps));
}

void
VideoSource::startStreaming() {
    struct v4l2_requestbuffers request;
    memset (&request, 0, sizeof(struct v4l2_requestbuffers));

    // request two single plane buffers
    request.count = NUM_BUFFERS;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;

    // allocate the buffers
    check_return(ioctl (itsCameraFd, VIDIOC_REQBUFS, &request));

    for (unsigned i = 0; i < request.count; i++) {
        struct v4l2_buffer buffer_info;
        memset (&buffer_info, 0, sizeof(struct v4l2_buffer));

        buffer_info.type = request.type;
        buffer_info.memory = V4L2_MEMORY_MMAP;
        buffer_info.index = i;

        // retrieve the buffer offset to pass to mmap, and its size
        check_return(ioctl (itsCameraFd, VIDIOC_QUERYBUF, &buffer_info));

        // mmap the buffer (by constructing a mmap_buffer struct in the buffers vector)
        itsBuffers.emplace_back(itsCameraFd, buffer_info.m.offset, buffer_info.length);

        // add the empty (mapped) buffer in the queue
        check_return(ioctl (itsCameraFd, VIDIOC_QBUF, &buffer_info));
    }

    int type = V4L2_MEMORY_MMAP;
    check_return(ioctl (itsCameraFd, VIDIOC_STREAMON, &type));
}

VideoSource::VideoSource(int id, const std::string &fileName_value, int fps_value, int width, int height)
    : itsFileCapture()
    , itsCameraFd(id >= 0 ? openCamera(id) : -1)
    , itsWidth(width)
    , itsHeight(height)
    , itsStride(2 * width)
    , itsNextBuffer(0)
    , itsCurrentBuffer(-1)
{
    if (id >= 0) {
        checkCapabilities(itsCameraFd);
        switchToInput(itsCameraFd);
        negotiateFormat();
        setCameraFps(itsCameraFd, fps_value);
        startStreaming();
    } else {
        itsFileCapture.open(fileName_value);
        if (!itsFileCapture.isOpened())
            throw std::runtime_error("Failed to open file for reading.");
    }
}

VideoSource::~VideoSource() {
    if (itsCameraFd >= 0)
        close(itsCameraFd);
}

bool
VideoSource::read(cv::Mat& into) {
    if (isFile()) {
        cv::Mat tmp;
        if (!itsFileCapture.read(tmp))
            return false;
        cv::Mat ycbcr;
        cv::cvtColor(tmp, ycbcr, cv::COLOR_RGB2YCrCb);

        cv::Mat channels[3];
        cv::split(ycbcr, channels);
        channels[0].copyTo(into);
        return true;
    }

    struct v4l2_buffer buffer_info;

    // release the current buffer
    if (itsCurrentBuffer >= 0) {
        memset (&buffer_info, 0, sizeof(struct v4l2_buffer));
        buffer_info.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer_info.memory = V4L2_MEMORY_MMAP;
        buffer_info.index = itsCurrentBuffer;

        // let the driver fill the next buffer while we process this one
        check_return(ioctl (itsCameraFd, VIDIOC_QBUF, &buffer_info));
    }

    memset (&buffer_info, 0, sizeof(struct v4l2_buffer));

    buffer_info.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer_info.memory = V4L2_MEMORY_MMAP;
    buffer_info.index = itsNextBuffer;

    // wait until the buffer is filled, and lock it
    check_return(ioctl (itsCameraFd, VIDIOC_DQBUF, &buffer_info));

    // make a mat out of the buffer
    mmap_buffer& mmapped = itsBuffers[itsNextBuffer];
    // The buffer is Y_0 Cb_0 Y_1 Cr_1 Y_2 Cb_2 etc (Cb/Cr are subsampled)
    // in practice, we ignore all chroma components, so we effectively
    // have two bytes per pixel
    cv::Mat frame(itsHeight, itsWidth, CV_8UC2, mmapped.get(), itsStride);
    cv::Mat channels[2];
    cv::split(frame, channels);
    into = channels[0];

    itsCurrentBuffer = itsNextBuffer;
    itsNextBuffer = (itsNextBuffer + 1) % itsBuffers.size();

    return true;
}
