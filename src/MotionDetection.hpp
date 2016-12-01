#ifndef MOTIONDETECTION_H_INCLUDED
#define MOTIONDETECTION_H_INCLUDED

#include <future>
#include <opencv2/opencv.hpp>
#include <canberra.h>

#include "CommandLine.hpp"
#include "RieszTransform.hpp"
#include "VideoSource.hpp"
#include "WorkerThread.hpp"

#define MINIMUM_FRAMES 3
#define SPLIT 3
#define NSEC_PER_SEC 1000000

// Types of frame sizes for reinitializing the Riesz FPS.
enum frame_size {
    FULL_FRAME,
    CROPPED_FRAME
};

class MotionDetection {

private:
    cv::Mat frameBuffer[3];
    int frameCount;
    cv::Mat erodeKernel;
    cv::Mat dilateKernel;
    cv::Mat evaluation;
    cv::Mat accumulator;
    cv::Rect roi;
    double full_fps;
    double crop_fps;
    double input_fps;
    bool usingCamera;
    int diffThreshold;
    bool showDiff;
    bool showMagnification;
    bool crop;
    int pixelThreshold;
    int motionDuration;
    int frameWidth;
    int frameHeight;
    unsigned timeToAlarm;
    unsigned framesToSettle;
    unsigned roiUpdateInterval;
    unsigned roiWindow;
    double breathingRate;
    RieszTransform rt[SPLIT];
    WorkerThread<cv::Mat, RieszTransform*, cv::Mat> thread[SPLIT];
    ca_context *snd_context;

    /**
     * Use simple image diffs over 3 frames to create a black/white evaulation
     * image where white pixels indicate pixels that have changed.
     */
    void DifferentialCollins();

    /**
     * Erodes and dialates the motion pixels and determines where the
     * largest area of motion is in the frame.
     */
    void calculateROI();

    /**
     * Performs video magnification based on MIT's work. Requires that the
     * frame buffer is filled with frames of the same size.
     * @param  frame Input image to magnify.
     * @return       Magnified version of the frame.
     */
    cv::Mat magnifyVideo(cv::Mat frame);

    /**
     * Push a new frame to the frame buffer.
     * @param newFrame [description]
     */
    void pushFrameBuffer(cv::Mat newFrame);

    /**
     * Reset the ReiszTransforms. Uses the input frame just to get the
     * correct sizes.
     * @param frame Input frame to reference for sizing.
     */
    void reinitializeReisz(cv::Mat frame, frame_size size);

    /**
     * Accumulate the bitwise OR in the accumulator each time it is called.
     */
    void monitorMotion();

    /**
     * Return the number of pixel differences that pass the duration and
     * difference thresholds for the current frame.
     */
    unsigned countNumChanges();

    /**
     * When a peak is detected, this is called so that the times can be logged
     * to calculate the breathing rate.
     */
    void calculatePeriod();

    /**
     * Returns the current estimate breathing rate based on the pixel differences
     * over a short time history.
     * @return Approximate breathing rate.
     */
    double getBreathingRate();

    /**
     * If no motion is detected for a period of time, call this function to
     * sound an alarm!
     */
    void soundAlarm();

public:

    /**
     * Operates as the tick function of the state machine. Drives the state
     * machine every time a new frame is provided from the video.
     * @param newFrame Next unprocessed video frame.
     */
    void update(cv::Mat newFrame);

    /**
     * Constructor sets motion detection params based on what was provided by
     * the user.
     */
    MotionDetection(const CommandLine &cl);
};

#endif // #ifndef MOTIONDETECTION_H_INCLUDED
