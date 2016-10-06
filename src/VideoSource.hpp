#ifndef VIDEO_SOURCE_H_INCLUDED
#define VIDEO_SOURCE_H_INCLUDED

#include <opencv2/highgui/highgui.hpp>


// Just cv::VideoCapture extended for convenience.
//
// const_cast<>(this) to work around get() not being const (as it should).
//
class VideoSource: public cv::VideoCapture {

    const std::string itsFile;
    int itsCameraId;

public:

    // Return true iff the video source is a file.
    //
    bool isFile() const { return itsCameraId < 0; }

    // Return true iff the video source is a camera.
    //
    bool isCamera() const { return !isFile(); }

    // Return "" or the name of the file that is the source of this.
    //
    const std::string &fileName() const { return itsFile; }

    // Return the id of this camera, which is not valid if negative.
    //
    int cameraId() const { return itsCameraId; }

    // Return the frame rate reported by this.
    //
    double fps() const {
        VideoSource *const ncThis = const_cast<VideoSource *>(this);
        const double result = ncThis->get(CV_CAP_PROP_FPS);
        return result;
    }

    // Return the integral 4CC codec for this.  Default to "mp4v".
    //
    int fourCcCodec() const {
        VideoSource *const ncThis = const_cast<VideoSource *>(this);
        const int codec = ncThis->get(CV_CAP_PROP_FOURCC);
        return codec ? codec : CV_FOURCC('m', 'p', '4', 'v');
    }

    // Return 0 or the number of frames in this video source.
    //
    int frameCount() const {
        VideoSource *const ncThis = const_cast<VideoSource *>(this);
        return ncThis->get(CV_CAP_PROP_FRAME_COUNT);
    }

    // Return the size of the video frames from this.
    //
    cv::Size frameSize() const {
        VideoSource *const ncThis = const_cast<VideoSource *>(this);
        const int w = ncThis->get(CV_CAP_PROP_FRAME_WIDTH);
        const int h = ncThis->get(CV_CAP_PROP_FRAME_HEIGHT);
        const cv::Size result(w, h);
        return result;
    }

    // Open the video file named fileName.
    //
    VideoSource(const std::string &fileName)
        : cv::VideoCapture(fileName)
        , itsFile(fileName)
        , itsCameraId(-1)
    {}

    // Open the camera identified by id.
    //
    VideoSource(int id)
        : cv::VideoCapture(id)
        , itsFile()
        , itsCameraId(id)
    {}

    // Open the same video source used by that.
    //
    VideoSource(const VideoSource &that)
        : cv::VideoCapture()
        , itsFile(that.itsFile)
        , itsCameraId(that.itsCameraId)
    {
        if (itsCameraId < 0) {
            this->open(itsFile);
        } else {
            this->open(itsCameraId);
        }
    }

    // If id is negative, open the video file named fileName.
    // Otherwise open the camera identified by id.
    //
    VideoSource(int id, const std::string &fileName)
        : cv::VideoCapture()
        , itsFile(fileName)
        , itsCameraId(id)
    {
        if (itsCameraId < 0) {
            this->open(itsFile);
        } else {
            this->open(itsCameraId);
        }
    }
};

#endif // #ifndef VIDEO_SOURCE_H_INCLUDED
