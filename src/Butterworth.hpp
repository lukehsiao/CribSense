/**
 * This file is released is under the Patented Algorithm with Software released
 * Agreement. See LICENSE.md for more information, and view the original repo
 * at https://github.com/tbl3rd/Pyramids
 */

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
