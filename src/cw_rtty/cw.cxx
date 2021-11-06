// ----------------------------------------------------------------------------
// cw.cxx  --  morse code modem
//
// Copyright (C) 2006-2010
//		Dave Freese, W1HKJ
//		   (C) Mauri Niininen, AG1LE
//
// This file is part of fldigi.  Adapted from code contained in gmfsk source code
// distribution.
//  gmfsk Copyright (C) 2001, 2002, 2003
//  Tomi Manninen (oh2bns@sral.fi)
//  Copyright (C) 2004
//  Lawrence Glaister (ve7it@shaw.ca)
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

#include <cstring>
#include <string>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include "digiscope.h"
#include "waterfall.h"
#include "fl_digi.h"
#include "fftfilt.h"

#include "cw.h"
#include "misc.h"
#include "configuration.h"
#include "confdialog.h"
#include "status.h"
#include "debug.h"
#include "FTextRXTX.h"
#include "modem.h"

#include "qrunner.h"

#include "winkeyer.h"
#include "nanoIO.h"

using namespace std;

#define XMT_FILT_LEN 256
#define QSK_DELAY_LEN 4*XMT_FILT_LEN
#define CW_FFT_SIZE 2048 // must be a factor of 2

static double nano_d2d = 0;
static int nano_wpm = 0;

const cw::SOM_TABLE cw::som_table[] = {
	/* Prosigns */
	{"-...-",	{1.0,  0.33,  0.33,  0.33, 1.0,   0, 0} },
	{".-.-",	{ 0.33, 1.0,  0.33, 1.0,   0,   0, 0} },
	{".-...",	{ 0.33, 1.0,  0.33,  0.33,  0.33,   0, 0} },
	{".-.-.",	{ 0.33, 1.0,  0.33, 1.0,  0.33,   0, 0} },
	{"...-.-",	{ 0.33,  0.33,  0.33, 1.0,  0.33, 1.0, 0} },
	{"-.--.",	{1.0,  0.33, 1.0, 1.0,  0.33,   0, 0} },
	{"..-.-",	{ 0.33,  0.33, 1.0,  0.33, 1.0,   0, 0} },
	{"....--",	{ 0.33,  0.33,  0.33,  0.33, 1.0, 1.0, 0} },
	{"...-.",	{ 0.33,  0.33,  0.33, 1.0,  0.33,   0, 0} },
	/* ASCII 7bit letters */
	{".-",		{ 0.33, 1.0,   0,   0,   0,   0, 0}	},
	{"-...",	{1.0,  0.33,  0.33,  0.33,   0,   0, 0}	},
	{"-.-.",	{1.0,  0.33, 1.0,  0.33,   0,   0, 0}	},
	{"-..",		{1.0,  0.33,  0.33,   0,   0,   0, 0} 	},
	{".",		{ 0.33,   0,   0,   0,   0,   0, 0}	},
	{"..-.",	{ 0.33,  0.33, 1.0,  0.33,   0,   0, 0}	},
	{"--.",		{1.0, 1.0,  0.33,   0,   0,   0, 0}	},
	{"....",	{ 0.33,  0.33,  0.33,  0.33,   0,   0, 0}	},
	{"..",		{ 0.33,  0.33,   0,   0,   0,   0, 0}	},
	{".---",	{ 0.33, 1.0, 1.0, 1.0,   0,   0, 0}	},
	{"-.-",		{1.0,  0.33, 1.0,   0,   0,   0, 0}	},
	{".-..",	{ 0.33, 1.0,  0.33,  0.33,   0,   0, 0}	},
	{"--",		{1.0, 1.0,   0,   0,   0,   0, 0}	},
	{"-.",		{1.0,  0.33,   0,   0,   0,   0, 0}	},
	{"---",		{1.0, 1.0, 1.0,   0,   0,   0, 0}	},
	{".--.",	{ 0.33, 1.0, 1.0,  0.33,   0,   0, 0}	},
	{"--.-",	{1.0, 1.0,  0.33, 1.0,   0,   0, 0}	},
	{".-.",		{ 0.33, 1.0,  0.33,   0,   0,   0, 0}	},
	{"...",		{ 0.33,  0.33,  0.33,   0,   0,   0, 0}	},
	{"-",		{1.0,   0,   0,   0,   0,   0, 0}	},
	{"..-",		{ 0.33,  0.33, 1.0,   0,   0,   0, 0}	},
	{"...-",	{ 0.33,  0.33,  0.33, 1.0,   0,   0, 0}	},
	{".--",		{ 0.33, 1.0, 1.0,   0,   0,   0, 0}	},
	{"-..-",	{1.0,  0.33,  0.33, 1.0,   0,   0, 0}	},
	{"-.--",	{1.0,  0.33, 1.0, 1.0,   0,   0, 0}	},
	{"--..",	{1.0, 1.0,  0.33,  0.33,   0,   0, 0}	},
	/* Numerals */
	{"-----",	{1.0, 1.0, 1.0, 1.0, 1.0,   0, 0}	},
	{".----",	{ 0.33, 1.0, 1.0, 1.0, 1.0,   0, 0}	},
	{"..---",	{ 0.33,  0.33, 1.0, 1.0, 1.0,   0, 0}	},
	{"...--",	{ 0.33,  0.33,  0.33, 1.0, 1.0,   0, 0}	},
	{"....-",	{ 0.33,  0.33,  0.33,  0.33, 1.0,   0, 0}	},
	{".....",	{ 0.33,  0.33,  0.33,  0.33,  0.33,   0, 0}	},
	{"-....",	{1.0,  0.33,  0.33,  0.33,  0.33,   0, 0}	},
	{"--...",	{1.0, 1.0,  0.33,  0.33,  0.33,   0, 0}	},
	{"---..",	{1.0, 1.0, 1.0,  0.33,  0.33,   0, 0}	},
	{"----.",	{1.0, 1.0, 1.0, 1.0,  0.33,   0, 0}	},
	/* Punctuation */
	{".-..-.",	{ 0.33, 1.0,  0.33,  0.33, 1.0,  0.33, 0}	},
	{".----.",	{ 0.33, 1.0, 1.0, 1.0, 1.0,  0.33, 0}	},
	{"...-..-",	{ 0.33,  0.33,  0.33, 1.0,  0.33,  0.33, 1.0}	},
	{"-.---.",	{1.0,  0.33, 1.0, 1.0,  0.33,   0, 0}	},
	{"-.--.-",	{1.0,  0.33, 1.0, 1.0,  0.33, 1.0, 0}	},
	{"--..--",	{1.0, 1.0,  0.33,  0.33, 1.0, 1.0, 0}	},
	{"-....-",	{1.0,  0.33,  0.33,  0.33,  0.33, 1.0, 0}	},
	{".-.-.-",	{ 0.33, 1.0,  0.33, 1.0,  0.33, 1.0, 0}	},
	{"-..-.",	{1.0,  0.33,  0.33, 1.0,  0.33,   0, 0}	},
	{"---...",	{1.0, 1.0, 1.0,  0.33,  0.33,  0.33, 0}	},
	{"-.-.-.",	{1.0,  0.33, 1.0,  0.33, 1.0,  0.33, 0}	},
	{"..--..",	{ 0.33,  0.33, 1.0, 1.0,  0.33,  0.33, 0}	},
	{"..--.-",	{ 0.33,  0.33, 1.0, 1.0,  0.33, 1.0, 0}	},
	{".--.-.",	{ 0.33, 1.0, 1.0,  0.33, 1.0,  0.33, 0}	},
	{"-.-.--",	{1.0,  0.33, 1.0,  0.33, 1.0, 1.0, 0}	},

	{".-.-",	{0.33, 1.0, 0.33, 1.0, 0, 0 , 0}  },	// A umlaut, A aelig
	{".--.-",	{0.33, 1.0, 1.0, 0.33, 1.0, 0, 0 }  },	// A ring
	{"-.-..",	{1.0, 0.33, 1.0, 0.33, 0.33, 0, 0} },	// C cedilla
	{".-..-",	{0.33, 1.0, 0.33, 0.33, 1.0, 0, 0} },	// E grave
	{"..-..",	{0.33, 0.33, 1.0, 0.33, 0.33, 0, 0} },	// E acute
	{"---.",	{1.0, 1.0, 1.0, 0.33, 0, 0, 0} },		// O acute, O umlat, O slash
	{"--.--",	{1.0, 1.0, 0.33, 1.0, 1.0, 0, 0} },		// N tilde
	{"..--",	{0.33, 0.33, 1.0, 1.0, 0, 0, 0} },		// U umlaut, U circ

	{"", {0.0}}
};

