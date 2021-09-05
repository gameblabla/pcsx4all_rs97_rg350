#ifndef __PSXPORT_H__
#define __PSXPORT_H__

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <assert.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN 256
#endif

extern char cheatsdir[MAXPATHLEN];

struct ps1_controller {
	uint8_t id;
	uint8_t joy_right_ax0;
	uint8_t joy_right_ax1;
	uint8_t joy_left_ax0;
	uint8_t joy_left_ax1;
	uint8_t Vib[2];
	uint8_t VibF[2];
	uint8_t pad_mode;
	uint8_t pad_controllertype;
	uint8_t configmode;
};

extern struct ps1_controller player_controller[2];

///////////////////////////
// Windows compatibility //
///////////////////////////
#if defined(_WIN32) && !defined(__CYGWIN__)
// Windows lacks fsync():
static inline int fsync(int f) { return 0; }
#endif

#define	CONFIG_VERSION	0

INLINE int string_is_empty(const char *data)
{
	return (!data || (*data == '\0')) ? 1 : 0;
}

unsigned get_ticks(void);
void wait_ticks(unsigned s);
void pad_update(void);
unsigned short pad_read(int num);

void video_flip(void);
#ifdef GPU_DFXVIDEO
void video_set(unsigned short* pVideo,unsigned int width,unsigned int height);
#endif
void video_clear(void);
void port_printf(int x, int y, const char *text);

extern unsigned short *SCREEN;
extern int SCREEN_WIDTH, SCREEN_HEIGHT;

char *GetMemcardPath(int slot);

int state_load(int slot);
int state_save(int slot);

void update_memcards(int load_mcd);
const char *bios_file_get();
void check_spec_bios();

void Rumble_Init();
int trigger_rumble(uint8_t low, uint8_t high);
int set_rumble_gain(unsigned gain);

int SelectGame();
int GameMenu();

#endif
