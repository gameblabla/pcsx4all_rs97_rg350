/***************************************************************************
 *   Copyright (C) 2007 PCSX-df Team                                       *
 *   Copyright (C) 2009 Wei Mingzhi                                        *
 *   Copyright (C) 2012 notaz                                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

// NOTE: Code here adapted from newer PCSX Rearmed/Reloaded code

#include "psxcommon.h"
#include "plugins.h"
#include "cdrom.h"
#include "cdriso.h"
#include "ppf.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <process.h>
#include <windows.h>

#define strcasecmp _stricmp
#define fseeko fseek
#define ftello ftell
#else // UNIX:
#include <pthread.h>
#endif

#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <zlib.h>

#define OFF_T_MSB ((off_t)1 << (sizeof(off_t) * 8 - 1))

static FILE *cdHandle = NULL;
static FILE *cddaHandle = NULL;
static FILE *subHandle = NULL;

static uint_fast8_t subChanMixed = FALSE;
static uint_fast8_t subChanRaw = FALSE;
static uint_fast8_t subChanMissing = FALSE;

static uint_fast8_t multifile = FALSE;

static unsigned char cdbuffer[CD_FRAMESIZE_RAW];
static unsigned char subbuffer[SUB_FRAMESIZE];

static unsigned char sndbuffer[CD_FRAMESIZE_RAW * 10];

unsigned char *(*CDR_getBuffer)(void);

#define CDDA_FRAMETIME			(1000 * (sizeof(sndbuffer) / CD_FRAMESIZE_RAW) / 75)

#ifdef _WIN32
static HANDLE threadid;
#else
static pthread_t threadid;
#endif
static unsigned int initial_offset = 0;
static uint_fast8_t playing = FALSE;
static uint_fast8_t cddaBigEndian = FALSE;

// cdda sectors in toc, byte offset in file
static unsigned int cdda_cur_sector;
static unsigned int cdda_first_sector;
static unsigned int cdda_file_offset;
/* Frame offset into CD image where pregap data would be found if it was there.
 * If a game seeks there we must *not* return subchannel data since it's
 * not in the CD image, so that cdrom code can fake subchannel data instead.
 * XXX: there could be multiple pregaps but PSX dumps only have one? */
static unsigned int pregapOffset;

#define cddaCurPos cdda_cur_sector

// compressed image stuff
typedef struct {
	unsigned char buff_raw[16][CD_FRAMESIZE_RAW];
	unsigned char buff_compressed[CD_FRAMESIZE_RAW * 16 + 100];
	off_t *index_table;
	unsigned int index_len;
	unsigned int block_shift;
	unsigned int current_block;
	unsigned int sector_in_blk;
} COMPR_IMG;

static COMPR_IMG *compr_img;

int (*cdimg_read_func)(FILE *f, unsigned int base, void *dest, int sector);

char* CDR__getDriveLetter(void);
long CDR__configure(void);
long CDR__test(void);
void CDR__about(void);
long CDR__setfilename(char *filename);
long CDR__getStatus(struct CdrStat *stat);

static void DecodeRawSubData(void);

typedef enum {
	DATA = 1,
	CDDA
} cd_type;

struct trackinfo {
	cd_type type;
	char start[3];		// MSF-format
	char length[3];		// MSF-format
	FILE *handle;		// for multi-track images CDDA
	unsigned int start_offset; // byte offset from start of above file
};

#define MAXTRACKS 100 /* How many tracks can a CD hold? */

static int numtracks = 0;
static struct trackinfo ti[MAXTRACKS];

static char IsoFile[MAXPATHLEN] = "";
static int64_t cdOpenCaseTime = 0;

//-----------------------------------------------------------------------------
// Multi-CD image section (PSP Eboot .pbp files: see handlepbp() )
//-----------------------------------------------------------------------------
unsigned int cdrIsoMultidiskCount = 0;
unsigned int cdrIsoMultidiskSelect = 0;
//senquack - The frontend GUI can register a callback function that gets called
// when a multi-CD image is detected on load (Eboot .pbp format supports this),
// allowing user to select which CD to boot from. Important for games like
// Resident Evil 2 which allow different story arcs depending on boot CD.
// The callback function will return having set cdrIsoMultidiskSelect.
void (CALLBACK *cdrIsoMultidiskCallback)(void) = NULL;
//-----------------------------------------------------------------------------
// END Multi-CD image section
//-----------------------------------------------------------------------------

// for CD swap
int ReloadCdromPlugin()
{
	if (cdrIsoActive())
		CDR_shutdown();

	return CDR_init();
}

void SetIsoFile(const char *filename) {
	//Reset multi-CD count & selection when loading any ISO
	cdrIsoMultidiskCount = 0;
	cdrIsoMultidiskSelect = 0;

	if (filename == NULL) {
		IsoFile[0] = '\0';
		return;
	}
	strcpy(IsoFile, filename);
}

const char *GetIsoFile(void) {
	return IsoFile;
}

uint_fast8_t UsingIso(void) {
	return (IsoFile[0] != '\0');
}

void SetCdOpenCaseTime(int64_t time) {
	cdOpenCaseTime = time;
}

int64_t GetCdOpenCaseTime(void)
{
	return cdOpenCaseTime;
}

// get a sector from a msf-array
static unsigned int msf2sec(char *msf) {
	return ((msf[0] * 60 + msf[1]) * 75) + msf[2];
}

static void sec2msf(unsigned int s, char *msf) {
	msf[0] = s / 75 / 60;
	s = s - msf[0] * 75 * 60;
	msf[1] = s / 75;
	s = s - msf[1] * 75;
	msf[2] = s;
}

// divide a string of xx:yy:zz into m, s, f
static void tok2msf(char *time, char *msf) {
	char *token;

	token = strtok(time, ":");
	if (token) {
		msf[0] = atoi(token);
	}
	else {
		msf[0] = 0;
	}

	token = strtok(NULL, ":");
	if (token) {
		msf[1] = atoi(token);
	}
	else {
		msf[1] = 0;
	}

	token = strtok(NULL, ":");
	if (token) {
		msf[2] = atoi(token);
	}
	else {
		msf[2] = 0;
	}
}

#ifndef _WIN32
static long GetTickCount(void) {
	static time_t		initial_time = 0;
	struct timeval		now;

	gettimeofday(&now, NULL);

	if (initial_time == 0) {
		initial_time = now.tv_sec;
	}

	return (now.tv_sec - initial_time) * 1000L + now.tv_usec / 1000L;
}
#endif

