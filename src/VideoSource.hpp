#ifndef VIDEO_SOURCE_H_INCLUDED
#define VIDEO_SOURCE_H_INCLUDED

/**
 * This file is released is under the Patented Algorithm with Software released
 * Agreement. See LICENSE.md for more information, and view the original repo
 * at https://github.com/tbl3rd/Pyramids
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/sysmacros.h>
#include <exception>
#include <system_error>

#include <opencv2/highgui/highgui.hpp>

class mmap_buffer {
    void *address;
    size_t bytes;

public:
    mmap_buffer(int fd, off_t offset, size_t size);
    ~mmap_buffer();
    mmap_buffer(const mmap_buffer&) = delete;
    mmap_buffer(mmap_buffer&&);

    size_t size() const { return bytes; }
    const void *get() const { return address; }
    void *get() { return address; }
};

// A wrapper around the V42L API (or cv::VideoCapture for file IO)
class VideoSource {
    const std::string itsFileName;
    cv::VideoCapture itsFileCapture;
    int itsCameraFd;
    int itsWidth, itsHeight, itsStride, itsBufferSize;
    std::vector<mmap_buffer> itsBuffers;
    int itsNextBuffer;
    int itsCurrentBuffer;

    void negotiateFormat();
    void startStreaming();

public:

    // Return true iff the video source is a file.
    //
    bool isFile() const { return itsCameraFd < 0; }

    // Return true iff the video source is a camera.
    //
    bool isCamera() const { return !isFile(); }

    // Return "" or the name of the file that is the source of this.
    //
    const std::string &fileName() const { return itsFileName; }

    // Return the size of the video frames from this.
    //
    cv::Size frameSize() const;

    // If id is negative, open the video file named fileName.
    // Otherwise open the camera identified by id.
    //
    VideoSource(int id, const std::string &fileName, int fps, int width, int height);

    ~VideoSource();

    VideoSource(const VideoSource&) = delete;
    VideoSource(VideoSource&&) = delete;

    // Read the next frame into the provided matrix
    // Only the Y channel is provided, and the result is a CV_8UC1 matrix
    //
    // Returns true if more frames are available in the input source, false when done
    bool read(cv::Mat& into);
};

#endif // #ifndef VIDEO_SOURCE_H_INCLUDED
