// ----------------------------------------------------------------------------
// fsq.h  --  BASIS FOR ALL MODEMS
//
// Copyright (C) 2006
//		Dave Freese, W1HKJ
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

#ifndef _FSQ_H
#define _FSQ_H

#include <string>

#include "trx.h"
#include "modem.h"
#include "complex.h"
#include "filters.h"
#include "crc8.h"

class fsq : public modem {

#define	SR			12000
#define FFTSIZE		4096
#define FSQ_SYMLEN	4096

#define BLOCK_SIZE (FFTSIZE / 2)
#define SHIFT_SIZE	(FSQ_SYMLEN / 16) // at 3 baud

#define FSQ_SHAPE_SIZE	64 // 5 msec rise time

//#define FSQBOL " \n"
//#define FSQEOL "\n "
//#define FSQEOT "  \b  "

friend
	void timed_xmt(void *);

public:
static int			symlen;

protected:
// Rx
	double			rx_stream[BLOCK_SIZE + SHIFT_SIZE];
	cmplx			fft_data[2*FFTSIZE];
	double			a_blackman[BLOCK_SIZE];
	int				a_step;
	int 			bkptr;
	g_fft<double>	*fft;
	Cmovavg			*sqlch;
	Cmovavg			*snfilt;
	int				shift_size; // number of samples to shift in time
	int				block_size; // size of the windowed sample set for FFT
	double			val;
	double			max;
	int				peak;
	double			baud_estimate;
	int				baud_counter;
	int				baud_bin;
	int				prev_peak;
	int				last_peak;
	int				peak_counter;
	int				peak_hits;
	int				symbol;
	int				prev_symbol;
	int				curr_nibble;
	int				prev_nibble;
	void			lf_check(int);
	void			process_symbol(int);
	double			signoise;
	double			s2n;
	char			szestimate[40];
	std::string		rx_text;
	std::string		toprint;
	void			parse_rx_text();
	void			parse_space();
	void			parse_qmark();
	void			parse_star();
	void			parse_excl();
	void			parse_tilda();
	void			parse_pound();
	void			parse_dollar();
	void			parse_at();
	void			parse_amp();
	void			parse_carat();
	void			parse_pcnt();
	void			parse_vline();
	void			parse_greater();
	void			parse_less();
	void			parse_plus();
	void			parse_minus();
	void			parse_semi();

	bool			b_bot;
	bool			b_eol;
	bool			b_eot;

// Tx
	double			*keyshape;
	int				tone;
	int				prevtone;
	double			txphase;
	void			send_string(std::string);
	bool			send_bot;
	void			flush_buffer ();
	void			send_char (int);
	void			send_idle ();
	void			send_symbol(int sym);
	void			send_tone(int tone);
	std::string		xmt_string;
	double			xmtdelay();
	void			reply(std::string);

// RxTx
	double			center_freq;
	int				spacing;
	int				basetone;
	double			speed;
	double			metric;
	bool			ch_sqlch_open;
	CRC8			crc;
	std::string		station_calling;
	std::string		mycall;

	void			show_mode();
	void			adjust_for_speed();
	void			process_tones();

	void			start_thread ();
	void			stop_thread ();
	void			increment_symbol_count();
	void			clear_symbol_count();
	int				get_symbol_count();

	bool			valid_char(int);

	static const char *FSQBOL;
	static const char *FSQEOL;
	static const char *FSQEOT;

public:
	fsq (trx_mode md);
	~fsq ();
	void	init ();
	void	rx_init ();
	void	restart ();
	void	tx_init (SoundBase *sc);
	int		rx_process (const double *buf, int len);

	int		tx_process ();

};

#endif