#ifdef _WIN32
static void playthread(void *param)
#else
static void *playthread(void *param)
#endif
{
	long osleep, d, t, i, s;
	unsigned char	tmp;
	int ret = 0, sector_offs;

	t = GetTickCount();

	while (playing) {
		s = 0;
		for (i = 0; i < sizeof(sndbuffer) / CD_FRAMESIZE_RAW; i++) {
			sector_offs = cdda_cur_sector - cdda_first_sector;
			if (sector_offs < 0) {
				d = CD_FRAMESIZE_RAW;
				memset(sndbuffer + s, 0, d);
			}
			else {
				d = cdimg_read_func(cddaHandle, cdda_file_offset,
					sndbuffer + s, sector_offs);
				if (d < CD_FRAMESIZE_RAW)
					break;
			}

			s += d;
			cdda_cur_sector++;
		}

		if (s == 0) {
			playing = FALSE;
			initial_offset = 0;
			break;
		}

		if (!cdr.Muted && playing) {
			if (cddaBigEndian) {
				for (i = 0; i < s / 2; i++) {
					tmp = sndbuffer[i * 2];
					sndbuffer[i * 2] = sndbuffer[i * 2 + 1];
					sndbuffer[i * 2 + 1] = tmp;
				}
			}

			// can't do it yet due to readahead..
			//cdrAttenuate((short *)sndbuffer, s / 4, 1);
			do {
				ret = SPU_playCDDAchannel((short *)sndbuffer, s);
				if (ret == 0x7761)
					usleep(6 * 1000);
			} while (ret == 0x7761 && playing); // rearmed_wait
		}

		if (ret != 0x676f) { // !rearmed_go
			// do approx sleep
			long now;

			//senquack - disabled pcsxReARMed-specific stuff we don't have (yet)
			// HACK: stop feeding data while emu is paused
			//extern int stop;
			//while (stop && playing)
			//	usleep(10000);

			now = GetTickCount();
			osleep = t - now;
			if (osleep <= 0) {
				osleep = 1;
				t = now;
			}
			else if (osleep > CDDA_FRAMETIME) {
				osleep = CDDA_FRAMETIME;
				t = now;
			}

			usleep(osleep * 1000);
			t += CDDA_FRAMETIME;
		}

	}

#ifdef _WIN32
	_endthread();
#else
	pthread_exit(0);
	return NULL;
#endif
}

// stop the CDDA playback
static void stopCDDA() {
	if (!playing) {
		return;
	}

	playing = FALSE;
#ifdef _WIN32
	WaitForSingleObject(threadid, INFINITE);
#else
	pthread_join(threadid, NULL);
#endif
}

// start the CDDA playback
static void startCDDA(void) {
	if (playing) {
		stopCDDA();
	}

	playing = TRUE;

#ifdef _WIN32
	threadid = (HANDLE)_beginthread(playthread, 0, NULL);
#else
	pthread_create(&threadid, NULL, playthread, NULL);
#endif
}

// this function tries to get the .toc file of the given .bin
// the necessary data is put into the ti (trackinformation)-array
static int parsetoc(const char *isofile) {
	char			tocname[MAXPATHLEN];
	FILE			*fi;
	char			linebuf[256], tmp[256], name[256];
	char			*token;
	char			time[20], time2[20];
	unsigned int	t, sector_offs, sector_size;
	unsigned int	current_zero_gap = 0;

	numtracks = 0;

	// copy name of the iso and change extension from .bin to .toc
	strncpy(tocname, isofile, sizeof(tocname));
	tocname[MAXPATHLEN - 1] = '\0';
	if (strlen(tocname) >= 4) {
		strcpy(tocname + strlen(tocname) - 4, ".toc");
	} else {
		return -1;
	}

	uint_fast8_t toc_named_as_cue = false;
	if ((fi = fopen(tocname, "r")) == NULL) {
		// try changing extension to .cue (to satisfy some stupid tutorials)
		strcpy(tocname + strlen(tocname) - 4, ".cue");

		if ((fi = fopen(tocname, "r")) == NULL) {
			// if filename is image.toc.bin, try removing .bin (for Brasero)
			strcpy(tocname, isofile);
			t = strlen(tocname);
			if (t >= 8 && strcmp(tocname + t - 8, ".toc.bin") == 0) {
				tocname[t - 4] = '\0';
				if ((fi = fopen(tocname, "r")) == NULL) {
					return -1;
				}
			} else {
				return -1;
			}
		} else {
			toc_named_as_cue = true;
		}
	}

	// check if it's really a TOC and not a CUE
	uint_fast8_t is_toc_file = false;
	while (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		if (strstr(linebuf, "TRACK") != NULL) {
			char* mode_substr = strstr(linebuf, "MODE");
			if (mode_substr != NULL &&
				(mode_substr[4] == '1' || mode_substr[4] == '2') &&
			    mode_substr[5] != '/') {
				// A line containing both the substrings "TRACK" and either
				//  "MODE1" or "MODE2" exists, and the mode string lacks a
				//  trailing slash, which would have indicated a CUE file.
				is_toc_file = true;

				if (toc_named_as_cue)
					printf("\nWarning: .CUE file is really a .TOC file (processing as TOC..)\n");
			}
		}
	}

	if (!is_toc_file) {
		fclose(fi);
		return -1;
	}

	fseek(fi, 0, SEEK_SET);
	memset(&ti, 0, sizeof(ti));
	cddaBigEndian = TRUE; // cdrdao uses big-endian for CD Audio
	sector_size = CD_FRAMESIZE_RAW;
	sector_offs = 2 * 75;

	// parse the .toc file
	while (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		// search for tracks
		strncpy(tmp, linebuf, sizeof(linebuf));
		token = strtok(tmp, " ");

		if (token == NULL) continue;

		if (!strcmp(token, "TRACK")) {
			sector_offs += current_zero_gap;
			current_zero_gap = 0;

			// get type of track
			token = strtok(NULL, " ");
			numtracks++;

			if (!strncmp(token, "MODE2_RAW", 9)) {
				ti[numtracks].type = DATA;
				sec2msf(2 * 75, ti[numtracks].start); // assume data track on 0:2:0

				// check if this image contains mixed subchannel data
				token = strtok(NULL, " ");
				if (token != NULL && !strncmp(token, "RW", 2)) {
					sector_size = CD_FRAMESIZE_RAW + SUB_FRAMESIZE;
					subChanMixed = TRUE;
					if (!strncmp(token, "RW_RAW", 6))
						subChanRaw = TRUE;
				}
			}
			else if (!strncmp(token, "AUDIO", 5)) {
				ti[numtracks].type = CDDA;
			}
		}
		else if (!strcmp(token, "DATAFILE")) {
			if (ti[numtracks].type == CDDA) {
				sscanf(linebuf, "DATAFILE \"%[^\"]\" #%d %8s", name, &t, time2);
				ti[numtracks].start_offset = t;
				t = t / sector_size + sector_offs;
				sec2msf(t, (char *)&ti[numtracks].start);
				tok2msf((char *)&time2, (char *)&ti[numtracks].length);
			}
			else {
				sscanf(linebuf, "DATAFILE \"%[^\"]\" %8s", name, time);
				tok2msf((char *)&time, (char *)&ti[numtracks].length);
			}
		}
		else if (!strcmp(token, "FILE")) {
			sscanf(linebuf, "FILE \"%[^\"]\" #%d %8s %8s", name, &t, time, time2);
			tok2msf((char *)&time, (char *)&ti[numtracks].start);
			t += msf2sec(ti[numtracks].start) * sector_size;
			ti[numtracks].start_offset = t;
			t = t / sector_size + sector_offs;
			sec2msf(t, (char *)&ti[numtracks].start);
			tok2msf((char *)&time2, (char *)&ti[numtracks].length);
		}
		else if (!strcmp(token, "ZERO") || !strcmp(token, "SILENCE")) {
			// skip unneeded optional fields
			while (token != NULL) {
				token = strtok(NULL, " ");
				if (strchr(token, ':') != NULL)
					break;
			}
			if (token != NULL) {
				tok2msf(token, tmp);
				current_zero_gap = msf2sec(tmp);
			}
			if (numtracks > 1) {
				t = ti[numtracks - 1].start_offset;
				t /= sector_size;
				pregapOffset = t + msf2sec(ti[numtracks - 1].length);
			}
		}
		else if (!strcmp(token, "START")) {
			token = strtok(NULL, " ");
			if (token != NULL && strchr(token, ':')) {
				tok2msf(token, tmp);
				t = msf2sec(tmp);
				ti[numtracks].start_offset += (t - current_zero_gap) * sector_size;
				t = msf2sec(ti[numtracks].start) + t;
				sec2msf(t, (char *)&ti[numtracks].start);
			}
		}
	}

	if (numtracks <= 0) goto error;

	fclose(fi);
	return 0;

error:
	printf("\nError reading .TOC file %s\n", tocname);
	fclose(fi);
	return -1;
}