int cw::normalize(float *v, int n, int twodots)
{
	if( n == 0 ) return 0 ;

	float max = v[0];
	float min = v[0];
	int j;

	/* find max and min values */
	for (j=1; j<n; j++) {
		float vj = v[j];
		if (vj > max)	max = vj;
		else if (vj < min)	min = vj;
	}
	/* all values 0 - no need to normalize or decode */
	if (max == 0.0) return 0;

	/* scale values between  [0,1] -- if Max longer than 2 dots it was "dah" and should be 1.0, otherwise it was "dit" and should be 0.33 */
	float ratio = (max > twodots) ? 1.0 : 0.33 ;
	ratio /= max ;
	for (j=0; j<n; j++) v[j] *= ratio;
	return (1);
}


std::string cw::find_winner (float *inbuf, int twodots)
{
	float diffsf = 999999999999.0;

	if ( normalize (inbuf, WGT_SIZE, twodots) == 0) return " ";

	int winner = -1;
	for ( int n = 0; som_table[n].rpr.length(); n++) {
		 /* Compute the distance between codebook and input entry */
		float difference = 0.0;
	   	for (int i = 0; i < WGT_SIZE; i++) {
			float diff = (inbuf[i] - som_table[n].wgt[i]);
					difference += diff * diff;
					if (difference > diffsf) break;
	  		}

	 /* If distance is smaller than previous distances */
			if (difference < diffsf) {
	  			winner = n;
	  			diffsf = difference;
			}
	}

	if (!som_table[winner].rpr.empty()) {
		string sc = morse.rx_lookup(som_table[winner].rpr);
		if (sc.empty()) return "*";
		else return sc;
	} else
		return "*";
}

void cw::tx_init()
{
	phaseacc = 0;
	lastsym = 0;
	qskphase = 0;
	if (progdefaults.pretone) pretone();

	symbols = 0;
	acc_symbols = 0;
	ovhd_symbols = 0;

	cw_xmt_filter->create_filter(lwr, upr);
	qsk_ptr = XMT_FILT_LEN / 2;
	maxval = 0.0;
}

void cw::rx_init()
{
	cw_receive_state = RS_IDLE;
	smpl_ctr = 0;
	cw_rr_current = 0;
	cw_ptr = 0;
	agc_peak = 0;
	set_scope_mode(Digiscope::SCOPE);

	update_Status();
	usedefaultWPM = false;
	scope_clear = true;

	viewcw.restart();
}

void cw::init()
{
	bool wfrev = wf->Reverse();
	bool wfsb = wf->USB();
	reverse = wfrev ^ !wfsb;

	if (progdefaults.StartAtSweetSpot)
		set_freq(progdefaults.CWsweetspot);
	else if (progStatus.carrier != 0) {
		set_freq(progStatus.carrier);
#if !BENCHMARK_MODE
		progStatus.carrier = 0;
#endif
	} else
		set_freq(wf->Carrier());

	trackingfilter->reset();
	two_dots = (long int)trackingfilter->run(2 * cw_send_dot_length);
	put_cwRcvWPM(cw_send_speed);
	for (int i = 0; i < OUTBUFSIZE; i++)
		outbuf[i] = qskbuf[i] = 0.0;
	rx_init();
	use_paren = progdefaults.CW_use_paren;
	prosigns = progdefaults.CW_prosigns;
	stopflag = false;
	maxval = 0;

	if (use_nanoIO) set_nanoCW();

}

cw::~cw() {
	if (cw_FFT_filter) delete cw_FFT_filter;
	if (cw_xmt_filter) delete cw_xmt_filter;
	if (bitfilter) delete bitfilter;
	if (trackingfilter) delete trackingfilter;
}

