// ----------------------------------------------------------------------------
// rtty.h  --  RTTY modem
//
// Copyright (C) 2006
//		Dave Freese, W1HKJ
//
// This file is part of fldigi.  Adapted from code contained in gmfsk source code
// distribution.
//  gmfsk Copyright (C) 2001, 2002, 2003
//  Tomi Manninen (oh2bns@sral.fi)
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

#ifndef VIEW_RTTY_H
#define VIEW_RTTY_H

#include "rtty.h"
#include "complex.h"
#include "modem.h"
#include "globals.h"
#include "filters.h"
#include "fftfilt.h"
#include "digiscope.h"

#define	VIEW_RTTY_SampleRate	8000

#define	VIEW_RTTY_MAXBITS	(2 * VIEW_RTTY_SampleRate / 23 + 1)

#define MAX_CHANNELS 30

enum CHANNEL_STATE {IDLE, SRCHG, RCVNG, WAITING};

struct RTTY_CHANNEL {

	int				state;

	float			phaseacc;

	fftfilt *mark_filt;
	fftfilt *space_filt;

	Cmovavg		*bits;
	bool		nubit;
	bool		bit;

	bool		bit_buf[MAXBITS];

	float mark_phase;
	float space_phase;

	float		metric;

	int			rxmode;
	RTTY_RX_STATE	rxstate;

	float		frequency;
	float		freqerr;
	float		phase;
	float		posfreq;
	float		negfreq;
	float		freqerrhi;
	float		freqerrlo;
	float		poserr;
	float		negerr;
	int			poscnt;
	int			negcnt;
	int			timeout;

	float		mark_mag;
	float		space_mag;
	float		mark_env;
	float		space_env;
	float		noise_floor;
	float		mark_noise;
	float		space_noise;

	float		sigpwr;
	float		noisepwr;
	float		avgsig;

	float		prevsymbol;
	cmplx		prevsmpl;
	int			counter;
	int			bitcntr;
	int			rxdata;
	int			inp_ptr;

	cmplx		mark_history[MAXPIPE];
	cmplx		space_history[MAXPIPE];

	int			sigsearch;
};


class view_rtty : public modem {
public:
	static const float SHIFT[];
	static const float BAUD[];
	static const int    BITS[];

private:

	float shift;
	int symbollen;
	int nbits;
	int stoplen;
	int msb;
	bool useFSK;

	RTTY_CHANNEL	channel[MAX_CHANNELS];

	float		rtty_squelch;
	float		rtty_shift;
	float      rtty_BW;
	float		rtty_baud;
	int 		rtty_bits;
	RTTY_PARITY	rtty_parity;
	int			rtty_stop;
	bool		rtty_msbfirst;

	int bflen;
	float bp_filt_lo;
	float bp_filt_hi;

	int txmode;
	int preamble;

	void clear_syncscope();
	void update_syncscope();
	cmplx mixer(float &phase, float f, cmplx in);

	unsigned char bitreverse(unsigned char in, int n);
	int decode_char(int ch);
	int rttyparity(unsigned int);
	bool rx(int ch, bool bit);

	int rttyxprocess();
	char baudot_dec(int ch, unsigned char data);
	void Metric(int ch);
public:
	view_rtty(trx_mode mode);
	~view_rtty();
	void init();
	void rx_init();
	void tx_init(SoundBase *sc){}
	void restart();
	void reset_filters(int ch);
	int rx_process(const float *buf, int len);
	int tx_process();

	void find_signals();
	void clearch(int ch);
	void clear();
	int get_freq(int n) { return (int)channel[n].frequency;}

	bool is_mark_space(int ch, int &);
	bool is_mark(int ch);

};

extern view_rtty *rttyviewer;

#endif
