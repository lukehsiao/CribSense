#ifndef RIESZ_TRANSFORM_H_INCLUDED
#define RIESZ_TRANSFORM_H_INCLUDED

#include <opencv2/opencv.hpp>

#include "Butterworth.hpp"
#include "ComplexMat.hpp"


// One level of a Riesz Transform (R) Laplacian Pyramid (Lp).
//
class RieszPyramidLevel {

    RieszPyramidLevel &operator=(const RieszPyramidLevel &);

public:

    cv::Mat itsLp;                     // the frame scaled to this octave
    ComplexMat itsR;                   // the transform
    CompExpMat itsPhase;               // the amplified result
    CompExpMat itsRealPass;            // per-level filter state maintained
    CompExpMat itsImagPass;            // across frames

    void build(const cv::Mat &octave) {
        static const cv::Mat realK = (cv::Mat_<float>(1, 3) << -0.6, 0, 0.6);
        static const cv::Mat imagK = realK.t();
        itsLp = octave;
        cv::filter2D(itsLp, real(itsR), itsLp.depth(), realK);
        cv::filter2D(itsLp, imag(itsR), itsLp.depth(), imagK);
    }

    // Write into result the element-wise inverse cosine of X.
    //
    static void arcCosX(const cv::Mat &X, cv::Mat &result) {
        assert(X.isContinuous() && result.isContinuous());
        const float *const pX =      X.ptr<float>(0);
        float *const pResult  = result.ptr<float>(0);
        const int count = X.rows * X.cols;
        for (int i = 0; i < count; ++i) pResult[i] = acos(pX[i]);
    }

    void unwrapOrientPhase(const RieszPyramidLevel &prior) {
        cv::Mat temp1
            =      itsLp.mul(prior.itsLp)
            + real(itsR).mul(real(prior.itsR))
            + imag(itsR).mul(imag(prior.itsR));
        cv::Mat temp2
            =       real(itsR).mul(prior.itsLp)
            - real(prior.itsR).mul(itsLp);
        cv::Mat temp3
            =       imag(itsR).mul(prior.itsLp)
            - imag(prior.itsR).mul(itsLp);
        cv::Mat tempP  = temp2.mul(temp2) + temp3.mul(temp3);
        cv::Mat phi    = tempP            + temp1.mul(temp1);
        cv::sqrt(phi, phi);
        cv::divide(temp1, phi, temp1);
        arcCosX(temp1, phi);
        cv::sqrt(tempP, tempP);
        cv::divide(temp2, tempP, temp2);
        cv::divide(temp3, tempP, temp3);
        cos(itsPhase) = temp2.mul(phi);
        sin(itsPhase) = temp3.mul(phi);
    }

    // Write into result the element-wise cosines and sines of X.
    //
    static void cosSinX(const cv::Mat &X, CompExpMat &result)
    {
        assert(X.isContinuous());
        cos(result) = cv::Mat::zeros(X.size(), CV_32F);
        sin(result) = cv::Mat::zeros(X.size(), CV_32F);
        assert(cos(result).isContinuous() && sin(result).isContinuous());
        const float *const pX =           X.ptr<float>(0);
        float *const pCosX    = cos(result).ptr<float>(0);
        float *const pSinX    = sin(result).ptr<float>(0);
        const int count = X.rows * X.cols;
        for (int i = 0; i < count; ++i) {
            pCosX[i] = cos(pX[i]);
            pSinX[i] = sin(pX[i]);
        }
    }

    cv::Mat rms() {
        cv::Mat result = square(itsR) + itsLp.mul(itsLp);
        cv::sqrt(result, result);
        return result;
    }

    // Normalize the phase change of this level into result.
    //
    void normalize(CompExpMat &result) {
        static const double sigma = 3.0;
        static const int aperture = 1 + 4 * sigma;
        static const cv::Mat kernel
            = cv::getGaussianKernel(aperture, sigma, CV_32F);
        cv::Mat amplitude = rms();
        const CompExpMat change = itsRealPass - itsImagPass;
        cos(result) = cos(change).mul(amplitude);
        sin(result) = sin(change).mul(amplitude);
        cv::sepFilter2D(cos(result), cos(result), -1, kernel, kernel);
        cv::sepFilter2D(sin(result), sin(result), -1, kernel, kernel);
        cv::Mat temp;
        cv::sepFilter2D(amplitude, temp, -1, kernel, kernel);
        cv::divide(cos(result), temp, cos(result));
        cv::divide(sin(result), temp, sin(result));
    }

    // Multipy the phase difference in this level by alpha but only up to
    // some ceiling threshold.
    //
    void amplify(double alpha, double threshold) {
        CompExpMat temp; normalize(temp);
        cv::Mat MagV = square(temp);
        cv::sqrt(MagV, MagV);
        cv::Mat MagV2 = MagV * alpha;
        cv::threshold(MagV2, MagV2, threshold, 0, cv::THRESH_TRUNC);
        CompExpMat phaseDiff; cosSinX(MagV2, phaseDiff);
        cv::Mat pair = real(itsR).mul(cos(temp)) + imag(itsR).mul(sin(temp));
        cv::divide(pair, MagV, pair);
        itsLp = itsLp.mul(cos(phaseDiff)) - pair.mul(sin(phaseDiff));
    }

