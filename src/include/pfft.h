
#ifndef PI_FFT_H
#define PI_FFT_H

#include <complex>

class p_fft
{
private:
	int FFT_size;
	struct GPU_FFT_COMPLEX *base;
	struct GPU_FFT *fftf;
	struct GPU_FFT *fftr;
	void GpuInit(int jobs);
	void GpuDeinit();
public:
        p_fft(int M, int jobs) {
		FFT_size = 8;
                if (M < 256) M = 256;
                if (M > 65536) M = 65536;
		while (M > 256) {
			M >>= 1;
                	FFT_size++;
		}
		GpuInit(jobs);
	}
        ~p_fft() {
		GpuDeinit();
        }

        void ComplexFFT(cmplx *in, cmplx *out);
	void ComplexFFT2(cmplx *in, cmplx *out);
        void InverseComplexFFT(cmplx *in, cmplx *out);
	void InverseComplexFFT2(cmplx *in, cmplx *out);
};

#endif