cw::cw() : modem()
{
	cap |= CAP_BW;

	mode = MODE_CW;
	freqlock = false;
	usedefaultWPM = false;
	frequency = progdefaults.CWsweetspot;
	tx_frequency = get_txfreq_woffset();
	risetime = progdefaults.CWrisetime;
	QSKshape = progdefaults.QSKshape;

	cw_ptr = 0;
	clrcount = CLRCOUNT;

	samplerate = CW_SAMPLERATE;
	fragmentsize = CWMaxSymLen;

	cw_speed  = progdefaults.CWspeed;
	bandwidth = progdefaults.CWbandwidth;

	cw_send_speed = cw_speed;
	cw_receive_speed = cw_speed;
	two_dots = 2 * KWPM / cw_speed;
	cw_noise_spike_threshold = two_dots / 4;
	cw_send_dot_length = KWPM / cw_send_speed;
	cw_send_dash_length = 3 * cw_send_dot_length;
	symbollen = (int)round(samplerate * 1.2 / progdefaults.CWspeed);
	fsymlen = (int)round(samplerate * 1.2 / progdefaults.CWfarnsworth);

	rx_rep_buf.clear();

// block of variables that get updated each time speed changes
	pipesize = (22 * samplerate * 12) / (progdefaults.CWspeed * 160);
	if (pipesize < 0) pipesize = 512;
	if (pipesize > MAX_PIPE_SIZE) pipesize = MAX_PIPE_SIZE;

	cwTrack = true;
	phaseacc = 0.0;
	FFTphase = 0.0;
	FFTvalue = 0.0;
	pipeptr = 0;
	clrcount = 0;

	upper_threshold = progdefaults.CWupper;
	lower_threshold = progdefaults.CWlower;
	for (int i = 0; i < MAX_PIPE_SIZE; clearpipe[i++] = 0.0);

	agc_peak = 1.0;
	in_replay = 0;

	use_matched_filter = progdefaults.CWmfilt;

	bandwidth = progdefaults.CWbandwidth;
	if (use_matched_filter)
		progdefaults.CWbandwidth = bandwidth = 2.0 * progdefaults.CWspeed / 1.2;

	cw_FFT_filter = new fftfilt(progdefaults.CWspeed/(1.2 * samplerate), CW_FFT_SIZE);

// transmit filtering

	nbfreq = frequency;
	nbpf = progdefaults.CW_bpf;
	lwr = nbfreq - nbpf/2.0;
	upr = nbfreq + nbpf/2.0;
	if (lwr < 50.0) lwr = 50.0;
	if (upr > 3900.0) upr = 3900.0;
	lwr /= samplerate;
	upr /= samplerate;
	xmt_signal = new double[XMT_FILT_LEN];
	for (int i = 0; i < XMT_FILT_LEN; i++) xmt_signal[i] = 0.0;
	qsk_signal = new double[QSK_DELAY_LEN];
	for (int i = 0; i < QSK_DELAY_LEN; i++) qsk_signal[i] = 0.0;
	cw_xmt_filter = new fftfilt( lwr, upr, 2*XMT_FILT_LEN);// transmit filter length

	int bfv = symbollen / 32;
	if (bfv < 1) bfv = 1;
	bitfilter = new Cmovavg(bfv);

	trackingfilter = new Cmovavg(TRACKING_FILTER_SIZE);

	makeshape();

	nano_wpm = progdefaults.CWspeed;
	nano_d2d = progdefaults.CWdash2dot;

	sync_parameters();
	REQ(static_cast<void (waterfall::*)(int)>(&waterfall::Bandwidth), wf, (int)bandwidth);
	REQ(static_cast<int (Fl_Value_Slider2::*)(double)>(&Fl_Value_Slider2::value), sldrCWbandwidth, (int)bandwidth);
	update_Status();

	synchscope = 50;
	noise_floor = 1.0;
	sig_avg = 0.0;

}

// SHOULD ONLY BE CALLED FROM THE rx_processing loop
void cw::reset_rx_filter()
{
	if (use_matched_filter != progdefaults.CWmfilt ||
		cw_speed != progdefaults.CWspeed ||
		(bandwidth != progdefaults.CWbandwidth && !use_matched_filter)) {

		use_matched_filter = progdefaults.CWmfilt;
		cw_send_speed = cw_speed = progdefaults.CWspeed;

		if (use_matched_filter)
			progdefaults.CWbandwidth = bandwidth = 2.0 * progdefaults.CWspeed / 1.2;
		else
			bandwidth = progdefaults.CWbandwidth;

		double fbw = 0.5 * bandwidth / samplerate;
		cw_FFT_filter->create_lpf(fbw);
		FFTphase = 0;

		REQ(static_cast<void (waterfall::*)(int)>(&waterfall::Bandwidth),
			wf, (int)bandwidth);
		REQ(static_cast<int (Fl_Value_Slider2::*)(double)>(&Fl_Value_Slider2::value),
			sldrCWbandwidth, (int)bandwidth);

		pipesize = (22 * samplerate * 12) / (progdefaults.CWspeed * 160);
		if (pipesize < 0) pipesize = 512;
		if (pipesize > MAX_PIPE_SIZE) pipesize = MAX_PIPE_SIZE;

		two_dots = 2 * KWPM / cw_speed;
		cw_noise_spike_threshold = two_dots / 4;
		cw_send_dot_length = KWPM / cw_send_speed;
		cw_send_dash_length = 3 * cw_send_dot_length;
		symbollen = (int)round(samplerate * 1.2 / progdefaults.CWspeed);
		fsymlen = (int)round(samplerate * 1.2 / progdefaults.CWfarnsworth);

		phaseacc = 0.0;
		FFTphase = 0.0;
		FFTvalue = 0.0;
		pipeptr = 0;
		clrcount = 0;
		smpl_ctr = 0;

		rx_rep_buf.clear();

	int bfv = symbollen / 32;
	if (bfv < 1) bfv = 1;
	bitfilter->setLength(bfv);

	siglevel = 0;

	}

}

