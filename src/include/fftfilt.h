/*
 *    fftfilt.h  --  Fast convolution FIR filter
*/

#ifndef	_FFTFILT_H
#define	_FFTFILT_H

#include "complex.h"
#include "pfft.h"

//----------------------------------------------------------------------

class fftfilt {
enum {NONE, BLACKMAN, HAMMING, HANNING};

protected:
	int flen;
	int flen2;
	p_fft *fft;
	p_fft *ift;
	cmplx *ht;
	cmplx *filter;
	cmplx *timedata;
	cmplx *freqdata;
	cmplx *ovlbuf;
	cmplx *output;
	int inptr;
	int pass;
	int window;

	inline float fsinc(float fc, int i, int len) {
		return (i == len/2) ? 2.0 * fc: 
				sin(2 * M_PI * fc * (i - len/2)) / (M_PI * (i - len/2));
	}
	inline float _blackman(int i, int len) {
		return (0.42 - 
				 0.50 * cos(2.0 * M_PI * i / len) + 
				 0.08 * cos(4.0 * M_PI * i / len));
	}
	void init_filter(int jobs);

public:
	fftfilt(float f1, float f2, int len, int jobs=1);
	fftfilt(float f, int len);
	~fftfilt();
// f1 < f2 ==> bandpass
// f1 > f2 ==> band reject
	void create_filter(float f1, float f2);

	void create_lpf(float f) {
		create_filter(0, f);
	}
	void create_hpf(float f) {
		create_filter(f, 0);
	}
	void rtty_filter(float);

	int run(const cmplx& in, cmplx **out);
	int rundual(const cmplx& in1, const cmplx& in2, cmplx **out1, cmplx **out2);
};

#endif
