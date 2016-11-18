
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
#define NUM_PLANES 3

static inline int
check_return(int ret) {
    if (ret < 0)
        throw std::system_error(errno, std::system_category());
    return ret;
}

mmap_buffer::mmap_buffer(int fd, off_t offset, size_t size) : address(nullptr), bytes(size)
{
    address = mmap (nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
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

    // request a multi plane format
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    format.fmt.pix_mp.width = itsWidth;
    format.fmt.pix_mp.height = itsHeight;
    format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;

    // this colorspace is the sRGB with default whitepoint
    // and full range quantization
    // it's what opencv assumes when it does rgb<->ycbcr
    // transformation, and it's the dumb default colorspace
    // if you don't know anything about colorspaces
    // which is probably right for us
    format.fmt.pix_mp.colorspace = V4L2_COLORSPACE_JPEG;

    check_return(ioctl (itsCameraFd, VIDIOC_S_FMT, &format));

    // the ioctl can return success but the driver is free
    // not to change the format (in which case it must set
    // format to whatever the current format is)
    if (format.fmt.pix_mp.pixelformat != V4L2_PIX_FMT_YUV420 ||
        format.fmt.pix_mp.num_planes != NUM_PLANES)
        throw new std::runtime_error("Failed to set the image format: the driver rejected it.");

    itsWidth = format.fmt.pix_mp.width;
    itsHeight = format.fmt.pix_mp.height;

    // the plane we care about is plane 0, the Y plane
    itsStride = format.fmt.pix_mp.plane_fmt[0].bytesperline;
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
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    request.memory = V4L2_MEMORY_MMAP;

    // allocate the buffers
    check_return(ioctl (itsCameraFd, VIDIOC_REQBUFS, &request));

    for (unsigned i = 0; i < request.count; i++) {
        struct v4l2_buffer buffer_info;
        memset (&buffer_info, 0, sizeof(struct v4l2_buffer));

        struct v4l2_plane plane_info[NUM_PLANES];
        memset (&plane_info, 0, sizeof(plane_info));

        buffer_info.type = request.type;
        buffer_info.memory = V4L2_MEMORY_MMAP;
        buffer_info.index = i;

        buffer_info.length = NUM_PLANES;
        buffer_info.m.planes = plane_info;

        // retrieve the buffer offset to pass to mmap, and its size
        check_return(ioctl (itsCameraFd, VIDIOC_QUERYBUF, &buffer_info));

        // ignore all planes but 0 (= Y), no need to mmap Cb/Cr
        // mmap the buffer (by constructing a mmap_buffer struct in the buffers vector)
        itsBuffers.emplace_back(itsCameraFd, buffer_info.m.planes[0].m.mem_offset, buffer_info.m.planes[0].length);

        // add the empty (mapped) buffer in the queue
        check_return(ioctl (itsCameraFd, VIDIOC_QBUF, &buffer_info));
    }

    int type = V4L2_MEMORY_MMAP;
    check_return(ioctl (itsCameraFd, VIDIOC_STREAMON, &type));
}

VideoSource::VideoSource(int id, const std::string &fileName, int fps, int width, int height)
    : itsFileCapture()
    , itsCameraFd(id >= 0 ? openCamera(id) : -1)
    , itsWidth(width)
    , itsHeight(height)
    , itsStride(3 * width)
    , itsNextBuffer(0)
    , itsCurrentBuffer(-1)
{
    if (id >= 0) {
        checkCapabilities(itsCameraFd);
        switchToInput(itsCameraFd);
        negotiateFormat();
        setCameraFps(itsCameraFd, fps);
        startStreaming();
    } else {
        itsFileCapture.open(fileName);
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
        buffer_info.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buffer_info.memory = V4L2_MEMORY_MMAP;
        buffer_info.index = itsCurrentBuffer;

        // let the driver fill the next buffer while we process this one
        check_return(ioctl (itsCameraFd, VIDIOC_QBUF, &buffer_info));
    }

    memset (&buffer_info, 0, sizeof(struct v4l2_buffer));

    buffer_info.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buffer_info.memory = V4L2_MEMORY_MMAP;
    buffer_info.index = itsNextBuffer;

    // wait until the buffer is filled, and lock it
    check_return(ioctl (itsCameraFd, VIDIOC_DQBUF, &buffer_info));

    // make a mat out of the buffer
    mmap_buffer& mmapped = itsBuffers[itsNextBuffer];
    cv::Mat frame(itsHeight, itsWidth, CV_8UC3, mmapped.get(), itsStride);

    itsCurrentBuffer = itsNextBuffer;
    itsNextBuffer = (itsNextBuffer + 1) % itsBuffers.size();

    into = std::move(frame);
    return true;
}

