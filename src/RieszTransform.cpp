/**
 * This file is released is under the Patented Algorithm with Software released
 * Agreement. See LICENSE.md for more information, and view the original repo
 * at https://github.com/tbl3rd/Pyramids
 */

#include <cstdlib>
#include <functional>
#include <algorithm>
#include <memory>

#include "RieszTransform.hpp"

#include "Butterworth.hpp"
#include "ComplexMat.hpp"

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

// One level of a Riesz Transform (R) Laplacian Pyramid (Lp).
//
class RieszPyramidLevel {

    RieszPyramidLevel &operator=(const RieszPyramidLevel &);

    cv::Mat itsLp;                     // the frame scaled to this octave
    ComplexMat itsR;                   // the transform
    CompExpMat itsPhase;               // the amplified result
    CompExpMat itsRealPass;            // per-level filter state maintained
    CompExpMat itsImagPass;            // across frames

public:
    // note: this will be called after the first call to build(), which
    // sets itsLp
    void initialize() {
    	const cv::Size size = itsLp.size();
        cos(itsPhase)    = cv::Mat::zeros(size, CV_32F);
        sin(itsPhase)    = cv::Mat::zeros(size, CV_32F);
        cos(itsRealPass) = cv::Mat::zeros(size, CV_32F);
        sin(itsRealPass) = cv::Mat::zeros(size, CV_32F);
        cos(itsImagPass) = cv::Mat::zeros(size, CV_32F);
        sin(itsImagPass) = cv::Mat::zeros(size, CV_32F);
    }

    void build(const cv::Mat &octave) {
        static const cv::Mat realK = (cv::Mat_<float>(1, 3) << -0.6, 0, 0.6);
        static const cv::Mat imagK = realK.t();
        itsLp = octave;
        cv::filter2D(itsLp, real(itsR), itsLp.depth(), realK);
        cv::filter2D(itsLp, imag(itsR), itsLp.depth(), imagK);
    }

    void assign(const RieszPyramidLevel& current) {
        /**/   current.itsLp      .copyTo(        itsLp   );
        real ( current.itsR     ) .copyTo( real ( itsR     ));
        imag ( current.itsR     ) .copyTo( imag ( itsR     ));
        cos  ( current.itsPhase ) .copyTo( cos  ( itsPhase ));
        sin  ( current.itsPhase ) .copyTo( sin  ( itsPhase ));
    }

    void filter(const RieszTemporalFilter& hiCut, const RieszTemporalFilter& loCut, RieszPyramidLevel& prior) {
        hiCut.pass(itsRealPass, itsPhase, prior.itsPhase);
        loCut.pass(itsImagPass, itsPhase, prior.itsPhase);
    }

    const cv::Mat& get_result() const {
        return itsLp;
    }

private:
    static float safe_divide(float dividend, float divisor) __attribute__((always_inline)) {
        if (divisor == 0.0 || divisor == -0.0)
            return 1;
        return dividend/divisor;
    }

public:
    void unwrapOrientPhase(const RieszPyramidLevel &prior) {
#if 0
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
#else
        assert(itsLp.isContinuous() && real(itsR).isContinuous() && imag(itsR).isContinuous()
               && cos(itsPhase).isContinuous() && sin(itsPhase).isContinuous());

        const float * __restrict const lpData = itsLp.ptr<float>(0);
        const float * __restrict const priorLpData = prior.itsLp.ptr<float>(0);
        const float * __restrict const realRData = real(itsR).ptr<float>(0);
        const float * __restrict const imagRData = imag(itsR).ptr<float>(0);
        const float * __restrict const priorRealRData = real(prior.itsR).ptr<float>(0);
        const float * __restrict const priorImagRData = imag(prior.itsR).ptr<float>(0);
        float * __restrict const cosPhaseData = cos(itsPhase).ptr<float>(0);
        float * __restrict const sinPhaseData = sin(itsPhase).ptr<float>(0);

        const int N = itsLp.rows * itsLp.cols;
        for (int i = 0; i < N; i++) {
            float lp = lpData[i];
            float priorLp = priorLpData[i];
            float realR = realRData[i];
            float imagR = imagRData[i];
            float priorRealR = priorRealRData[i];
            float priorImagR = priorImagRData[i];

            float temp1 = lp * priorLp + realR * priorRealR + imagR * priorImagR;
            float temp2 = realR * priorLp - priorRealR * lp;
            float temp3 = imagR * priorLp - priorImagR * lp;
            float tempP = temp2 * temp2 + temp3 * temp3;
            float phi = tempP + temp1 * temp1;

            phi = sqrt(phi);
            temp1 = safe_divide(temp1, phi);
            phi = acos(temp1);
            tempP = sqrtf(tempP);
            temp2 = safe_divide(temp2, tempP);
            temp3 = safe_divide(temp3, tempP);

            cosPhaseData[i] = temp2 * phi;
            sinPhaseData[i] = temp3 * phi;
        }
#endif
    }

