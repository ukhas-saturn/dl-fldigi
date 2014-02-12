// ----------------------------------------------------------------------------
//	fftfilt.cxx  --  Fast convolution Overlap-Add filter
//
// Filter implemented using overlap-add FFT convolution method
// h(t) characterized by Windowed-Sinc impulse response
//
// Reference:
//	 "The Scientist and Engineer's Guide to Digital Signal Processing"
//	 by Dr. Steven W. Smith, http://www.dspguide.com
//	 Chapters 16, 18 and 21
//
// Copyright (C) 2006-2008 Dave Freese, W1HKJ
//
// This file is part of fldigi.
//
// Fldigi is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fldigi.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include <config.h>

#include <memory.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <typeinfo>

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory.h>
#include "configuration.h"

#include "misc.h"
#include "fftfilt.h"

//------------------------------------------------------------------------------
// initialize the filter
// create forward and reverse FFTs
//------------------------------------------------------------------------------

void fftfilt::init_filter(int jobs)
{
	flen2 = flen >> 1;
	fft		= new p_fft(flen, jobs);

	filter		= new cmplx[flen*jobs];
	timedata	= new cmplx[flen*jobs];
	freqdata	= new cmplx[flen*jobs];
	output		= new cmplx[flen*jobs];
	ovlbuf		= new cmplx[flen2*jobs];
	ht		= new cmplx[flen*jobs];

	memset(filter, 0, jobs * flen * sizeof(cmplx));
	memset(timedata, 0, jobs * flen * sizeof(cmplx));
	memset(freqdata, 0, jobs * flen * sizeof(cmplx));
	memset(output, 0, jobs * flen * sizeof(cmplx));
	memset(ovlbuf, 0, jobs * flen2 * sizeof(cmplx));
	memset(ht, 0, jobs * flen * sizeof(cmplx));

	inptr = 0;
}

//------------------------------------------------------------------------------
// fft filter
// f1 < f2 ==> band pass filter
// f1 > f2 ==> band reject filter
// f1 == 0 ==> low pass filter
// f2 == 0 ==> high pass filter
//------------------------------------------------------------------------------
fftfilt::fftfilt(float f1, float f2, int len, int jobs)
{
	flen	= len;
	init_filter( (2==jobs)? 2:1 );
	create_filter(f1, f2);
}

//------------------------------------------------------------------------------
// low pass filter
//------------------------------------------------------------------------------
fftfilt::fftfilt(float f, int len)
{
	flen	= len;
	init_filter( 1 );
	create_lpf(f);
}

fftfilt::~fftfilt()
{
	if (fft) delete fft;

	if (filter) delete [] filter;
	if (timedata) delete [] timedata;
	if (freqdata) delete [] freqdata;
	if (output) delete [] output;
	if (ovlbuf) delete [] ovlbuf;
	if (ht) delete [] ht;
}

void fftfilt::create_filter(float f1, float f2)
{
// initialize the filter to zero
	memset(ht, 0, flen * sizeof(cmplx));

// create the filter shape coefficients by fft
// filter values initialized to the ht response h(t)
	bool b_lowpass, b_highpass;//, window;
	b_lowpass = (f2 != 0);
	b_highpass = (f1 != 0);

	for (int i = 0; i < flen2; i++) {
		ht[i] = 0;
//combine lowpass / highpass
// lowpass @ f2
		if (b_lowpass) ht[i] += fsinc(f2, i, flen2);
// highighpass @ f1
		if (b_highpass) ht[i] -= fsinc(f1, i, flen2);
	}
// highpass is delta[flen2/2] - h(t)
	if (b_highpass && f2 < f1) ht[flen2 / 2] += 1;

	for (int i = 0; i < flen2; i++)
		ht[i] *= _blackman(i, flen2);

// ht is flen complex points with imaginary all zero
// first half describes h(t), second half all zeros
// perform the cmplx forward fft to obtain H(w)
// filter is flen/2 complex values

	fft->ComplexFFT(ht, filter);

// normalize the output filter for unity gain
	float scale = 0, mag;
	for (int i = 0; i < flen2; i++) {
		mag = abs(filter[i]);
		if (mag > scale) scale = mag;
	}
	if (scale != 0) {
		for (int i = 0; i < flen; i++)
			filter[i] /= scale;
	}

}

/*
 * Filter with fast convolution (overlap-add algorithm).
 *
 * Dual version halves Pi GPU latency, almost doubles speed
 * - draft code only. With hardware acceleration RTTY is <1% cpu for FFT
 *  so further improvements are less signficant.
 */

int fftfilt::rundual(const cmplx &in1, const cmplx &in2, cmplx **out1, cmplx **out2)
{
	timedata[inptr]  = in1;
	timedata[inptr++] = in2;
	if (inptr < flen2) return 0;
	fft->ComplexFFT(timedata, freqdata);
	for (int i = 0; i < flen; i++) {
		freqdata[i] *= filter[i];
		freqdata[flen+i] *= filter[i];
	}
	fft->InverseComplexFFT(freqdata, output);
	for (int i = 0; i < flen2; i++) {
		output[i] += ovlbuf[i];
		ovlbuf[i] = output[i+flen2];
		output[flen+i] += ovlbuf[flen2+i];
		ovlbuf[flen2+i] = output[flen+i+flen2];
	}

	*out1 = output;
	*out2 = &output[flen];

	inptr = 0;
	return flen2;
}

int fftfilt::run(const cmplx & in, cmplx **out)
{
// collect flen/2 input samples
	timedata[inptr++] = in;

	if (inptr < flen2)
		return 0;

// FFT transpose to the frequency domain
//	memcpy(freqdata, timedata, flen * sizeof(cmplx));
	fft->ComplexFFT(timedata, freqdata);

// multiply with the filter shape
	for (int i = 0; i < flen; i++)
		freqdata[i] *= filter[i];

// transform back to time domain
	fft->InverseComplexFFT(freqdata, output);

// overlap and add
// save the second half for overlapping next inverse FFT
	for (int i = 0; i < flen2; i++) {
		output[i] += ovlbuf[i];
		ovlbuf[i] = output[i+flen2];
	}

// clear inbuf pointer
	inptr = 0;

// signal the caller there is flen/2 samples ready
	*out = output;
	return flen2;
}

//------------------------------------------------------------------------------
// rtty filter
//------------------------------------------------------------------------------

void fftfilt::rtty_filter(float f){ create_filter(0,f); }

