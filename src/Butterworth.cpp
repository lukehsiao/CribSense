#include "Butterworth.hpp"

#include <algorithm>
#include <complex>


// An ordering on complex that lets real roots dominate.
//
static bool sortComplex(std::complex<double> x, std::complex<double> y)
{
    if (std::real(x) < std::real(y)) return true;
    if (std::real(y) < std::real(x)) return false;
    return std::imag(x) < std::imag(y);
}
static bool hasPosImag(const std::complex<double> &z)
{
    return std::imag(z) > 0;
}
static bool hasNegImag(const std::complex<double> &z)
{
    return std::imag(z) < 0;
}

// Return the polynomial coefficients for roots.
// Keep roots sorted with sortComplex() to maintain precision.
//
static std::vector< std::complex<double> >
polynomialCoefficients(std::vector< std::complex<double> > roots)
{
    std::vector< std::complex<double> > coeffs(roots.size() + 1, 0.0);
    coeffs[0] = 1.0;
    std::sort(roots.begin(), roots.end(), sortComplex);
    unsigned int sofar = 1;
    for (unsigned int k = 0; k < roots.size(); ++k) {
        const std::complex<double> w = -roots[k];
        for (unsigned int j = sofar; j > 0; --j) {
            coeffs[j] = coeffs[j] * w + coeffs[j - 1];
        }
        coeffs[0] *= w;
        ++sofar;
    }
    std::vector< std::complex<double> > result = coeffs;

    std::vector< std::complex<double> > pos_roots = roots;
    std::vector< std::complex<double> >::iterator pos_end;
    pos_end = std::remove_if(pos_roots.begin(), pos_roots.end(), hasNegImag);
    pos_roots.assign(pos_roots.begin(), pos_end);
    std::sort(pos_roots.begin(), pos_roots.end(), sortComplex);

    std::vector< std::complex<double> > neg_roots = roots;
    std::vector< std::complex<double> >::iterator neg_end;
    neg_end = std::remove_if(neg_roots.begin(), neg_roots.end(), hasPosImag);
    neg_roots.assign(neg_roots.begin(), neg_end);
    std::sort(neg_roots.begin(), neg_roots.end(), sortComplex);

    const bool same
        =  neg_roots.size() == pos_roots.size()
        && std::equal(pos_roots.begin(), pos_roots.end(), neg_roots.begin());
    if (same) {
        for (unsigned k = 0; k < coeffs.size(); ++k) {
            result[k] = std::real(coeffs[k]);
        }
    }
    return result;
}

// Write into a and b the real polynomial transfer function coefficients
// from gain, zeros, and poles.
//
static void
zerosPolesToTransferCoefficients(std::vector< std::complex<double> > zeros,
                                 std::vector< std::complex<double> > poles,
                                 double gain,
                                 std::vector< std::complex<double> > &a,
                                 std::vector< std::complex<double> > &b)
{
    a = polynomialCoefficients(poles);
    b = polynomialCoefficients(zeros);
    for (unsigned int k = 0; k < b.size(); ++k) b[k] *= gain;
}


// Normalize the polynomial representation of the real transfer
// coefficients in a and b.
//
// Output coefficients will also be real numbers.
// Remove leading zeros to avoid dividing by 0.
//
static void normalize(std::vector< std::complex<double> > &b,
                      std::vector< std::complex<double> > &a)
{
    while (a.front() == 0.0 && a.size() > 1) a.erase(a.begin());
    const std::complex<double> leading_coeff = a.front();
    for (unsigned int k = 0; k < a.size(); ++k) a[k] /= leading_coeff;
    for (unsigned int k = 0; k < b.size(); ++k) b[k] /= leading_coeff;
}


// Return the binomial coefficient: n choose k.
//
static unsigned choose(unsigned n, unsigned k)
{
    if (k > n) return 0;
    if (k * 2 > n) k = n - k;
    if (k == 0) return 1;
    unsigned result = n;
    for (unsigned i = 2; i <= k; ++i) {
        result *= (n - i + 1);
        result /= i;
    }
    return result;
}


