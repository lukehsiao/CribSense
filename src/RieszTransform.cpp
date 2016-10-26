#include <cstdlib>
#include <functional>
#include <algorithm>
#include <memory>

#include "RieszTransform.hpp"

#include "Butterworth.hpp"

// A low-pass or high-pass filter.
//
class RieszTemporalFilter {

    RieszTemporalFilter &operator=(const RieszTemporalFilter &);
    RieszTemporalFilter(const RieszTemporalFilter &);

public:
    double itsFrequency;
    double itsA[2];
    double itsB[2];

    // Compute this filter's Butterworth coefficients for the sampling
    // frequency, fps (frames per second).
    //
    void computeCoefficients(double halfFps)
    {
        const double Wn = itsFrequency / halfFps;

        std::vector<double> A;
        std::vector<double> B;
        butterworth(1, Wn, A, B);

        assert(A.size() == 2 && B.size() == 2);
        itsA[0] = A[0];
        itsA[1] = A[1];
        itsB[0] = B[0];
        itsB[1] = B[1];
    }

    void pass(cv::Mat &result,
                  const cv::Mat &phase,
                  const cv::Mat &prior) const {
        result
            = itsB[0] * phase
            + itsB[1] * prior
            - itsA[1] * result;
        result /= itsA[0];
    }

    RieszTemporalFilter(double f): itsFrequency(f), itsA(), itsB() {}
};

// One level of a Riesz Transform (R) Laplacian Pyramid (Lp).
//
class RieszPyramidLevel {

    RieszPyramidLevel &operator=(const RieszPyramidLevel &);

    cv::Mat itsLp;                     // the frame scaled to this octave
    cv::Mat itsR;                      // the transform
    cv::Mat itsPhase;                  // the amplified result
    cv::Mat itsRealPass;               // per-level filter state maintained
    cv::Mat itsImagPass;               // across frames

public:
    // note: this will be called after the first call to build(), which
    // sets itsLp
    void initialize() {
        const cv::Size size = itsLp.size();
        itsPhase = cv::Mat::zeros(size, CV_32FC2);
        itsRealPass = cv::Mat::zeros(size, CV_32FC2);
        itsImagPass = cv::Mat::zeros(size, CV_32FC2);
    }

private:
    static float sample(const float * __restrict data, int R, int C, int i, int j) {
        if (i < 0 || i > R)
            i = cv::borderInterpolate(i, R, cv::BORDER_DEFAULT);
        if (j < 0 || j > C)
            j = cv::borderInterpolate(j, C, cv::BORDER_DEFAULT);
        return data[i * C + j];
    }

public:
    void build(const cv::Mat &octave) {
        static const float K[3] = { -0.6, 0, 0.6 };
        itsLp = octave;

        const float * __restrict const lpData = itsLp.ptr<float>(0);

#if 0
        itsR.create(itsLp.size(), CV_32FC2);
        float * __restrict const rData = itsR.ptr<float>(0);

        const int R = itsLp.rows;
        const int C = itsLp.cols;
        for (int i = 0; i < R; i++) {
            for (int j = 0; j < C; j++) {
                // real part, K should be a row kernel
                float acc = 0;
                for (int k = -1; k < 2; k++) {
                    int i1 = i;
                    int j1 = j+k;
                    acc += K[k+1] * sample(lpData, R, C, i1, j1);
                }
                rData[R * i * 2 + j * 2] = acc;

                // imaginary part, K should be a column kernel
                acc = 0;
                for (int k = -1; k < 2; k++) {
                    int i1 = i+k;
                    int j1 = j;
                    acc += K[k+1] * sample(lpData, R, C, i1, j1);
                }
                rData[R * i * 2 + j * 2 + 1] = acc;
            }
        }
#else
        static const cv::Mat realK = (cv::Mat_<float>(1, 3) << -0.6, 0, 0.6);
        static const cv::Mat imagK = realK.t();

        cv::Mat split[2];

        cv::filter2D(itsLp, split[0], itsLp.depth(), realK);
        cv::filter2D(itsLp, split[1], itsLp.depth(), imagK);
        cv::merge(split, 2, itsR);
#endif
    }