// this function tries to get the .cue file of the given .bin
// the necessary data is put into the ti (trackinformation)-array
static int parsecue(const char *isofile) {
	char			cuename[MAXPATHLEN];
	char			filepath[MAXPATHLEN];
	char			*incue_fname;
	FILE			*fi;
	char			*token;
	char			time[20];
	char			*tmp;
	char			linebuf[256], tmpb[256], dummy[256];
	unsigned int	incue_max_len;
	unsigned int	t, file_len, mode, sector_offs;
	unsigned int	sector_size = CD_FRAMESIZE_RAW;

	numtracks = 0;

	// copy name of the iso and change extension from .bin to .cue
	strncpy(cuename, isofile, sizeof(cuename));
	cuename[MAXPATHLEN - 1] = '\0';
	if (strlen(cuename) >= 4) {
		strcpy(cuename + strlen(cuename) - 4, ".cue");
	} else {
		return -1;
	}

	if ((fi = fopen(cuename, "r")) == NULL) {
		return -1;
	}

	// Some stupid tutorials wrongly tell users to use cdrdao to rip a
	// "bin/cue" image, which is in fact a "bin/toc" image. So let's check
	// that...
	while (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		if (strstr(linebuf, "TRACK") != NULL) {
			char* mode_substr = strstr(linebuf, "MODE");
			if (mode_substr != NULL &&
			    (mode_substr[4] == '1' || mode_substr[4] == '2') &&
			    mode_substr[5] != '/') {
				// A line containing both the substrings "TRACK" and either
				//  "MODE1" or "MODE2" exists, and the mode string lacks a
				//  trailing slash, which indicates this is a .TOC file
				//  falsely named as a .CUE file.
				printf("\nWarning: .CUE file is really a .TOC file (processing as TOC..)\n");
				fclose(fi);
				return parsetoc(isofile);
			}
		}
	}
	fseek(fi, 0, SEEK_SET);

	// build a path for files referenced in .cue
	strncpy(filepath, cuename, sizeof(filepath));
	tmp = strrchr(filepath, '/');
	if (tmp == NULL)
		tmp = strrchr(filepath, '\\');
	if (tmp != NULL)
		tmp++;
	else
		tmp = filepath;
	*tmp = 0;
	filepath[sizeof(filepath) - 1] = 0;
	incue_fname = tmp;
	incue_max_len = sizeof(filepath) - (tmp - filepath) - 1;

	memset(&ti, 0, sizeof(ti));

	file_len = 0;
	sector_offs = 2 * 75;

	while (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		strncpy(dummy, linebuf, sizeof(linebuf));
		token = strtok(dummy, " ");

		if (token == NULL) {
			continue;
		}

		if (!strcmp(token, "TRACK")) {
			numtracks++;

			sector_size = 0;
			if (strstr(linebuf, "AUDIO") != NULL) {
				ti[numtracks].type = CDDA;
				sector_size = CD_FRAMESIZE_RAW;
			}
			else if (sscanf(linebuf, " TRACK %u MODE%u/%u", &t, &mode, &sector_size) == 3)
				ti[numtracks].type = DATA;
			else {
				printf(".cue: failed to parse TRACK\n");
				ti[numtracks].type = numtracks == 1 ? DATA : CDDA;
			}
			if (sector_size == 0)
				sector_size = CD_FRAMESIZE_RAW;
		}
		else if (!strcmp(token, "INDEX")) {
			if (sscanf(linebuf, " INDEX %02d %8s", &t, time) != 2)
				printf(".cue: failed to parse INDEX\n");
			tok2msf(time, (char *)&ti[numtracks].start);

			t = msf2sec(ti[numtracks].start);
			ti[numtracks].start_offset = t * sector_size;
			t += sector_offs;
			sec2msf(t, ti[numtracks].start);

			// default track length to file length
			t = file_len - ti[numtracks].start_offset / sector_size;
			sec2msf(t, ti[numtracks].length);

			if (numtracks > 1 && ti[numtracks].handle == NULL) {
				// this track uses the same file as the last,
				// start of this track is last track's end
				t = msf2sec(ti[numtracks].start) - msf2sec(ti[numtracks - 1].start);
				sec2msf(t, ti[numtracks - 1].length);
			}
			if (numtracks > 1 && pregapOffset == -1)
				pregapOffset = ti[numtracks].start_offset / sector_size;
		}
		else if (!strcmp(token, "PREGAP")) {
			if (sscanf(linebuf, " PREGAP %8s", time) == 1) {
				tok2msf(time, dummy);
				sector_offs += msf2sec(dummy);
			}
			pregapOffset = -1; // mark to fill track start_offset
		}
		else if (!strcmp(token, "FILE")) {
			t = sscanf(linebuf, " FILE \"%255[^\"]\"", tmpb);
			if (t != 1)
				sscanf(linebuf, " FILE %255s", tmpb);

			// absolute path?
			/* HACK: Assume always relative paths, needed for frontend */
			/*ti[numtracks + 1].handle = fopen(tmpb, "rb");
			if (ti[numtracks + 1].handle == NULL)*/ {
				// relative to .cue?
				tmp = strrchr(tmpb, '\\');
				if (tmp == NULL)
					tmp = strrchr(tmpb, '/');
				if (tmp != NULL)
					tmp++;
				else
					tmp = tmpb;
				strncpy(incue_fname, tmp, incue_max_len);
				ti[numtracks + 1].handle = fopen(filepath, "rb");
			}

			// update global offset if this is not first file in this .cue
			if (numtracks + 1 > 1) {
				multifile = 1;
				sector_offs += file_len;
			}

			file_len = 0;
			if (ti[numtracks + 1].handle == NULL) {
				printf(("\ncould not open: %s\n"), filepath);
				continue;
			}
			fseek(ti[numtracks + 1].handle, 0, SEEK_END);
			file_len = ftell(ti[numtracks + 1].handle) / CD_FRAMESIZE_RAW;

			if (numtracks == 0 && strlen(isofile) >= 4 &&
				strcmp(isofile + strlen(isofile) - 4, ".cue") == 0)
			{
				// user selected .cue as image file, use it's data track instead
				fclose(cdHandle);
				cdHandle = fopen(filepath, "rb");
			}
		}
	}

	if (numtracks <= 0) goto error;

	fclose(fi);
	return 0;

error:
	printf("\nError reading .CUE file %s\n", cuename);
	fclose(fi);
	return -1;
}