    // Default the constructors because these are in a vector<>.
};


// Riesz Pyramid
//
class RieszPyramid {

    RieszPyramid &operator=(const RieszPyramid &);
    RieszPyramid(const RieszPyramid &);

public:

    typedef std::vector<RieszPyramidLevel>::size_type size_type;

    std::vector<RieszPyramidLevel> itsLevel;

    void build(const cv::Mat &frame) {
        const RieszPyramid::size_type max = itsLevel.size() - 1;
        cv::Mat octave = frame;
        for (RieszPyramid::size_type i = 0; i < max; ++i) {
            cv::Mat down, up;
            cv::pyrDown(octave, down);
            cv::pyrUp(down, up, octave.size());
            itsLevel[i].build(octave - up);
            octave = down;
        }
        itsLevel[max].build(octave);
    }

    void unwrapOrientPhase(const RieszPyramid &prior) {
        const RieszPyramid::size_type max = itsLevel.size() - 1;
        for (RieszPyramid::size_type i = 0; i < max; ++i) {
            itsLevel[i].unwrapOrientPhase(prior.itsLevel[i]);
        }
    }

    // Amplify motion by alpha up to threshold using filtered phase data.
    //
    void amplify(double alpha, double threshold)
    {
        RieszPyramid::size_type i = itsLevel.size() - 1;
        while (i--) itsLevel[i].amplify(alpha, threshold);
    }

    // Return the frame resulting from the collapse of this pyramid.
    //
    cv::Mat collapse() const {
        const int count = itsLevel.size() - 1;
        cv::Mat result = itsLevel[count].itsLp;
        for (int i = count - 1; i >= 0; --i) {
            const cv::Mat &octave = itsLevel[i].itsLp;
            cv::Mat up; cv::pyrUp(result, up, octave.size());
            result = up + octave;
        }
        return result;
    }

    static int countLevels(const cv::Size &size)
    {
        if (size.width > 5 && size.height > 5) {
            const cv::Size halved((1 + size.width) / 2, (1 + size.height) / 2);
            return 1 + countLevels(halved);
        }
        return 0;
    }

    // Initialize levels here because cannot do that through vector<>.
    //
    RieszPyramid(const cv::Mat &frame)
        : itsLevel(countLevels(frame.size()))
    {
        build(frame);
        const size_type count = itsLevel.size();
        for (size_type i = 0; i < count; ++i) {
            RieszPyramidLevel &rpl = itsLevel[i];
            const cv::Size size = rpl.itsLp.size();
            cos(rpl.itsPhase)    = cv::Mat::zeros(size, CV_32F);
            sin(rpl.itsPhase)    = cv::Mat::zeros(size, CV_32F);
            cos(rpl.itsRealPass) = cv::Mat::zeros(size, CV_32F);
            sin(rpl.itsRealPass) = cv::Mat::zeros(size, CV_32F);
            cos(rpl.itsImagPass) = cv::Mat::zeros(size, CV_32F);
            sin(rpl.itsImagPass) = cv::Mat::zeros(size, CV_32F);
        }
    }
};


// A low-pass or high-pass filter.
//
class RieszTemporalFilter {

    RieszTemporalFilter &operator=(const RieszTemporalFilter &);
    RieszTemporalFilter(const RieszTemporalFilter &);

public:

    double itsFrequency;
    std::vector<double> itsA;
    std::vector<double> itsB;

    // Compute this filter's Butterworth coefficients for the sampling
    // frequency, fps (frames per second).
    //
    void computeCoefficients(double halfFps)
    {
        const double Wn = itsFrequency / halfFps;
        butterworth(1, Wn, itsA, itsB);
    }

    void passEach(cv::Mat &result,
                  const cv::Mat &phase,
                  const cv::Mat &prior) const {
        result
            = itsB[0] * phase
            + itsB[1] * prior
            - itsA[1] * result;
        result /= itsA[0];
    }
    void pass(CompExpMat &result,
              const CompExpMat &phase,
              const CompExpMat &prior) const {
        passEach(cos(result), cos(phase), cos(prior));
        passEach(sin(result), sin(phase), sin(prior));
    }

    RieszTemporalFilter(double f): itsFrequency(f), itsA(), itsB() {}
};


// A temporal bandpass filter comprising a low-cut and high-cut filter.
//
class RieszTemporalBandpass {

    RieszTemporalBandpass &operator=(const RieszTemporalBandpass &);

public:

    double itsFps;
    RieszTemporalFilter itsLoCut;
    RieszTemporalFilter itsHiCut;

    // Recompute the Butterworth coefficients for current cut-off
    // frequencies and sampling frequency.
    //
    void computeFilter()
    {
        const double halfFps = itsFps / 2.0;
        itsLoCut.computeCoefficients(halfFps);
        itsHiCut.computeCoefficients(halfFps);
    }