// sync_parameters()
// Synchronize the dot, dash, end of element, end of character, and end
// of word timings and ranges to new values of Morse speed, or receive tolerance.

void cw::sync_transmit_parameters()
{
	wpm = usedefaultWPM ? progdefaults.defCWspeed : progdefaults.CWspeed;
	fwpm = progdefaults.CWfarnsworth;

	cw_send_dot_length = KWPM / progdefaults.CWspeed;
	cw_send_dash_length = 3 * cw_send_dot_length;

	nusymbollen = (int)round(samplerate * 1.2 / wpm);
	nufsymlen = (int)round(samplerate * 1.2 / fwpm);

	if (symbollen != nusymbollen ||
		nufsymlen != fsymlen ||
		risetime  != progdefaults.CWrisetime ||
		QSKshape  != progdefaults.QSKshape) {
		risetime = progdefaults.CWrisetime;
		QSKshape = progdefaults.QSKshape;
		symbollen = nusymbollen;
		fsymlen = nufsymlen;
		makeshape();
	}
}

void cw::sync_parameters()
{
	if (use_nanoIO) {
		if (nano_wpm != progdefaults.CWspeed) {
			nano_wpm = progdefaults.CWspeed;
			set_nanoWPM(progdefaults.CWspeed);
		}
		if (nano_d2d != progdefaults.CWdash2dot) {
			nano_d2d = progdefaults.CWdash2dot;
			set_nano_dash2dot(progdefaults.CWdash2dot);
		}
	}

	sync_transmit_parameters();

// check if user changed the tracking or the cw default speed
	if ((cwTrack != progdefaults.CWtrack) ||
		(cw_send_speed != progdefaults.CWspeed)) {
		trackingfilter->reset();
		two_dots = 2 * cw_send_dot_length;
		put_cwRcvWPM(cw_send_speed);
	}
	cwTrack = progdefaults.CWtrack;
	cw_send_speed = progdefaults.CWspeed;

// Receive parameters:
	lowerwpm = cw_send_speed - progdefaults.CWrange;
	upperwpm = cw_send_speed + progdefaults.CWrange;
	if (lowerwpm < progdefaults.CWlowerlimit)
		lowerwpm = progdefaults.CWlowerlimit;
	if (upperwpm > progdefaults.CWupperlimit)
		upperwpm = progdefaults.CWupperlimit;
	cw_lower_limit = 2 * KWPM / upperwpm;
	cw_upper_limit = 2 * KWPM / lowerwpm;

	if (cwTrack)
		cw_receive_speed = KWPM / (two_dots / 2);
	else {
		cw_receive_speed = cw_send_speed;
		two_dots = 2 * cw_send_dot_length;
	}

	if (cw_receive_speed > 0)
		cw_receive_dot_length = KWPM / cw_receive_speed;
	else
		cw_receive_dot_length = KWPM / 5;

	cw_receive_dash_length = 3 * cw_receive_dot_length;

	cw_noise_spike_threshold = cw_receive_dot_length / 2;

}


//=======================================================================
// cw_update_tracking()
//=======================================================================

inline void cw::update_tracking(int dur_1, int dur_2)
{
static int min_dot = KWPM / 200;
static int max_dash = 3 * KWPM / 5;
	if ((dur_1 > dur_2) && (dur_1 > 4 * dur_2)) return;
	if ((dur_2 > dur_1) && (dur_2 > 4 * dur_1)) return;
	if (dur_1 < min_dot || dur_2 < min_dot) return;
	if (dur_2 > max_dash || dur_2 > max_dash) return;

	two_dots = trackingfilter->run((dur_1 + dur_2) / 2);

	sync_parameters();
}

//=======================================================================
//update_syncscope()
//Routine called to update the display on the sync scope display.
//For CW this is an o scope pattern that shows the cw data stream.
//=======================================================================

void cw::update_Status()
{
	put_MODEstatus("CW %s Rx %d", usedefaultWPM ? "*" : " ", cw_receive_speed);

}

//=======================================================================
//update_syncscope()
//Routine called to update the display on the sync scope display.
//For CW this is an o scope pattern that shows the cw data stream.
//=======================================================================
//

void cw::update_syncscope()
{
	if (pipesize < 0 || pipesize > MAX_PIPE_SIZE)
		return;

	for (int i = 0; i < pipesize; i++)
		scopedata[i] = 0.96*pipe[i]+0.02;

	set_scope_xaxis_1(siglevel);

	set_scope(scopedata, pipesize, true);
	scopedata.next(); // change buffers

	clrcount = CLRCOUNT;
	put_cwRcvWPM(cw_receive_speed);
	update_Status();
}

void cw::clear_syncscope()
{
	set_scope_xaxis_1(siglevel);

	set_scope(clearpipe, pipesize, false);
	clrcount = CLRCOUNT;
}

cmplx cw::mixer(cmplx in)
{
	cmplx z (cos(phaseacc), sin(phaseacc));
	z = z * in;

	phaseacc += TWOPI * frequency / samplerate;
	if (phaseacc > TWOPI) phaseacc -= TWOPI;

	return z;
}

//=====================================================================
// cw_rxprocess()
// Called with a block (size SCBLOCKSIZE samples) of audio.
//
//======================================================================