// this function tries to get the .ccd file of the given .img
// the necessary data is put into the ti (trackinformation)-array
static int parseccd(const char *isofile) {
	char			ccdname[MAXPATHLEN];
	FILE			*fi;
	char			linebuf[256];
	unsigned int	t;

	numtracks = 0;

	// copy name of the iso and change extension from .img to .ccd
	strncpy(ccdname, isofile, sizeof(ccdname));
	ccdname[MAXPATHLEN - 1] = '\0';
	if (strlen(ccdname) >= 4) {
		strcpy(ccdname + strlen(ccdname) - 4, ".ccd");
	} else {
		return -1;
	}

	if ((fi = fopen(ccdname, "r")) == NULL) {
		return -1;
	}

	memset(&ti, 0, sizeof(ti));

	while (fgets(linebuf, sizeof(linebuf), fi) != NULL) {
		if (!strncmp(linebuf, "[TRACK", 6)){
			numtracks++;
		}
		else if (!strncmp(linebuf, "MODE=", 5)) {
			sscanf(linebuf, "MODE=%d", &t);
			ti[numtracks].type = ((t == 0) ? CDDA : DATA);
		}
		else if (!strncmp(linebuf, "INDEX 1=", 8)) {
			sscanf(linebuf, "INDEX 1=%d", &t);
			sec2msf(t + 2 * 75, ti[numtracks].start);
			ti[numtracks].start_offset = t * CD_FRAMESIZE_RAW;

			// If we've already seen another track, this is its end
			if (numtracks > 1) {
				t = msf2sec(ti[numtracks].start) - msf2sec(ti[numtracks - 1].start);
				sec2msf(t, ti[numtracks - 1].length);
			}
		}
	}

	if (numtracks <= 0)
		goto error;

	// Fill out the last track's end based on size
	if (numtracks >= 1) {
		fseek(cdHandle, 0, SEEK_END);
		t = ftell(cdHandle) / CD_FRAMESIZE_RAW - msf2sec(ti[numtracks].start) + 2 * 75;
		sec2msf(t, ti[numtracks].length);
	}

	fclose(fi);
	return 0;

error:
	printf("\nError reading .CCD file %s\n", ccdname);
	fclose(fi);
	return -1;
}

//Alcohol 120% 'Media Descriptor Image' (.mds/.mdf) format support
// this function tries to get the .mds file of the given .mdf
// the necessary data is put into the ti (trackinformation)-array
static int parsemds(const char *isofile) {
	char      mdsname[MAXPATHLEN];
	FILE      *fi;
	uint32_t  offset, extra_offset, l, i;
	uint16_t  s;
	int       c;

	numtracks = 0;

	// copy name of the iso and change extension from .mdf to .mds
	strncpy(mdsname, isofile, sizeof(mdsname));
	mdsname[MAXPATHLEN - 1] = '\0';
	if (strlen(mdsname) >= 4) {
		strcpy(mdsname + strlen(mdsname) - 4, ".mds");
	} else {
		return -1;
	}

	if ((fi = fopen(mdsname, "rb")) == NULL)
		return -1;

	memset(&ti, 0, sizeof(ti));

	// check if it's a valid mds file
	if (fread(&i, 4, 1, fi) != 1)
		goto error;
	i = SWAP32(i);
	if (i != 0x4944454D) {
		// not an valid mds file
		printf("\nError: %s is not a valid .MDS file\n", mdsname);
		goto error;
	}

	// get offset to session block
	if (fseek(fi, 0x50, SEEK_SET) == -1 ||
	    fread(&offset, 4, 1, fi) != 1)
		goto error;
	offset = SWAP32(offset);

	// get total number of tracks
	offset += 14;
	if (fseek(fi, offset, SEEK_SET) == -1 ||
	    fread(&s, 2, 1, fi) != 1)
		goto error;
	s = SWAP16(s);
	numtracks = s;

	// get offset to track blocks
	if (fseek(fi, 4, SEEK_CUR) == -1 ||
	    fread(&offset, 4, 1, fi) != 1)
		goto error;
	offset = SWAP32(offset);

	// skip lead-in data
	while (1) {
		if (fseek(fi, offset + 4, SEEK_SET) == -1 ||
			(c = fgetc(fi)) == EOF)
			goto error;
		if (c < 0xA0)
			break;
		offset += 0x50;
	}

	// check if the image contains mixed subchannel data
	if (fseek(fi, offset + 1, SEEK_SET) == -1 ||
	    fgetc(fi) == EOF)
		goto error;
	subChanMixed = subChanRaw = (c ? TRUE : FALSE);

	// read track data
	for (i = 1; i <= numtracks; i++) {
		if (fseek(fi, offset, SEEK_SET) == -1)
			goto error;

		// get the track type
		if ((c = fgetc(fi)) == EOF)
			goto error;
		ti[i].type = (c == 0xA9) ? CDDA : DATA;
		fseek(fi, 8, SEEK_CUR);

		// get the track starting point
		for (int j = 0; j <= 2; ++j) {
			if ((c = fgetc(fi)) == EOF)
				goto error;
			ti[i].start[j] = c;
		}

		if (fread(&extra_offset, 4, 1, fi) != 1)
			goto error;
		extra_offset = SWAP32(extra_offset);

		// get track start offset (in .mdf)
		if (fseek(fi, offset + 0x28, SEEK_SET) == -1 ||
		    fread(&l, 4, 1, fi) != 1)
			goto error;
		l = SWAP32(l);
		ti[i].start_offset = l;

		// get pregap
		if (fseek(fi, extra_offset, SEEK_SET) == -1 ||
		    fread(&l, 4, 1, fi) != 1)
			goto error;
		l = SWAP32(l);
		if (l != 0 && i > 1)
			pregapOffset = msf2sec(ti[i].start);

		// get the track length
		if (fread(&l, 4, 1, fi) != 1)
			goto error;
		l = SWAP32(l);
		sec2msf(l, ti[i].length);

		offset += 0x50;
	}

	if (numtracks == 0) goto error;

	fclose(fi);
	return 0;

error:
	printf("\nError reading .MDS file %s\n", mdsname);
	fclose(fi);
	return -1;
}

