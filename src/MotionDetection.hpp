#ifndef MOTIONDETECTION_H_INCLUDED
#define MOTIONDETECTION_H_INCLUDED

#include <opencv2/opencv.hpp>
#include "CommandLine.hpp"

class MotionDetection {

private:
    cv::Mat frameBuffer[3];
    int frameCount;
    cv::Mat erodeKernel;
    int diffThreshold;
    bool showDiff;
    int pixelThreshold;
    int motionDuration;

    /**
     * Use simple image diffs over 3 frames to create a black/white evaulation
     * image where white pixels indicate pixels that have changed.
     * @return A black/white evaluation image
     */
    cv::Mat DifferentialCollins();

public:

    /**
     * Given a new frame of video, update the frame buffer, use the Differential
     * algorithm to compute an evaluation frame, and return the number of px
     * that have changed.
     * @param  newFrame The new frame of the video
     * @return          The number of pixels that have changed this frame.
     */
    int isValidMotion(cv::Mat newFrame);

    /**
     * Constructor sets motion detection params based on what was provided by
     * the user.
     */
    MotionDetection(const CommandLine &cl);
};

#endif // #ifndef MOTIONDETECTION_H_INCLUDED