// Use the bilinear transform to convert the analog filter coefficients in
// a and b into a digital filter for the sampling frequency fs (1/T).
//
static void bilinearTransform(std::vector< std::complex<double> > &b,
                              std::vector< std::complex<double> > &a,
                              double fs)
{
    const unsigned int D = a.size() - 1;
    const unsigned int N = b.size() - 1;
    const unsigned int M = std::max(N, D);
    const unsigned int Np = M;
    const unsigned int Dp = M;

    std::vector< std::complex<double> > bprime(Np + 1, 0.0);
    for (unsigned j = 0; j < Np + 1; ++j) {
        std::complex<double> val = 0.0;
        for (unsigned i = 0; i < N + 1; ++i) {
            for (unsigned k = 0; k < i + 1; ++k) {
                for (unsigned l = 0; l < M - i + 1; ++l) {
                    if (k + l == j) {
                        val += std::complex<double>(choose(i, k))
                            *  std::complex<double>(choose(M - i, l))
                            *  b[N - i] * pow(2.0 * fs, i) * pow(-1.0, k);
                    }
                }
            }
        }
        bprime[j] = real(val);
    }

    std::vector< std::complex<double> > aprime(Dp + 1, 0.0);
    for (unsigned j = 0; j < Dp + 1; ++j) {
        std::complex<double> val = 0.0;
        for (unsigned i = 0; i < D + 1; ++i) {
            for(unsigned k = 0; k < i + 1; ++k) {
                for(unsigned l = 0; l < M - i + 1; ++l) {
                    if (k + l == j) {
                        val += std::complex<double>(choose(i, k))
                            *  std::complex<double>(choose(M - i, l))
                            *  a[D - i] * pow(2.0 * fs, i) * pow(-1.0, k);
                    }
                }
            }
        }
        aprime[j] = real(val);
    }

    normalize(bprime, aprime);
    a = aprime;
    b = bprime;
}


// Transform a and b coefficients of transfer function
// into a low-pass filter with cutoff frequency w_0.
// Assume the transfer function has only real coefficients.
//
static void toLowpass(std::vector< std::complex<double> > &b,
                      std::vector< std::complex<double> > &a,
                      double w0)
{
    std::vector<double> pwo;
    const int d = a.size();
    const int n = b.size();
    const int M = int(std::max(double(d), double(n)));
    const unsigned int start1 = int(std::max(double(n - d), 0.0));
    const unsigned int start2 = int(std::max(double(d - n), 0.0));
    for (int k = M - 1; k > -1; --k) pwo.push_back(pow(w0, double(k)));
    unsigned int k;
    for (k = start2; k < pwo.size() && k - start2 < b.size(); ++k) {
        b[k - start2]
            *= std::complex<double>(pwo[start1])
            /  std::complex<double>(pwo[k]);
    }

    for (k = start1; k < pwo.size() && k - start1 < a.size(); ++k) {
        a[k - start1]
            *= std::complex<double>(pwo[start1])
            /  std::complex<double>(pwo[k]);
    }
    normalize(b, a);
}


// Compute zeros, poles and gain for filter of order N assuming the
// normalized Butterworth form of transfer function.
//
// The gain is always 1.0, but parameterized to agree with textbooks.
//
static void
prototypeAnalogButterworth(unsigned N,
                           std::vector< std::complex<double> > &zeros,
                           std::vector< std::complex<double> > &poles,
                           double &gain)
{
    static const std::complex<double> j = std::complex<double>(0, 1.0);
    for (unsigned k = 1; k < N + 1; ++k) {
        poles.push_back(exp(j * (2.0 * k - 1) / (2.0 * N) * M_PI) * j);
    }
    gain = 1.0;
    zeros.clear();
}


// Tangentially warp the Wn input analog frequency to W0
// for the resulting digital filter.
//
// W0 is the warped (high or low) bandpass cutoff frequency.
//
// See http://www.robots.ox.ac.uk/~sjrob/Teaching/SP/l6.pdf
//
void butterworth(unsigned int N, double Wn,
                 std::vector<double> &out_a,
                 std::vector<double> &out_b)
{
    static const double fs = 2.0;
    const double w0 = 2.0 * fs * tan(M_PI * Wn / fs);
    std::vector< std::complex<double> > zeros, poles;
    double gain;
    prototypeAnalogButterworth(N, zeros, poles, gain);
    std::vector< std::complex<double> > a, b;
    zerosPolesToTransferCoefficients(zeros, poles, gain, a, b);
    toLowpass(b, a, w0);
    bilinearTransform(b, a, fs);
    out_a.clear();
    for (unsigned k = 0; k < a.size(); ++k) out_a.push_back(std::real(a[k]));
    out_b.clear();
    for (unsigned k = 0; k < b.size(); ++k) out_b.push_back(std::real(b[k]));
}