static int handlepbp(const char *isofile) {
	struct {
		unsigned int sig;
		unsigned int dontcare[8];
		unsigned int psar_offs;
	} pbp_hdr;
	struct {
		unsigned char type;
		unsigned char pad0;
		unsigned char track;
		char index0[3];
		char pad1;
		char index1[3];
	} toc_entry;
	struct {
		unsigned int offset;
		unsigned int size;
		unsigned int dontcare[6];
	} index_entry;
	char psar_sig[11];
	off_t psisoimg_offs, cdimg_base;
	unsigned int t, cd_length;
	unsigned int offsettab[8];
	const char *ext = NULL;
	int i, ret;

	if (strlen(isofile) >= 4)
		ext = isofile + strlen(isofile) - 4;
	if (ext == NULL || (strcmp(ext, ".pbp") != 0 && strcmp(ext, ".PBP") != 0))
		return -1;

	fseeko(cdHandle, 0, SEEK_SET);

	numtracks = 0;

	ret = fread(&pbp_hdr, 1, sizeof(pbp_hdr), cdHandle);
	if (ret != sizeof(pbp_hdr)) {
		printf("failed to read pbp\n");
		goto fail_io;
	}

	ret = fseeko(cdHandle, pbp_hdr.psar_offs, SEEK_SET);
	if (ret != 0) {
		printf("failed to seek to %x\n", pbp_hdr.psar_offs);
		goto fail_io;
	}

	psisoimg_offs = pbp_hdr.psar_offs;
	ret = fread(psar_sig, 1, sizeof(psar_sig), cdHandle);
	if (ret != sizeof(psar_sig)) {
		printf("failed to read sig (1)\n");
		goto fail_io;
	}
	psar_sig[10] = 0;
	if (strcmp(psar_sig, "PSTITLEIMG") == 0) {
		// multidisk image?
		ret = fseeko(cdHandle, pbp_hdr.psar_offs + 0x200, SEEK_SET);
		if (ret != 0) {
			printf("failed to seek to %x\n", pbp_hdr.psar_offs + 0x200);
			goto fail_io;
		}

		if (fread(&offsettab, 1, sizeof(offsettab), cdHandle) != sizeof(offsettab)) {
			printf("failed to read offsettab\n");
			goto fail_io;
		}

		for (i = 0; i < sizeof(offsettab) / sizeof(offsettab[0]); i++) {
			if (offsettab[i] == 0)
				break;
		}
		cdrIsoMultidiskCount = i;
		if (cdrIsoMultidiskCount == 0) {
			printf("ERROR: multidisk eboot has 0 images?\n");
			goto fail_io;
		}

		//senquack - New feature allows GUI front end to register callback
		// to allow user to pick CD to boot from before proceeding:
		if (cdrIsoMultidiskCallback) cdrIsoMultidiskCallback();

		if (cdrIsoMultidiskSelect >= cdrIsoMultidiskCount)
			cdrIsoMultidiskSelect = 0;

		psisoimg_offs += offsettab[cdrIsoMultidiskSelect];

		ret = fseeko(cdHandle, psisoimg_offs, SEEK_SET);
		if (ret != 0) {
			printf("failed to seek to %llx\n", (long long)psisoimg_offs);
			goto fail_io;
		}

		ret = fread(psar_sig, 1, sizeof(psar_sig), cdHandle);
		if (ret != sizeof(psar_sig)) {
			printf("failed to read sig (2)\n");
			goto fail_io;
		}
		psar_sig[10] = 0;
	}

	if (strcmp(psar_sig, "PSISOIMG00") != 0) {
		printf("bad psar_sig: %s\n", psar_sig);
		goto fail_io;
	}

	// seek to TOC
	ret = fseeko(cdHandle, psisoimg_offs + 0x800, SEEK_SET);
	if (ret != 0) {
		printf("failed to seek to %llx\n", (long long)psisoimg_offs + 0x800);
		goto fail_io;
	}

	// first 3 entries are special
	if (fseek(cdHandle, sizeof(toc_entry), SEEK_CUR) == -1 ||
	    fread(&toc_entry, 1, sizeof(toc_entry), cdHandle) != sizeof(toc_entry)) {
		printf("failed reading first toc entry\n");
		goto fail_io;
	}

	numtracks = btoi(toc_entry.index1[0]);

	if (fread(&toc_entry, 1, sizeof(toc_entry), cdHandle) != sizeof(toc_entry)) {
		printf("failed reading second toc entry\n");
		goto fail_io;
	}

	cd_length = btoi(toc_entry.index1[0]) * 60 * 75 +
		btoi(toc_entry.index1[1]) * 75 + btoi(toc_entry.index1[2]);

	for (i = 1; i <= numtracks; i++) {
		if (fread(&toc_entry, 1, sizeof(toc_entry), cdHandle) != sizeof(toc_entry)) {
			printf("failed reading toc entry for track %d\n", i);
			goto fail_io;
		}

		ti[i].type = (toc_entry.type == 1) ? CDDA : DATA;

		ti[i].start_offset = btoi(toc_entry.index0[0]) * 60 * 75 +
			btoi(toc_entry.index0[1]) * 75 + btoi(toc_entry.index0[2]);
		ti[i].start_offset *= CD_FRAMESIZE_RAW;
		ti[i].start[0] = btoi(toc_entry.index1[0]);
		ti[i].start[1] = btoi(toc_entry.index1[1]);
		ti[i].start[2] = btoi(toc_entry.index1[2]);

		if (i > 1) {
			t = msf2sec(ti[i].start) - msf2sec(ti[i - 1].start);
			sec2msf(t, ti[i - 1].length);
		}
	}
	t = cd_length - ti[numtracks].start_offset / CD_FRAMESIZE_RAW;
	sec2msf(t, ti[numtracks].length);

	// seek to ISO index
	ret = fseeko(cdHandle, psisoimg_offs + 0x4000, SEEK_SET);
	if (ret != 0) {
		printf("failed to seek to ISO index\n");
		goto fail_io;
	}

	compr_img = (COMPR_IMG *)calloc(1, sizeof(*compr_img));
	if (compr_img == NULL)
		goto fail_io;

	compr_img->block_shift = 4;
	compr_img->current_block = (unsigned int)-1;

	compr_img->index_len = (0x100000 - 0x4000) / sizeof(index_entry);
	compr_img->index_table = (off_t *)malloc((compr_img->index_len + 1) *
				 sizeof(compr_img->index_table[0]));
	if (compr_img->index_table == NULL)
		goto fail_io;

	cdimg_base = psisoimg_offs + 0x100000;
	for (i = 0; i < compr_img->index_len; i++) {
		ret = fread(&index_entry, 1, sizeof(index_entry), cdHandle);
		if (ret != sizeof(index_entry)) {
			printf("failed to read index_entry #%d\n", i);
			goto fail_index;
		}

		if (index_entry.size == 0)
			break;

		compr_img->index_table[i] = cdimg_base + index_entry.offset;
	}
	compr_img->index_table[i] = cdimg_base + index_entry.offset + index_entry.size;

	return 0;

fail_index:
	free(compr_img->index_table);
	compr_img->index_table = NULL;
fail_io:
	if (compr_img != NULL) {
		free(compr_img);
		compr_img = NULL;
	}
	return -1;
}