    void lowCutoff(double frequency) {
        if (frequency <= itsHiCut.itsFrequency) {
            itsLoCut.itsFrequency = frequency;
            computeFilter();
        }
    }

    void highCutoff(double frequency) {
        if (frequency >= itsLoCut.itsFrequency) {
            itsHiCut.itsFrequency = frequency;
            computeFilter();
        }
    }

    // Shift the current filter state to the prior filter state.
    // The prior itsRealPass and itsImagPass are never referenced.
    //
    void shift(const RieszPyramidLevel &current, RieszPyramidLevel &prior)
    {
        /**/   current.itsLp      .copyTo(        prior.itsLp   );
        real ( current.itsR     ) .copyTo( real ( prior.itsR     ));
        imag ( current.itsR     ) .copyTo( imag ( prior.itsR     ));
        cos  ( current.itsPhase ) .copyTo( cos  ( prior.itsPhase ));
        sin  ( current.itsPhase ) .copyTo( sin  ( prior.itsPhase ));
    }

    void filterLevel(RieszPyramidLevel &current, RieszPyramidLevel &prior)
    {
        itsHiCut.pass(current.itsRealPass, current.itsPhase, prior.itsPhase);
        itsLoCut.pass(current.itsImagPass, current.itsPhase, prior.itsPhase);
        shift(current, prior);
    }

    void filterPyramids(RieszPyramid &current, RieszPyramid &prior)
    {
        assert(current.itsLevel.size() == prior.itsLevel.size());
        const RieszPyramid::size_type count = current.itsLevel.size() - 1;
        for (RieszPyramid::size_type i = 0; i < count; ++i) {
            filterLevel(current.itsLevel[i], prior.itsLevel[i]);
        }
        shift(current.itsLevel[count], prior.itsLevel[count]);
    }

    RieszTemporalBandpass(const RieszTemporalBandpass &that)
        : itsFps(that.itsFps)
        , itsLoCut(that.itsLoCut.itsFrequency)
        , itsHiCut(that.itsHiCut.itsFrequency)
    {}

    RieszTemporalBandpass() : itsFps(0.0), itsLoCut(0.0), itsHiCut(0.0) {}
};


class RieszTransform {

    RieszTransform &operator=(const RieszTransform &);

    cv::Mat itsFrame;
    double itsAlpha;
    double itsThreshold;
    RieszTemporalBandpass itsBand;
    RieszPyramid *itsCurrent;
    RieszPyramid *itsPrior;

public:

    ~RieszTransform() { delete itsPrior; delete itsCurrent; }

    // Set the frames per second which is the filter sampling frequency.
    //
    void fps(double fps) { itsBand.itsFps = fps; itsBand.computeFilter(); }

    // Set the low or high cut-off frequency of the bandpass filter.
    //
    void lowCutoff(double frequency)  { itsBand.lowCutoff(frequency);  }
    void highCutoff(double frequency) { itsBand.highCutoff(frequency); }

    // Set the amplification (alpha parameter) to value.
    //
    void alpha(int value)             { itsAlpha = value; }

    // Truncate the maximum phase difference to t.
    //
    void threshold(int t)             { itsThreshold = t; }

    // Return copy of frame with motion magnified.
    //
    cv::Mat transform(const cv::Mat &frame) {
        static const double PI_PERCENT = M_PI / 100.0;
        static const double scaleFactor = 1.0 / 255.0;
        frame.convertTo(itsFrame, CV_32F, scaleFactor);
        cv::cvtColor(itsFrame, itsFrame, cv::COLOR_RGB2YCrCb);
        std::vector<cv::Mat> channels; cv::split(itsFrame, channels);
        cv::Mat result;
        if (itsCurrent) {
            itsCurrent->build(channels[0]);
            itsCurrent->unwrapOrientPhase(*itsPrior);
            itsBand.filterPyramids(*itsCurrent, *itsPrior);
            itsCurrent->amplify(itsAlpha, itsThreshold * PI_PERCENT);
            channels[0] = itsCurrent->collapse();
            cv::merge(channels, result);
            cv::cvtColor(result, result, cv::COLOR_YCrCb2RGB);
            result.convertTo(result, CV_8UC3, 255);
        } else {
            itsCurrent = new RieszPyramid(channels[0]);
            itsPrior   = new RieszPyramid(channels[0]);
            frame.copyTo(result);
        }
        return result;
    }

    // Construct this transform with the same settings as that.
    //
    RieszTransform(const RieszTransform &that)
        : itsFrame()
        , itsAlpha(that.itsAlpha)
        , itsThreshold(that.itsThreshold)
        , itsBand(that.itsBand)
        , itsCurrent(0)
        , itsPrior(0)
    {
        this->fps(itsBand.itsFps);
    }

    RieszTransform()
        : itsFrame()
        , itsAlpha(0.0)
        , itsThreshold(0.0)
        , itsBand()
        , itsCurrent(0)
        , itsPrior(0)
    {}
};

#endif // #ifndef RIESZ_TRANSFORM_H_INCLUDED