    void assign(const RieszPyramidLevel& current) {
        current.itsLp.copyTo(itsLp);
        current.itsR.copyTo(itsR);
        current.itsPhase.copyTo(itsPhase);
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
            return 0;
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
        assert(itsLp.isContinuous() && itsR.isContinuous()
               && itsPhase.isContinuous());

        const float * __restrict const lpData = itsLp.ptr<float>(0);
        const float * __restrict const priorLpData = prior.itsLp.ptr<float>(0);
        const float * __restrict const rData = itsR.ptr<float>(0);
        const float * __restrict const priorRData = prior.itsR.ptr<float>(0);
        float * __restrict const phaseData = itsPhase.ptr<float>(0);

        const int N = itsLp.rows * itsLp.cols;
        for (int i = 0; i < N; i++) {
            float lp = lpData[i];
            float priorLp = priorLpData[i];
            float realR = rData[2 * i];
            float imagR = rData[2 * i + 1];
            float priorRealR = priorRData[2 * i];
            float priorImagR = priorRData[2 * i + 1];

            float temp1 = lp * priorLp + realR * priorRealR + imagR * priorImagR;
            float temp2 = realR * priorLp - priorRealR * lp;
            float temp3 = imagR * priorLp - priorImagR * lp;
            float tempP = temp2 * temp2 + temp3 * temp3;
            float phi = tempP + temp1 * temp1;

            phi = sqrtf(phi);
            temp1 = safe_divide(temp1, phi);
            phi = acosf(temp1);
            tempP = sqrtf(tempP);
            temp2 = safe_divide(temp2, tempP);
            temp3 = safe_divide(temp3, tempP);

            phaseData[2 * i] = temp2 * phi;
            phaseData[2 * i + 1] = temp3 * phi;
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
        const float * __restrict const rData = itsR.ptr<float>(0);
        const float * __restrict const realPassData = itsRealPass.ptr<float>(0);
        const float * __restrict const imagPassData = itsImagPass.ptr<float>(0);

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
            float realR = rData[2 * i];
            float imagR = rData[2 * i + 1];

            float amplitude = sqrtf(realR * realR + imagR * imagR + lp * lp);
            amplitudeData[i] = amplitude;

            float cosChange = realPassData[2 * i] - imagPassData[2 * i];
            float sinChange = realPassData[2 * i + 1] - imagPassData[2 * i + 1];

            float cosNormalized = cosChange * amplitude;
            float sinNormalized = sinChange * amplitude;

            normalizedData[2 * i] = cosNormalized;
            normalizedData[2 * i + 1] = sinNormalized;
        }
        cv::sepFilter2D(normalized, normalized, -1, kernel, kernel);
        cv::sepFilter2D(amplitude, amplitude, -1, kernel, kernel);

        for (int i = 0; i < N; i++) {
            float amplitude = amplitudeData[i];
            float cosNormalized = safe_divide(normalizedData[2 * i], amplitude);
            float sinNormalized = safe_divide(normalizedData[2 * i + 1], amplitude);

            float magV = cosNormalized * cosNormalized + sinNormalized * sinNormalized;
            magV = sqrtf(magV);

            float magV2 = magV * alpha;
            if (magV2 > threshold)
                magV2 = threshold;

            float cosPhaseDiff = cosf(magV2);
            float sinPhaseDiff = sinf(magV2);

            float realR = rData[2 * i];
            float imagR = rData[2 * i + 1];

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

void RieszTransform::fps(double fps) {
    state->itsBand.itsFps = fps;
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

cv::Mat RieszTransform::transform(const cv::Mat& frame) {
    static const double PI_PERCENT = M_PI / 100.0;
    static const double scaleFactor = 1.0 / 255.0;

    cv::Mat itsFrame;
    frame.convertTo(itsFrame, CV_32F, scaleFactor);
    cv::cvtColor(itsFrame, itsFrame, cv::COLOR_RGB2YCrCb);
    std::vector<cv::Mat> channels; cv::split(itsFrame, channels);
    cv::Mat result;

    if (state->itsCurrent) {
        state->itsCurrent.build(channels[0]);
        state->itsCurrent.unwrapOrientPhase(state->itsPrior);
        state->itsBand.filterPyramids(state->itsCurrent, state->itsPrior);
        state->itsCurrent.amplify(itsAlpha, itsThreshold * PI_PERCENT);
        channels[0] = state->itsCurrent.collapse();
        cv::merge(channels, result);
        cv::cvtColor(result, result, cv::COLOR_YCrCb2RGB);
        result.convertTo(result, CV_8UC3, 255);
    } else {
        state->itsCurrent.initialize(channels[0]);
        state->itsPrior.initialize(channels[0]);
        frame.copyTo(result);
    }

    return std::move(result);
}