void cw::decode_stream(double value)
{
	std::string sc;
	std::string somc;
	int attack = 0;
	int decay = 0;
	switch (progdefaults.cwrx_attack) {
		case 0: attack = 100; break;
		case 1: default: attack = 50; break;
		case 2: attack = 25;
	}
	switch (progdefaults.cwrx_decay) {
		case 0: decay = 1000; break;
		case 1: default : decay = 500; break;
		case 2: decay = 250;
	}

	sig_avg = decayavg(sig_avg, value, decay);

	if (value < sig_avg) {
		if (value < noise_floor)
			noise_floor = decayavg(noise_floor, value, attack);
		else 
			noise_floor = decayavg(noise_floor, value, decay);
	}
	if (value > sig_avg)  {
		if (value > agc_peak)
			agc_peak = decayavg(agc_peak, value, attack);
		else
			agc_peak = decayavg(agc_peak, value, decay); 
	}

	float norm_noise  = noise_floor / agc_peak;
	float norm_sig    = sig_avg / agc_peak;
	siglevel = norm_sig;

	if (agc_peak)
		value /= agc_peak;
	else
		value = 0;

	metric = 0.8 * metric;
	if ((noise_floor > 1e-4) && (noise_floor < sig_avg))
		metric += 0.2 * clamp(2.5 * (20*log10(sig_avg / noise_floor)) , 0, 100);

	float diff = (norm_sig - norm_noise);

	progdefaults.CWupper = norm_sig - 0.2 * diff;
	progdefaults.CWlower = norm_noise + 0.7 * diff;

	pipe[pipeptr] = value;
	if (++pipeptr == pipesize) pipeptr = 0;

	if (!progStatus.sqlonoff || metric > progStatus.sldrSquelchValue ) {
// Power detection using hysterisis detector
// upward trend means tone starting
		if ((value > progdefaults.CWupper) && (cw_receive_state != RS_IN_TONE)) {
			handle_event(CW_KEYDOWN_EVENT, sc);
		}
// downward trend means tone stopping
		if ((value < progdefaults.CWlower) && (cw_receive_state == RS_IN_TONE)) {
			handle_event(CW_KEYUP_EVENT, sc);
		}
	}

	if (handle_event(CW_QUERY_EVENT, sc) == CW_SUCCESS) {
		update_syncscope();
		synchscope = 100;
		if (progdefaults.CWuseSOMdecoding) {
			somc = find_winner(cw_buffer, two_dots);
			if (!somc.empty())
				for (size_t n = 0; n < somc.length(); n++)
					put_rx_char(
						somc[n],
						somc[0] == '<' ? FTextBase::CTRL : FTextBase::RECV);
			cw_ptr = 0;
			memset(cw_buffer, 0, sizeof(cw_buffer));
		} else {
			for (size_t n = 0; n < sc.length(); n++)
				put_rx_char(
					sc[n],
					sc[0] == '<' ? FTextBase::CTRL : FTextBase::RECV);
		}
	} else if (--synchscope == 0) {
		synchscope = 25;
	update_syncscope();
	}

}

void cw::rx_FFTprocess(const double *buf, int len)
{
	cmplx z, *zp;
	int n;

	while (len-- > 0) {

		z = cmplx ( *buf * cos(FFTphase), *buf * sin(FFTphase) );
		FFTphase += TWOPI * frequency / samplerate;
		if (FFTphase > TWOPI) FFTphase -= TWOPI;

		buf++;

		n = cw_FFT_filter->run(z, &zp); // n = 0 or filterlen/2

		if (!n) continue;

		for (int i = 0; i < n; i++) {
// update the basic sample counter used for morse timing
			++smpl_ctr;

			if (smpl_ctr % DEC_RATIO) continue; // decimate by DEC_RATIO

// demodulate
			FFTvalue = abs(zp[i]);
			FFTvalue = bitfilter->run(FFTvalue);

			decode_stream(FFTvalue);

		} // for (i =0; i < n ...

	} //while (len-- > 0)
}

static bool cwprocessing = false;

int cw::rx_process(const double *buf, int len)
{
	if (cwprocessing)
		return 0;

	cwprocessing = true;

	reset_rx_filter();

	rx_FFTprocess(buf, len);

	if (!clrcount--) clear_syncscope();

	display_metric(metric);

	if ( (dlgViewer->visible() || progStatus.show_channels ) 
		&& !bHighSpeed && !bHistory )
		viewcw.rx_process(buf, len);

	cwprocessing = false;

	return 0;
}

// ----------------------------------------------------------------------

// Compare two timestamps, and return the difference between them in usecs.

inline int cw::usec_diff(unsigned int earlier, unsigned int later)
{
	return (earlier >= later) ? 0 : (later - earlier);
}


//=======================================================================
// handle_event()
//	high level cw decoder... gets called with keyup, keydown, reset and
//	query commands.
//   Keyup/down influences decoding logic.
//	Reset starts everything out fresh.
//	The query command returns CW_SUCCESS and the character that has
//	been decoded (may be '*',' ' or [a-z,0-9] or a few others)
//	If there is no data ready, CW_ERROR is returned.
//=======================================================================

