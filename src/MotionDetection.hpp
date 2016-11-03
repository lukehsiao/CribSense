#ifndef MOTIONDETECTION_H_INCLUDED
#define MOTIONDETECTION_H_INCLUDED

#include <opencv2/opencv.hpp>
#include "CommandLine.hpp"
#include "RieszTransform.hpp"
#include "VideoSource.hpp"
#include "WorkerThread.hpp"
#include <future>

#define MINIMUM_FRAMES 3
#define SPLIT 3

class MotionDetection {

private:
    cv::Mat frameBuffer[3];
    int frameCount;
    cv::Mat erodeKernel;
    cv::Mat evaluation;
    cv::Mat accumulator;
    cv::Rect roi;
    int diffThreshold;
    bool showDiff;
    int pixelThreshold;
    int motionDuration;
    unsigned framesToSettle;
    unsigned roiUpdateInterval;
    unsigned roiWindow;
    RieszTransform rt[SPLIT];
    WorkerThread<cv::Mat, RieszTransform*, cv::Mat> thread[SPLIT];

    /**
     * Use simple image diffs over 3 frames to create a black/white evaulation
     * image where white pixels indicate pixels that have changed.
     */
    void DifferentialCollins();

    void calculateROI();

    cv::Mat magnifyVideo(cv::Mat frame);

    void pushFrameBuffer(cv::Mat newFrame);

    void reinitializeReisz(cv::Mat frame);

    void monitorMotion();

public:

    void update(cv::Mat newFrame);




    /**
     * Given a new frame of video, update the frame buffer, use the Differential
     * algorithm to compute an evaluation frame, and return the number of px
     * that have changed.
     * @param  newFrame The new frame of the video
     * @return          The number of pixels that have changed this frame.
     */
    int isValidMotion();

    /**
     * Constructor sets motion detection params based on what was provided by
     * the user.
     */
    MotionDetection(const CommandLine &cl);
};

#endif // #ifndef MOTIONDETECTION_H_INCLUDED
