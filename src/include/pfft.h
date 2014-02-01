
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
	void GpuInit();
	void GpuDeinit();
public:
        p_fft(int M = 1024) {
		FFT_size = 8;
                if (M < 256) M = 256;
                if (M > 65536) M = 65536;
		while (M > 256) {
			M >>= 1;
                	FFT_size++;
		}
		GpuInit();
		//gpu_fft_prepare( FFT_size, GPU_FFT_FWD, 1, &fftf);
		//gpu_fft_prepare( FFT_size, GPU_FFT_REV, 1, &fftr);
	}
        ~p_fft() {
		GpuDeinit();
		//gpu_fft_release(fftf);
		//gpu_fft_release(fftr);
        }

        void ComplexFFT(cmplx *buf);
        void InverseComplexFFT(cmplx *buf);
};

#endif