    // Multipy the phase difference in this level by alpha but only up to
    // some ceiling threshold.
    //
    void amplify(double alpha, double threshold) {
        static const double sigma = 3.0;
        static const int aperture = 1 + 4 * sigma;
        static const cv::Mat kernel
            = cv::getGaussianKernel(aperture, sigma, CV_32F);
#if 0
        CompExpMat temp;

        // normalize step
        cv::Mat amplitude = square(itsR) + itsLp.mul(itsLp);
        cv::sqrt(amplitude, amplitude);

        const CompExpMat change = itsRealPass - itsImagPass;
        cos(temp) = cos(change).mul(amplitude);
        sin(temp) = sin(change).mul(amplitude);
        cv::sepFilter2D(cos(temp), cos(temp), -1, kernel, kernel);
        cv::sepFilter2D(sin(temp), sin(temp), -1, kernel, kernel);
        cv::Mat temp2;
        cv::sepFilter2D(amplitude, temp2, -1, kernel, kernel);
        cv::divide(cos(temp), temp2, cos(temp));
        cv::divide(sin(temp), temp2, sin(temp));
        // end normalize step

        cv::Mat MagV = square(temp);
        cv::sqrt(MagV, MagV);
        cv::Mat MagV2 = MagV * alpha;
        cv::threshold(MagV2, MagV2, threshold, 0, cv::THRESH_TRUNC);
        CompExpMat phaseDiff; cosSinX(MagV2, phaseDiff);
        cv::Mat pair = real(itsR).mul(cos(temp)) + imag(itsR).mul(sin(temp));
        cv::divide(pair, MagV, pair);
        itsLp = itsLp.mul(cos(phaseDiff)) - pair.mul(sin(phaseDiff));
#else
        const int N = itsLp.rows * itsLp.cols;
        float * __restrict const lpData = itsLp.ptr<float>(0);
        const float * __restrict const realRData = real(itsR).ptr<float>(0);
        const float * __restrict const imagRData = imag(itsR).ptr<float>(0);
        const float * __restrict const cosRealPassData = cos(itsRealPass).ptr<float>(0);
        const float * __restrict const sinRealPassData = sin(itsRealPass).ptr<float>(0);
        const float * __restrict const cosImagPassData = cos(itsImagPass).ptr<float>(0);
        const float * __restrict const sinImagPassData = sin(itsImagPass).ptr<float>(0);

        // Note: we store the first part of the algorithm into a cv::Mat
        // to be able to run cv::sepFilter2D (which uses DFT if the kernel is
        // big enough)
        // Note 2: normalized is a complex matrix, hence 32FC2
        // for our purpose it does not matter that it holds complex numbers,
        // because we work on real and imaginary parts separately
        cv::Mat normalized(itsLp.rows, itsLp.cols, CV_32FC2);
        cv::Mat amplitude(itsLp.rows, itsLp.cols, CV_32F);

        // note: data is manipulated by opencv, so no restrict here
        float * const normalizedData = normalized.ptr<float>(0);
        float * const amplitudeData = amplitude.ptr<float>(0);

        for (int i = 0; i < N; i++) {
            float lp = lpData[i];
            float realR = realRData[i];
            float imagR = imagRData[i];

            float ampl = sqrtf(realR * realR + imagR * imagR + lp * lp);
            amplitudeData[i] = ampl;

            float cosChange = cosRealPassData[i] - cosImagPassData[i];
            float sinChange = sinRealPassData[i] - sinImagPassData[i];

            float cosNormalized = cosChange * ampl;
            float sinNormalized = sinChange * ampl;

            normalizedData[2 * i] = cosNormalized;
            normalizedData[2 * i + 1] = sinNormalized;
        }
        cv::sepFilter2D(normalized, normalized, -1, kernel, kernel);
        cv::sepFilter2D(amplitude, amplitude, -1, kernel, kernel);

        for (int i = 0; i < N; i++) {
            float ampl = amplitudeData[i];
            float cosNormalized = safe_divide(normalizedData[2 * i], ampl);
            float sinNormalized = safe_divide(normalizedData[2 * i + 1], ampl);

            float magV = cosNormalized * cosNormalized + sinNormalized * sinNormalized;
            magV = sqrtf(magV);

            float magV2 = magV * alpha;
            if (magV2 > threshold)
                magV2 = threshold;

            float cosPhaseDiff = cosf(magV2);
            float sinPhaseDiff = sinf(magV2);

            float realR = realRData[i];
            float imagR = imagRData[i];

            float pair = realR * cosNormalized + imagR * sinNormalized;
            pair = safe_divide(pair, magV);

            lpData[i] = lpData[i] * cosPhaseDiff - pair * sinPhaseDiff;
        }
#endif
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
        cv::Mat result = itsLevel[count].get_result();
        for (int i = count - 1; i >= 0; --i) {
            const cv::Mat &octave = itsLevel[i].get_result();
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

    RieszPyramid()
    {}

    bool initialized() const {
        return itsLevel.size() > 0;
    }

    explicit operator bool() const {
        return initialized();
    }

    // Initialize levels here because cannot do that through vector<>.
    //
    void initialize(const cv::Mat &frame)
    {
        itsLevel.resize(countLevels(frame.size()));
        build(frame);
        const size_type count = itsLevel.size();
        for (size_type i = 0; i < count; ++i) {
            RieszPyramidLevel &rpl = itsLevel[i];
            rpl.initialize();
        }
    }
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
        prior.assign(current);
    }

    void filterLevel(RieszPyramidLevel &current, RieszPyramidLevel &prior)
    {
        current.filter(itsHiCut, itsLoCut, prior);
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


struct RieszTransformState {
    RieszTemporalBandpass itsBand;
    RieszPyramid itsCurrent;
    RieszPyramid itsPrior;

    RieszTransformState() {}
    RieszTransformState(const RieszTransformState& other) : itsBand(other.itsBand) {
    }
};

void RieszTransform::fps(double value) {
    state->itsBand.itsFps = value;
    state->itsBand.computeFilter();
}
void RieszTransform::lowCutoff(double frequency) {
    state->itsBand.lowCutoff(frequency);
}
void RieszTransform::highCutoff(double frequency) {
    state->itsBand.highCutoff(frequency);
}

RieszTransform::RieszTransform() : state(new RieszTransformState()), itsAlpha(0.0), itsThreshold(0.0) {}

RieszTransform::RieszTransform(const RieszTransform& other) : state(new RieszTransformState(*other.state)), itsAlpha(other.itsAlpha), itsThreshold(other.itsThreshold) {
    fps(other.state->itsBand.itsFps);
}

RieszTransform::~RieszTransform() {}

void RieszTransform::initialize(const cv::Mat& frame) {
    static const double scaleFactor = 1.0 / 255.0;
    frame.convertTo(itsFrame, CV_32F, scaleFactor);
    state->itsCurrent.initialize(itsFrame);
    state->itsPrior.initialize(itsFrame);
}

cv::Mat RieszTransform::transform(const cv::Mat &frame) {
    static const double PI_PERCENT = M_PI / 100.0;
    static const double scaleFactor = 1.0 / 255.0;

    frame.convertTo(itsFrame, CV_32F, scaleFactor);
    cv::Mat result;

    if (state->itsCurrent) {
        state->itsCurrent.build(itsFrame);
        state->itsCurrent.unwrapOrientPhase(state->itsPrior);
        state->itsBand.filterPyramids(state->itsCurrent, state->itsPrior);
        state->itsCurrent.amplify(itsAlpha, itsThreshold * PI_PERCENT);
        itsFrame = state->itsCurrent.collapse();
        itsFrame.convertTo(result, CV_8UC1, 255);
    } else {
        state->itsCurrent.initialize(itsFrame);
        state->itsPrior.initialize(itsFrame);
        frame.copyTo(result);
    }

    return result;
}
