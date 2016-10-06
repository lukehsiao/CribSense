#ifndef BUTTERWORTH_H_INCLUDED
#define BUTTERWORTH_H_INCLUDED

#include <vector>


// Write into out_a and out_b the digital transfer function for the
// Butterworth filter of order N at the analog cutoff frequency Wn
// scaled by the sampling frequency.
//
extern void butterworth(unsigned int N, double Wn,
                        std::vector<double> &out_a,
                        std::vector<double> &out_b);

#endif // #ifndef BUTTERWORTH_H_INCLUDED