int cw::handle_event(int cw_event, string &sc)
{
	static int space_sent = true;	// for word space logic
	static int last_element = 0;	// length of last dot/dash
	int element_usec;		// Time difference in usecs

	switch (cw_event) {
	case CW_RESET_EVENT:
		sync_parameters();
		cw_receive_state = RS_IDLE;
		cw_rr_current = 0;			// reset decoding pointer
		cw_ptr = 0;
		memset(cw_buffer, 0, sizeof(cw_buffer));
		smpl_ctr = 0;					// reset audio sample counter
		rx_rep_buf.clear();
		break;
	case CW_KEYDOWN_EVENT:
// A receive tone start can only happen while we
// are idle, or in the middle of a character.
		if (cw_receive_state == RS_IN_TONE)
			return CW_ERROR;
// first tone in idle state reset audio sample counter
		if (cw_receive_state == RS_IDLE) {
			smpl_ctr = 0;
			rx_rep_buf.clear();
			cw_rr_current = 0;
			cw_ptr = 0;
		}
// save the timestamp
		cw_rr_start_timestamp = smpl_ctr;
// Set state to indicate we are inside a tone.
		old_cw_receive_state = cw_receive_state;
		cw_receive_state = RS_IN_TONE;
		return CW_ERROR;
		break;
	case CW_KEYUP_EVENT:
// The receive state is expected to be inside a tone.
		if (cw_receive_state != RS_IN_TONE)
			return CW_ERROR;
// Save the current timestamp
		cw_rr_end_timestamp = smpl_ctr;
		element_usec = usec_diff(cw_rr_start_timestamp, cw_rr_end_timestamp);

// make sure our timing values are up to date
		sync_parameters();
// If the tone length is shorter than any noise cancelling
// threshold that has been set, then ignore this tone.
		if (cw_noise_spike_threshold > 0
			&& element_usec < cw_noise_spike_threshold) {
			cw_receive_state = RS_IDLE;
 			return CW_ERROR;
		}

// Set up to track speed on dot-dash or dash-dot pairs for this test to work, we need a dot dash pair or a
// dash dot pair to validate timing from and force the speed tracking in the right direction. This method
// is fundamentally different than the method in the unix cw project. Great ideas come from staring at the
// screen long enough!. Its kind of simple really ... when you have no idea how fast or slow the cw is...
// the only way to get a threshold is by having both code elements and setting the threshold between them
// knowing that one is supposed to be 3 times longer than the other. with straight key code... this gets
// quite variable, but with most faster cw sent with electronic keyers, this is one relationship that is
// quite reliable. Lawrence Glaister (ve7it@shaw.ca)
		if (last_element > 0) {
// check for dot dash sequence (current should be 3 x last)
			if ((element_usec > 2 * last_element) &&
				(element_usec < 4 * last_element)) {
				update_tracking(last_element, element_usec);
			}
// check for dash dot sequence (last should be 3 x current)
			if ((last_element > 2 * element_usec) &&
				(last_element < 4 * element_usec)) {
				update_tracking(element_usec, last_element);
			}
		}
		last_element = element_usec;
// ok... do we have a dit or a dah?
// a dot is anything shorter than 2 dot times
		if (element_usec <= two_dots) {
			rx_rep_buf += CW_DOT_REPRESENTATION;
	//		printf("%d dit ", last_element/1000);  // print dot length
			cw_buffer[cw_ptr++] = (float)last_element;
		} else {
// a dash is anything longer than 2 dot times
			rx_rep_buf += CW_DASH_REPRESENTATION;
			cw_buffer[cw_ptr++] = (float)last_element;
		}
// We just added a representation to the receive buffer.
// If it's full, then reset everything as it probably noise
		if (rx_rep_buf.length() > MAX_MORSE_ELEMENTS) {
			cw_receive_state = RS_IDLE;
			cw_rr_current = 0;	// reset decoding pointer
			cw_ptr = 0;
			smpl_ctr = 0;		// reset audio sample counter
			return CW_ERROR;
		} else {
// zero terminate representation
//			rx_rep_buf.clear();
			cw_buffer[cw_ptr] = 0.0;
		}
// All is well.  Move to the more normal after-tone state.
		cw_receive_state = RS_AFTER_TONE;
		return CW_ERROR;
		break;
	case CW_QUERY_EVENT:
// this should be called quite often (faster than inter-character gap) It looks after timing
// key up intervals and determining when a character, a word space, or an error char '*' should be returned.
// CW_SUCCESS is returned when there is a printable character. Nothing to do if we are in a tone
		if (cw_receive_state == RS_IN_TONE)
			return CW_ERROR;
// compute length of silence so far
		sync_parameters();
		element_usec = usec_diff(cw_rr_end_timestamp, smpl_ctr);
// SHORT time since keyup... nothing to do yet
		if (element_usec < (2 * cw_receive_dot_length))
			return CW_ERROR;
// MEDIUM time since keyup... check for character space
// one shot through this code via receive state logic
// FARNSWOTH MOD HERE -->
		if (element_usec >= (2 * cw_receive_dot_length) &&
			element_usec <= (4 * cw_receive_dot_length) &&
			cw_receive_state == RS_AFTER_TONE) {
// Look up the representation
			sc = morse.rx_lookup(rx_rep_buf);
			if (sc.empty()) {
// invalid decode... let user see error
				sc = "*";
			}
			rx_rep_buf.clear();
			cw_receive_state = RS_IDLE;
			cw_rr_current = 0;	// reset decoding pointer
			space_sent = false;
			cw_ptr = 0;

			return CW_SUCCESS;
		}
// LONG time since keyup... check for a word space
// FARNSWOTH MOD HERE -->
		if ((element_usec > (4 * cw_receive_dot_length)) && !space_sent) {
			sc = " ";
			space_sent = true;
			return CW_SUCCESS;
		}
// should never get here... catch all
		return CW_ERROR;
		break;
	}
// should never get here... catch all
	return CW_ERROR;
}

//===========================================================================
// cw transmit routines
// Define the amplitude envelop for key down events (32 samples long)
// this is 1/2 cycle of a raised cosine
//===========================================================================

double keyshape[CWKNUM];

void cw::makeshape()
{
	for (int i = 0; i < CWKNUM; i++) keyshape[i] = 1.0;

	switch (QSKshape) {
		case 1: // blackman
			knum = (int)(8 * risetime);
			if (knum >= symbollen) knum = symbollen;
			for (int i = 0; i < knum; i++)
				keyshape[i] = (0.42 - 0.50 * cos(M_PI * i/ knum) + 0.08 * cos(2 * M_PI * i / knum));
			break;
		case 0: // hanning
		default:
			knum = (int)(8 * risetime);
			if (knum >= symbollen) knum = symbollen;
			for (int i = 0; i < knum; i++)
				keyshape[i] = 0.5 * (1.0 - cos (M_PI * i / knum));
	}
}

inline double cw::nco(double freq)
{
	phaseacc += 2.0 * M_PI * freq / samplerate;

	if (phaseacc > TWOPI) phaseacc -= TWOPI;

	return sin(phaseacc);
}

inline double cw::qsknco()
{
	double amp = sin(qskphase);
	qskphase += 2.0 * M_PI * 1000 / samplerate;
	if (qskphase > TWOPI) qskphase -= TWOPI;
	return amp;
}

//=====================================================================
// send_symbol()
// Sends a part of a morse character (one dot duration) of either
// sound at the correct freq or silence. Rise and fall time is controlled
// with a raised cosine shape.
//
// Left channel contains the shaped A2 CW waveform
// Right channel contains a square wave burst of 1600 Hz that is used
// to trigger a qsk switch.  Right channel has pre and post timings for
// proper switching of the qsk switch before and after the A2 element.
// If the Pre + Post timing exceeds the interelement spacing then the
// Pre and / or Post is only applied at the beginning and end of the
// character.
//=======================================================================

