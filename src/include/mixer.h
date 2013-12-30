// ----------------------------------------------------------------------------
//
//      mixer.h
//
// Copyright (C) 2006-2007
//              Dave Freese, W1HKJ
//
// Copyright (C) 2007-2008
//              Stelios Bounanos, M0GLD
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

#ifndef MIXER_H
#define MIXER_H

#include <cstdio>
#include <cstring>
#include <string>

#if USE_OSS
#  if defined(__OpenBSD__)
#    include <soundcard.h>
#  else
#    include <sys/soundcard.h>
#  endif
#endif

class MixerException {
	char	szError[80];
	int		error;
public:
	MixerException() { *szError = 0; error = 0; }
	MixerException(int e) {
		snprintf(szError, sizeof(szError), "Mixer error %d: %s", e, strerror(e));
		error = e;
	}
	MixerException(char *s) {
		snprintf(szError, sizeof(szError), "Mixer error: %s", s);
		error = 1;
	}
	const char* what(void) const { return szError; }
};

class MixerBase
{
public:
	MixerBase() { }
	virtual ~MixerBase() { }

	virtual void	openMixer(const char* dev = "/dev/mixer") { };
	virtual void	closeMixer(void) { };

	virtual void	setXmtLevel(float v) { };
	virtual void	setRcvGain(float v) { };

	virtual float	PCMVolume(void) { return 0; };
	virtual void	PCMVolume(float volume) { };

	virtual int	InputSourceNbr(const char *source) { return 0; };

	virtual void	SetCurrentInputSource(int i) { };
};

#if USE_OSS

class MixerOSS : public MixerBase
{
private:
        std::string	mixer;
	int		mixer_fd;
	int		recmask;
	int		devmask;
	int		outmask;
	int		recsrc;
	int		recmask0;
	int		devmask0;
	int		recsrc0;
	int		num_out;
	int		outs[SOUND_MIXER_NRDEVICES];
	int		num_rec;
	int		recs[SOUND_MIXER_NRDEVICES];

// values on init for restoration	
	int		inpsrc0;
	float	linelevel0;
//	float	lineplaythrough0;
	float	miclevel0;
//	float	micplaythrough0;
	float	pcmlevel0;
	float	vollevel0;
//	float	playthrough0;

	int		NumMixers;
	int		NumDevice;
	int		Devices[10];
	char	szDevice[12];

	int		initMask();
	void	findNumMixers();
	float	ChannelVolume(int);

	void	initValues();
	void	restoreValues();

public:
	MixerOSS();
	~MixerOSS();
	void		openMixer(const char *dev = "/dev/mixer");
	void		closeMixer();
	
	void		setXmtLevel(float v);
	void		setRcvGain(float v);
	
	float		PCMVolume();
	void		PCMVolume(float volume );
	int		InputSourceNbr(const char *source);
	void		SetCurrentInputSource( int i );
//	float		GetPlaythrough();
//	void		SetPlaythrough( float volume );
//	void		SetMuteInput(bool);

protected:
	int		numMixers() { return NumMixers;}
	int		MixerNum(int i) { return Devices[i];}
	const char *	MixerName( int index );
	float		OutVolume();
	void		OutVolume(float vol);

	int		NumOutputVolumes();
	float		OutputVolume( int i );
	void		OutputVolume( int i, float volume );
	const char *	OutputVolumeName( int i );
	int				GetNumInputSources();
	const char *	GetInputSourceName( int i);

	float		InputVolume();
	void		InputVolume( float volume );
	int		GetCurrentInputSource();
};

#endif // USE_OSS

#endif
