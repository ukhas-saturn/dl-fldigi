#ifndef	_COEFF_H
#define	_COEFF_H

#define	FIRLEN	64

extern float gmfir1c[FIRLEN];
extern float gmfir2c[FIRLEN];
extern float syncfilt[16];

extern void raisedcosfilt(float *);
extern void wsincfilt(float *, float fc, bool blackman);

#endif