//void appendfile(double *left, double *right, size_t count)
//{
//	FILE *ofile = fl_fopen("stereo.txt", "a");
//	for (size_t i = 0; i < count; i++)
//		fprintf(ofile,"%f,%f\n", left[i], right[i]);
//	fclose(ofile);
//}

void cw::nb_filter(double *output, double *qsk, int len)
{
// fft implementation of 
	if (nbfreq != tx_frequency ||
		nbpf != progdefaults.CW_bpf) {

		nbfreq = tx_frequency;
		nbpf = progdefaults.CW_bpf;
		lwr = nbfreq - nbpf/2.0;
		upr = nbfreq + nbpf/2.0;
		if (lwr < 50.0) lwr = 50.0;
		if (upr > 3900.0) upr = 3900.0;
		lwr /= samplerate;
		upr /= samplerate;
		cw_xmt_filter->create_filter(lwr, upr);
	}
	cmplx *zp;
	int n = 0;
	for (int i = 0; i < len; i++) {
// n will be either 0 or XMT_FILT_LEN
		qsk_signal[qsk_ptr] = qsk[i];
		qsk_ptr++;
		if (qsk_ptr == QSK_DELAY_LEN) qsk_ptr--;
		if ((n = cw_xmt_filter->run(
					cmplx(output[i], output[i]), 
					&zp)) > 0) {
			for (int k = 0; k < n; k++)
				if (abs(zp[k].real()) > maxval) maxval = abs(zp[k].real());
			if (maxval == 0) maxval = 1.0;
			for (int k = 0; k < n; k++)
				xmt_signal[k] = 0.95 * zp[k].real() / maxval;

//			appendfile(xmt_signal, qsk_signal, n);

			if (progdefaults.QSK)
				ModulateStereo(xmt_signal, qsk_signal, n);
			else
				ModulateXmtr(xmt_signal, n);
			for (int k = 0; k < QSK_DELAY_LEN - n - 1; k++)
				qsk_signal[k] = qsk_signal[k + n];
			qsk_ptr -= n;
		}
	}
}

int q_carryover = 0, carryover = 0;

void cw::send_symbol(int bits, int len)
{
	double freq;
	int sample, qsample, i;
	int delta = 0;
	int keydown;
	int keyup;
	int kpre;
	int kpost;
	int duration = 0;
	int symlen = 0;
	float dsymlen = 0.0;
	int currsym = bits & 1;
	double qsk_amp = progdefaults.QSK ? 1.0 : 0.0;

	sync_transmit_parameters();

	acc_symbols += len;

	acc_symbols += len;

	freq = get_txfreq_woffset();

	delta = (int) (len * (progdefaults.CWweight - 50) / 100.0);

	symlen = len;
	if (currsym == 1) {
   		dsymlen = len * (progdefaults.CWdash2dot - 3.0) / (progdefaults.CWdash2dot + 1.0);
		if (lastsym == 1 && currsym == 1)
			symlen += (int)(3 * dsymlen);
		else
			symlen -= (int)dsymlen;
	}

	if (delta < -(symlen - knum)) delta = -(symlen - knum);
	if (delta > (symlen - knum)) delta = symlen - knum;

	keydown = symlen + delta ;
	keyup = symlen - delta;

	kpre = (int)(progdefaults.CWpre * 8);
	if (kpre > symlen) kpre = symlen;
	if (progdefaults.QSK) {
		kpre = (int)(progdefaults.CWpre * 8);
		if (kpre > symlen) kpre = symlen;

		if (progdefaults.CWnarrow) {
			if (keydown - 2*knum < 0)
				kpost = knum + (int)(progdefaults.CWpost * 8);
			else
				kpost = keydown - knum + (int)(progdefaults.CWpost * 8);
		} else
			kpost = keydown + (int)(progdefaults.CWpost * 8);
		if (kpost < 0)
			kpost = 0;
	} else {
		kpre = 0;
		kpost = 0;
	}

	if (firstelement) {
		firstelement = false;
		return;
	}

	if (currsym == 1) { // keydown
		sample = 0;
		if (lastsym == 1) {
			for (i = 0; i < keydown; i++, sample++) {
				outbuf[sample] = nco(freq);
				qskbuf[sample] = qsk_amp * qsknco();
			}
			duration = keydown;
		} else {
			if (carryover) {
				for (int i = carryover; i < knum; i++, sample++)
					outbuf[sample] = nco(freq) * keyshape[knum - i];
				while (sample < kpre)
					outbuf[sample++] = 0 * nco(freq);
			} else
				for (int i = 0; i < kpre; i++, sample++)
					outbuf[sample] = 0 * nco(freq);
			sample = 0;
			for (int i = 0; i < kpre; i++, sample++) {
				qskbuf[sample] = qsk_amp * qsknco();
			}
			for (int i = 0; i < knum; i++, sample++) {
				outbuf[sample] = nco(freq) * keyshape[i];
				qskbuf[sample] = qsk_amp * qsknco();
			}
			duration = kpre + knum;
		}
		carryover = 0;
	}
	else { // keyup
		if (lastsym == 0) {
			duration = keyup;
			sample = 0;
			if (carryover) {
				for (int i = carryover; i < knum; i++, sample++)
					outbuf[sample] = nco(freq) * keyshape[knum - i];
				while (sample < duration)
					outbuf[sample++] = 0 * nco(freq);
			} else
				while (sample < duration)
					outbuf[sample++] = 0 * nco(freq);
			carryover = 0;

			qsample = 0;
			if (q_carryover) {
				for (int i = 0; i < q_carryover; i++, qsample++) {
					qskbuf[qsample] = qsk_amp * qsknco();
				}
				while (qsample < duration)
					qskbuf[qsample++] = 0 * qsknco();
			} else
				while (qsample < duration)
					qskbuf[qsample++] = 0 * qsknco();
			if (q_carryover > duration)
				q_carryover = duration - q_carryover;
			else
				q_carryover = 0;

		} else { // last symbol = 1
			duration = 2 * len - kpre - knum;
			carryover = 0;
			sample = 0;

			int next = keydown - knum;
			if (progdefaults.CWnarrow)
				next = keydown - 2*knum;

			for (int i = 0; i < next; i++, sample++)
				outbuf[sample] = nco(freq);

			for (int i = 0; i < knum; i++, sample++) {
				if (sample == duration) {
					carryover = i;
					break;
				}
				outbuf[sample] = nco(freq) * keyshape[knum - i];
			}
			while (sample < duration)
				outbuf[sample++] = 0 * nco(freq);

			q_carryover = 0;
			qsample = 0;

			for (int i = 0; i < kpost; i++, qsample++) {
				if (qsample == duration) {
					q_carryover = kpost - duration;
					break;
				}
				qskbuf[qsample] = qsk_amp * qsknco();
			}
			while (qsample < duration)
				qskbuf[qsample++] = 0 * qsknco();
		}
	}

	if (duration > 0) {
		if (progdefaults.CW_bpf_on)
			nb_filter(outbuf, qskbuf, duration);
		else {
			if (progdefaults.QSK)
				ModulateStereo(outbuf, qskbuf, duration);
			else
				ModulateXmtr(outbuf, duration);
		}
	}

	lastsym = currsym;
	firstelement = false;
}