static int handlecbin(const char *isofile) {
	struct
	{
		char magic[4];
		unsigned int header_size;
		unsigned long long total_bytes;
		unsigned int block_size;
		unsigned char ver;		// 1
		unsigned char align;
		unsigned char rsv_06[2];
	} ciso_hdr;
	const char *ext = NULL;
	unsigned int *index_table = NULL;
	unsigned int index = 0, plain;
	int i, ret;

	if (strlen(isofile) >= 5)
		ext = isofile + strlen(isofile) - 5;
	if (ext == NULL || (strcasecmp(ext + 1, ".cbn") != 0 && strcasecmp(ext, ".cbin") != 0))
		return -1;

	fseek(cdHandle, 0, SEEK_SET);

	ret = fread(&ciso_hdr, 1, sizeof(ciso_hdr), cdHandle);
	if (ret != sizeof(ciso_hdr)) {
		printf("failed to read ciso header\n");
		return -1;
	}

	if (strncmp(ciso_hdr.magic, "CISO", 4) != 0 || ciso_hdr.total_bytes <= 0 || ciso_hdr.block_size <= 0) {
		printf("bad ciso header\n");
		return -1;
	}
	if (ciso_hdr.header_size != 0 && ciso_hdr.header_size != sizeof(ciso_hdr)) {
		ret = fseeko(cdHandle, ciso_hdr.header_size, SEEK_SET);
		if (ret != 0) {
			printf("failed to seek to %x\n", ciso_hdr.header_size);
			return -1;
		}
	}

	compr_img = (COMPR_IMG *)calloc(1, sizeof(*compr_img));
	if (compr_img == NULL)
		goto fail_io;

	compr_img->block_shift = 0;
	compr_img->current_block = (unsigned int)-1;

	compr_img->index_len = ciso_hdr.total_bytes / ciso_hdr.block_size;
	index_table = (unsigned int *)malloc((compr_img->index_len + 1) * sizeof(index_table[0]));
	if (index_table == NULL)
		goto fail_io;

	ret = fread(index_table, sizeof(index_table[0]), compr_img->index_len, cdHandle);
	if (ret != compr_img->index_len) {
		printf("failed to read index table\n");
		goto fail_index;
	}

	compr_img->index_table = (off_t *)malloc((compr_img->index_len + 1) *
				 sizeof(compr_img->index_table[0]));
	if (compr_img->index_table == NULL)
		goto fail_index;

	for (i = 0; i < compr_img->index_len + 1; i++) {
		index = index_table[i];
		plain = index & 0x80000000;
		index &= 0x7fffffff;
		compr_img->index_table[i] = (off_t)index << ciso_hdr.align;
		if (plain)
			compr_img->index_table[i] |= OFF_T_MSB;
	}

	return 0;

fail_index:
	free(index_table);
fail_io:
	if (compr_img != NULL) {
		free(compr_img);
		compr_img = NULL;
	}
	return -1;
}

// this function tries to get the .sub file of the given .img
static int opensubfile(const char *isoname) {
	char		subname[MAXPATHLEN];

	// copy name of the iso and change extension from .img to .sub
	strncpy(subname, isoname, sizeof(subname));
	subname[MAXPATHLEN - 1] = '\0';
	if (strlen(subname) >= 4) {
		strcpy(subname + strlen(subname) - 4, ".sub");
	}
	else {
		return -1;
	}

	subHandle = fopen(subname, "rb");
	if (subHandle == NULL) {
		return -1;
	}

	return 0;
}

static int opensbifile(const char *isoname) {
	char		sbiname[MAXPATHLEN];
	int		s;

	strncpy(sbiname, isoname, sizeof(sbiname));
	sbiname[MAXPATHLEN - 1] = '\0';
	if (strlen(sbiname) >= 4) {
		strcpy(sbiname + strlen(sbiname) - 4, ".sbi");
	}
	else {
		return -1;
	}

	fseek(cdHandle, 0, SEEK_END);
	s = ftell(cdHandle) / CD_FRAMESIZE_RAW;

	return LoadSBI(sbiname, s);
}

static int cdread_normal(FILE *f, unsigned int base, void *dest, int sector)
{
	if (fseek(f, base + sector * CD_FRAMESIZE_RAW, SEEK_SET) == -1)
		return -1;
	return fread(dest, 1, CD_FRAMESIZE_RAW, f);
}

static int cdread_sub_mixed(FILE *f, unsigned int base, void *dest, int sector)
{
	int ret;

	if (fseek(f, base + sector * (CD_FRAMESIZE_RAW + SUB_FRAMESIZE), SEEK_SET) == -1)
		return -1;
	ret = fread(dest, 1, CD_FRAMESIZE_RAW, f);

	if (fread(subbuffer, 1, SUB_FRAMESIZE, f) != SUB_FRAMESIZE) {
		printf("Error reading mixed subchannel info in cdread_sub_mixed()\n");
	} else {
		if (subChanRaw) DecodeRawSubData();
	}

	return ret;
}

static int uncompress_pcsx(void *out, unsigned long *out_size, void *in, unsigned long in_size)
{
	static z_stream z;
	int ret = 0;

	if (z.zalloc == NULL) {
		// XXX: one-time leak here..
		z.next_in = Z_NULL;
		z.avail_in = 0;
		z.zalloc = Z_NULL;
		z.zfree = Z_NULL;
		z.opaque = Z_NULL;
		ret = inflateInit2(&z, -15);
	}
	else
		ret = inflateReset(&z);
	if (ret != Z_OK)
		return ret;

	z.next_in = (Bytef *)in;
	z.avail_in = in_size;
	z.next_out = (Bytef *)out;
	z.avail_out = *out_size;

	ret = inflate(&z, Z_NO_FLUSH);
	//inflateEnd(&z);

	*out_size -= z.avail_out;
	return ret == 1 ? 0 : ret;
}

static int cdread_compressed(FILE *f, unsigned int base, void *dest, int sector)
{
	unsigned long cdbuffer_size, cdbuffer_size_expect;
	unsigned int size;
	int is_compressed;
	off_t start_byte;
	int ret, block;

	if (base)
		sector += base / CD_FRAMESIZE_RAW;

	block = sector >> compr_img->block_shift;
	compr_img->sector_in_blk = sector & ((1 << compr_img->block_shift) - 1);

	if (block == compr_img->current_block) {
		//printf("hit sect %d\n", sector);
		goto finish;
	}

	if (sector >= compr_img->index_len * 16) {
		printf("sector %d is past img end\n", sector);
		return -1;
	}

	start_byte = compr_img->index_table[block] & ~OFF_T_MSB;
	if (fseeko(cdHandle, start_byte, SEEK_SET) != 0) {
		printf("seek error for block %d at %llx: ",
			block, (long long)start_byte);
		perror(NULL);
		return -1;
	}

	is_compressed = !(compr_img->index_table[block] & OFF_T_MSB);
	size = (compr_img->index_table[block + 1] & ~OFF_T_MSB) - start_byte;
	if (size > sizeof(compr_img->buff_compressed)) {
		printf("block %d is too large: %u\n", block, size);
		return -1;
	}

	if (fread(is_compressed ? compr_img->buff_compressed : compr_img->buff_raw[0],
				1, size, cdHandle) != size) {
		printf("read error for block %d at %llx: ", block, start_byte);
		perror(NULL);
		return -1;
	}

	if (is_compressed) {
		cdbuffer_size_expect = sizeof(compr_img->buff_raw[0]) << compr_img->block_shift;
		cdbuffer_size = cdbuffer_size_expect;
		ret = uncompress_pcsx(compr_img->buff_raw[0], &cdbuffer_size, compr_img->buff_compressed, size);
		if (ret != 0) {
			printf("uncompress failed with %d for block %d, sector %d\n",
					ret, block, sector);
			return -1;
		}
		if (cdbuffer_size != cdbuffer_size_expect)
			printf("cdbuffer_size: %lu != %lu, sector %d\n", cdbuffer_size,
					cdbuffer_size_expect, sector);
	}

	// done at last!
	compr_img->current_block = block;

finish:
	if (dest != cdbuffer) // copy avoid HACK
		memcpy(dest, compr_img->buff_raw[compr_img->sector_in_blk],
			CD_FRAMESIZE_RAW);
	return CD_FRAMESIZE_RAW;
}

