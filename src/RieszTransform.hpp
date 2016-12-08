#ifndef RIESZ_TRANSFORM_H_INCLUDED
#define RIESZ_TRANSFORM_H_INCLUDED

/**
 * This file is released is under the Patented Algorithm with Software released
 * Agreement. See LICENSE.md for more information, and view the original repo
 * at https://github.com/tbl3rd/Pyramids
 */

#include <opencv2/opencv.hpp>
#include <memory>

struct RieszTransformState;

class RieszTransform {

    RieszTransform &operator=(const RieszTransform &) = delete;

    cv::Mat itsFrame;
    std::unique_ptr<RieszTransformState> state;
    double itsAlpha;
    double itsThreshold;

public:
    void initialize(const cv::Mat &frame);

    // Set the frames per second which is the filter sampling frequency.
    //
    void fps(double fps);

    // Set the low or high cut-off frequency of the bandpass filter.
    //
    void lowCutoff(double frequency);
    void highCutoff(double frequency);

    // Set the amplification (alpha parameter) to value.
    //
    void alpha(int value)             { itsAlpha = value; }

    // Truncate the maximum phase difference to t.
    //
    void threshold(int t)             { itsThreshold = t; }

    // Return copy of frame with motion magnified.
    //
    cv::Mat transform(const cv::Mat &frame);

    RieszTransform();
    RieszTransform(const RieszTransform&);
    ~RieszTransform();
};

#endif // #ifndef RIESZ_TRANSFORM_H_INCLUDED