//=====================================================================
// send_ch()
// sends a morse character and the space afterwards
//=======================================================================

void cw::send_ch(int ch)
{
	string code;
	int chout = ch;
	int flen;

	sync_parameters();

// handle word space separately (7 dots spacing)
// last char already had 3 elements of inter-character spacing

	double T = (50.0 - 31 * fsymlen / symbollen) / 19.0;

	if (!progdefaults.CWusefarnsworth || (progdefaults.CWspeed >= progdefaults.CWfarnsworth))
		T = 1.0;

	int tc = 3.0 * T * symbollen;
	int tw = 7.0 * T * symbollen;

	if ((chout == ' ') || (chout == '\n')) {
		firstelement = false;
		flen = tw - tc - ( T > 1.0 ? fsymlen : 0);
		while (flen - symbollen > 0) {
			send_symbol(0, symbollen);
			flen -= symbollen;
		}
		if (flen) send_symbol(0, flen);
		put_echo_char(progdefaults.rx_lowercase ? tolower(ch) : ch);
		return;
	}

	if (progdefaults.CWusefarnsworth && (progdefaults.CWspeed <= progdefaults.CWfarnsworth))
		flen = fsymlen;
	else
		flen = symbollen;

	code = morse.tx_lookup(chout);
	if (!code.length()) {
		return;
	}

	firstelement = true;

//	while (code.length()) {
	for (size_t n = 0; n < code.length(); n++) {
		send_symbol(0, flen);
		send_symbol(1, flen);
		if (code[n] == '-') {
			send_symbol( 1, flen );
			send_symbol( 1, flen );
		}
	}
	send_symbol(0, flen);
	send_symbol(0, flen);
	send_symbol(0, flen);

// inter character space at WPM/FWPM rate
//	if (progdefaults.CWusefarnsworth && (progdefaults.CWspeed <= progdefaults.CWfarnsworth)) {
	if (T > 1.0) {
		flen = tc - 3 * flen;
		while(flen - symbollen > 0) {
			send_symbol(0, symbollen);
			flen -= symbollen;
		}
		if (flen) send_symbol(0, flen);
	}

	if (ch != -1) {
		string prtstr = morse.tx_print();
		for (size_t n = 0; n < prtstr.length(); n++)
			put_echo_char(
				prtstr[n],
				prtstr[0] == '<' ? FTextBase::CTRL : FTextBase::XMIT);
	}
}

//=====================================================================
// cw_txprocess()
// Read characters from screen and send them out the sound card.
// This is called repeatedly from a thread during tx.
//=======================================================================

int cw::tx_process()
{
	int c;

	if (use_paren != progdefaults.CW_use_paren ||
		prosigns != progdefaults.CW_prosigns) {
		use_paren = progdefaults.CW_use_paren;
		prosigns = progdefaults.CW_prosigns;
		morse.init();
	}

	c = get_tx_char();

	if (c == GET_TX_CHAR_NODATA) {
		if (stopflag) {
			stopflag = false;
			if (progdefaults.CW_bpf_on) { // flush the transmit bandpass filter
				int n = cw_xmt_filter->flush_size();
				for (int i = 0; i < n; i++) outbuf[i] = 0;
				nb_filter(outbuf, outbuf, n);
			}
			put_echo_char('\n');
			return -1;
		}
		Fl::awake();
		MilliSleep(50);
		return 0;
	}

	if (progStatus.WK_online) {
		if (c == GET_TX_CHAR_ETX || stopflag) {
			stopflag = false;
			put_echo_char('\n');
			return -1;
		}
		if (WK_send_char(c)){
			put_echo_char('\n');
			return -1; // WinKeyer problem
		}
		return 0;
	}

	if (use_nanoIO) {
		if (c == GET_TX_CHAR_ETX || stopflag) {
			stopflag = false;
			put_echo_char('\n');
			nano_serial_flush();
			return -1;
		}
		nano_send_char(c);
		put_echo_char(c);
		return 0;
	}

	if (c == GET_TX_CHAR_ETX || stopflag) {
		stopflag = false;
		if (progdefaults.CW_bpf_on) { // flush the transmit bandpass filter
			int n = cw_xmt_filter->flush_size();
			for (int i = 0; i < n; i++) outbuf[i] = 0;
			nb_filter(outbuf, outbuf, n);
		}
		put_echo_char('\n');
		return -1;
	}

	acc_symbols = 0;
	send_ch(c);

	char_samples = acc_symbols;

	return 0;
}

void cw::incWPM()
{

	if (usedefaultWPM) return;
	if (progdefaults.CWspeed < progdefaults.CWupperlimit) {
		progdefaults.CWspeed++;
		sync_parameters();
		set_CWwpm();
		update_Status();
	}
}

void cw::decWPM()
{

	if (usedefaultWPM) return;
	if (progdefaults.CWspeed > progdefaults.CWlowerlimit) {
		progdefaults.CWspeed--;
		set_CWwpm();
		sync_parameters();
		update_Status();
	}
}

void cw::toggleWPM()
{
	usedefaultWPM = !usedefaultWPM;
	sync_parameters();
	update_Status();
}