static int cdread_2048(FILE *f, unsigned int base, void *dest, int sector)
{
	int ret;

	if (fseek(f, base + sector * 2048, SEEK_SET) == -1)
		return -1;

	ret = fread((char *)dest + 12 * 2, 1, 2048, f);

	// not really necessary, fake mode 2 header
	memset(cdbuffer, 0, 12 * 2);
	sec2msf(sector + 2 * 75, (char *)&cdbuffer[12]);
	cdbuffer[12 + 3] = 1;

	return ret;
}

static unsigned char *CDR_getBuffer_compr(void) {
	return compr_img->buff_raw[compr_img->sector_in_blk] + 12;
}

static unsigned char *CDR_getBuffer_norm(void) {
	return cdbuffer + 12;
}

static void PrintTracks(void) {
	int i;

	for (i = 1; i <= numtracks; i++) {
		printf(("Track %.2d (%s) - Start %.2d:%.2d:%.2d, Length %.2d:%.2d:%.2d\n"),
			i, (ti[i].type == DATA ? "DATA" : "AUDIO"),
			ti[i].start[0], ti[i].start[1], ti[i].start[2],
			ti[i].length[0], ti[i].length[1], ti[i].length[2]);
	}
}

// This function is invoked by the front-end when opening an CDR_
// file for playback
long CDR_open(void) {
	uint_fast8_t isMode1CDR_ = FALSE;
	char alt_bin_filename[MAXPATHLEN];
	const char *bin_filename;

	if (cdHandle != NULL) {
		return 0; // it's already open
	}

	cdHandle = fopen(GetIsoFile(), "rb");
	if (cdHandle == NULL) {
		printf(("Could't open '%s' for reading: %s\n"),
			GetIsoFile(), strerror(errno));
		return -1;
	}

	printf("Loaded CD Image: %s", GetIsoFile());

	cddaBigEndian = FALSE;
	subChanMixed = FALSE;
	subChanRaw = FALSE;
	pregapOffset = 0;
	cdrIsoMultidiskCount = 1;
	multifile = 0;

	CDR_getBuffer = CDR_getBuffer_norm;
	cdimg_read_func = cdread_normal;

	if (parsetoc(GetIsoFile()) == 0) {
		printf("[+toc]");
	}
	else if (parseccd(GetIsoFile()) == 0) {
		printf("[+ccd]");
	}
	else if (parsemds(GetIsoFile()) == 0) {
		printf("[+mds]");
	}
	else if (parsecue(GetIsoFile()) == 0) {
		printf("[+cue]");
	}
	if (handlepbp(GetIsoFile()) == 0) {
		printf("[pbp]");
		CDR_getBuffer = CDR_getBuffer_compr;
		cdimg_read_func = cdread_compressed;
	}
	else if (handlecbin(GetIsoFile()) == 0) {
		printf("[cbin]");
		CDR_getBuffer = CDR_getBuffer_compr;
		cdimg_read_func = cdread_compressed;
	}

	if (!subChanMixed && opensubfile(GetIsoFile()) == 0) {
		printf("[+sub]");
	}
	if (opensbifile(GetIsoFile()) == 0) {
		printf("[+sbi]");
	}
	
	fseeko(cdHandle, 0, SEEK_END);
	off_t file_len = ftello(cdHandle);
	if (numtracks < 1) {
		// set a default track, to satisfy bios read
		unsigned int t;

		memset(&ti, 0, sizeof(ti));
		numtracks = 1;
		ti[numtracks].handle = fopen(GetIsoFile(), "rb");
		ti[numtracks].type = DATA;
		memset(ti[numtracks].start, 0, 3);

		t = msf2sec(ti[numtracks].start);
		ti[numtracks].start_offset = t * CD_FRAMESIZE_RAW;
		t += 2 * 75;
		sec2msf(t, ti[numtracks].start);

		// default track length to file length
		t = (file_len - ti[numtracks].start_offset) / CD_FRAMESIZE_RAW;
		sec2msf(t, ti[numtracks].length);
	}

	// maybe user selected metadata file instead of main .bin ..
	bin_filename = GetIsoFile();
	if (file_len < CD_FRAMESIZE_RAW * 0x10) {
		static const char *exts[] = { ".bin", ".BIN", ".img", ".IMG" };
		FILE *tmpf = NULL;
		size_t i;
		char *p;

		strncpy(alt_bin_filename, bin_filename, sizeof(alt_bin_filename));
		alt_bin_filename[MAXPATHLEN - 1] = '\0';
		if (strlen(alt_bin_filename) >= 4) {
			p = alt_bin_filename + strlen(alt_bin_filename) - 4;
			for (i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
				strcpy(p, exts[i]);
				tmpf = fopen(alt_bin_filename, "rb");
				if (tmpf != NULL)
					break;
			}
		}
		if (tmpf != NULL) {
			bin_filename = alt_bin_filename;
			fclose(cdHandle);
			cdHandle = tmpf;
			fseeko(cdHandle, 0, SEEK_END);
		}
	}

	// guess whether it is mode1/2048
	if (ftello(cdHandle) % 2048 == 0) {
		unsigned int modeTest = 0;
		fseek(cdHandle, 0, SEEK_SET);
		if (fread(&modeTest, 4, 1, cdHandle) == 1) {
			if (SWAP32(modeTest) != 0xffffff00) {
				printf("[2048]");
				isMode1CDR_ = TRUE;
			}
		}
	}
	fseek(cdHandle, 0, SEEK_SET);

	printf(".\n");

	if (cdrIsoMultidiskCount > 1)
		printf("Loading multi-CD image %d of %d.\n", cdrIsoMultidiskSelect+1, cdrIsoMultidiskCount);

	PrintTracks();

	if (subChanMixed)
		cdimg_read_func = cdread_sub_mixed;
	else if (isMode1CDR_)
		cdimg_read_func = cdread_2048;

	// make sure we have another handle open for cdda
	if (numtracks > 1 && ti[1].handle == NULL) {
		ti[1].handle = fopen(bin_filename, "rb");
	}
	cdda_cur_sector = 0;
	cdda_file_offset = 0;

	return 0;
}

long CDR_close(void) {
	int i;

	if (cdHandle != NULL) {
		fclose(cdHandle);
		cdHandle = NULL;
	}
	if (subHandle != NULL) {
		fclose(subHandle);
		subHandle = NULL;
	}
	stopCDDA();
	cddaHandle = NULL;

	if (compr_img != NULL) {
		free(compr_img->index_table);
		free(compr_img);
		compr_img = NULL;
	}

	for (i = 1; i <= numtracks; i++) {
		if (ti[i].handle != NULL) {
			fclose(ti[i].handle);
			ti[i].handle = NULL;
		}
	}
	numtracks = 0;
	ti[1].type = (cd_type)0;
	UnloadSBI();
	memset(cdbuffer, 0, sizeof(cdbuffer));
	CDR_getBuffer = CDR_getBuffer_norm;

	return 0;
}

long CDR_init(void) {
	numtracks = 0;
	assert(cdHandle == NULL);
	assert(subHandle == NULL);

	return 0; // do nothing
}

long CDR_shutdown(void) {
	CDR_close();

	//senquack - Added:
	FreePPFCache();

	return 0;
}

// return Starting and Ending Track
// buffer:
//  byte 0 - start track
//  byte 1 - end track
long CDR_getTN(unsigned char *buffer) {
	buffer[0] = 1;

	if (numtracks > 0) {
		buffer[1] = numtracks;
	}
	else {
		buffer[1] = 1;
	}

	return 0;
}

// return Track Time
// buffer:
//  byte 0 - frame
//  byte 1 - second
//  byte 2 - minute
long CDR_getTD(unsigned char track, unsigned char *buffer) {
	if (track == 0) {
		unsigned int sect;
		unsigned char time[3];
		sect = msf2sec(ti[numtracks].start) + msf2sec(ti[numtracks].length);
		sec2msf(sect, (char *)time);
		buffer[2] = time[0];
		buffer[1] = time[1];
		buffer[0] = time[2];
	}
	else if (numtracks > 0 && track <= numtracks) {
		buffer[2] = ti[track].start[0];
		buffer[1] = ti[track].start[1];
		buffer[0] = ti[track].start[2];
	}
	else {
		buffer[2] = 0;
		buffer[1] = 2;
		buffer[0] = 0;
	}

	return 0;
}

// decode 'raw' subchannel data ripped by cdrdao
static void DecodeRawSubData(void) {
	unsigned char subQData[12];
	int i;

	memset(subQData, 0, sizeof(subQData));

	for (i = 0; i < 8 * 12; i++) {
		if (subbuffer[i] & (1 << 6)) { // only subchannel Q is needed
			subQData[i >> 3] |= (1 << (7 - (i & 7)));
		}
	}

	memcpy(&subbuffer[12], subQData, 12);
}

// read track
// time: byte 0 - minute; byte 1 - second; byte 2 - frame
// uses bcd format
long CDR_readTrack(unsigned char *time) {
	int sector = MSF2SECT(btoi(time[0]), btoi(time[1]), btoi(time[2]));
	long ret;

	if (cdHandle == NULL) {
		return -1;
	}

	if (pregapOffset) {
		subChanMissing = FALSE;
		if (sector >= pregapOffset) {
			sector -= 2 * 75;
			if (sector < pregapOffset)
				subChanMissing = TRUE;
		}
	}

	ret = cdimg_read_func(cdHandle, 0, cdbuffer, sector);
	if (ret < 0)
		return -1;

	if (subHandle != NULL) {
		if (fseek(subHandle, sector * SUB_FRAMESIZE, SEEK_SET) != -1 &&
		    fread(subbuffer, 1, SUB_FRAMESIZE, subHandle) == SUB_FRAMESIZE) {
			if (subChanRaw) DecodeRawSubData();
		} else {
			printf("Error reading subchannel info in CDR_readTrack()\n");
		}
	}

	return 0;
}

// plays cdda audio
// sector: byte 0 - minute; byte 1 - second; byte 2 - frame
// does NOT uses bcd format
long CDR_play(unsigned char *time) {
	unsigned int i;

	if (numtracks <= 1)
		return 0;

	// find the track
	cdda_cur_sector = msf2sec((char *)time);
	for (i = numtracks; i > 1; i--) {
		cdda_first_sector = msf2sec(ti[i].start);
		if (cdda_first_sector <= cdda_cur_sector + 2 * 75)
			break;
	}
	cdda_file_offset = ti[i].start_offset;

	// find the file that contains this track
	for (; i > 1; i--)
		if (ti[i].handle != NULL)
			break;

	cddaHandle = ti[i].handle;

	// Uncomment when SPU_playCDDAchannel is a func ptr again
	//if (SPU_playCDDAchannel != NULL)
		startCDDA();

	return 0;
}

// stops cdda audio
long CDR_stop(void) {
	stopCDDA();
	return 0;
}

// gets subchannel data
unsigned char* CDR_getBufferSub(void) {
	if ((subHandle != NULL || subChanMixed) && !subChanMissing) {
		return subbuffer;
	}

	return NULL;
}

long CDR_getStatus(struct CdrStat *stat) {
	uint32_t sect;

	//senquack - PCSX Rearmed cdriso.c code has this if/else cdOpenCaseTime logic
	// abstracted through a call here to a separate function CDR__getStatus() in
	// its plugins.c, but since we don't support multiple CD plugins, just do
	// the logic directly here. (This handles CD swapping status bit)
	//CDR__getStatus(stat);  // <<<< This function is essentially this vvvvv
	if (cdOpenCaseTime < 0 || cdOpenCaseTime > (int64_t)time(NULL))
		stat->Status = 0x10;
	else
		stat->Status = 0;


	if (playing) {
		stat->Type = 0x02;
		stat->Status |= 0x80;
	}
	else {
		// BIOS - boot ID (CD type)
		stat->Type = ti[1].type;
	}

	// relative -> absolute time
	sect = cddaCurPos;
	sec2msf(sect, (char *)stat->Time);

	return 0;
}

// read CDDA sector into buffer
long CDR_readCDDA(unsigned char m, unsigned char s, unsigned char f, unsigned char *buffer) {
	unsigned char msf[3] = {m, s, f};
	unsigned int file, track, track_start = 0;
	int ret;

	cddaCurPos = msf2sec((char *)msf);

	// find current track index
	for (track = numtracks; ; track--) {
		track_start = msf2sec(ti[track].start);
		if (track_start <= cddaCurPos)
			break;
		if (track == 1)
			break;
	}

	// data tracks play silent
	if (ti[track].type != CDDA) {
		memset(buffer, 0, CD_FRAMESIZE_RAW);
		return 0;
	}

	file = 1;
	if (multifile) {
		// find the file that contains this track
		for (file = track; file > 1; file--)
			if (ti[file].handle != NULL)
				break;
	}

	ret = cdimg_read_func(ti[file].handle, ti[track].start_offset,
		buffer, cddaCurPos - track_start);
	if (ret != CD_FRAMESIZE_RAW) {
		memset(buffer, 0, CD_FRAMESIZE_RAW);
		return -1;
	}

	if (cddaBigEndian) {
		int i;
		unsigned char tmp;

		for (i = 0; i < CD_FRAMESIZE_RAW / 2; i++) {
			tmp = buffer[i * 2];
			buffer[i * 2] = buffer[i * 2 + 1];
			buffer[i * 2 + 1] = tmp;
		}
	}

	return 0;
}

void cdrIsoInit(void) {
	numtracks = 0;
}

int cdrIsoActive(void) {
	return (cdHandle != NULL);
}