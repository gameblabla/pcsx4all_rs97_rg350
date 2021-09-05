#include <stdint.h>
#include <string.h>
#include "port.h"
#include "r3000a.h"
#include "plugins.h"
#include "plugin_lib.h"
#include "perfmon.h"

#ifdef GPU_UNAI
  #include "gpu/gpu_unai/gpu.h"
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

uint_fast8_t ishack_enabled = 0;
uint_fast8_t default_analog = 0;

static const char CNTfix_table[25][10] =
{
	/* Vandal Hearts */
	{"SCPS45183"},
	{"SCPS45183"},
	{"SLES00204"},
	{"SLUS00447"},
	/* Vandal Hearts II */
	{"SLES02469"},
	{"SLES02497"},
	{"SLES02496"},
	{"SLUS00940"},
	{"SLPM86251"},
	{"SLPM86007"},
	/* Parasite Eve II */
	{"SLES02561"},
	{"SLES12562"},
	{"SLES02562"},
	{"SLES12560"},
	{"SLES02560"},
	{"SLES12559"},
	{"SLES02559"},
	{"SLES12558"},
	{"SLES02558"},
	{"SLUS01042"},
	{"SLUS01055"},
	{"SCPS45467"},
	{"SLPS02480"},
	{"SLPS91479"},
	{"SLPS02779"},
};

static const char MemorycardHack[8][10] =
{
	/* Lifeforce Tenka, also known as Codename Tenka */
	{"SLES00613"},
	{"SLED00690"},
	{"SLES00614"},
	{"SLES00615"},
	{"SLES00616"},
	{"SLES00617"},
	{"SCUS94409"}
};

static const char DualShockOnlyGames[16][10] =
{
	/* Ape Escape */
	{"SCES01564"},
	{"SCES02031"},
	{"SCES02028"},
	{"SCES02029"},
	{"SCES02030"},
	{"SCPS45411"},
	{"SCPS10091"},
	{"SCPS91196"},
	{"SCPS91331"},
	{"SCUS94423"},
	
	/* Resident Evil 2 : Dual shock edition */
	{"SLPS01510"},
	{"SLUS00748"},
	{"SLUS00756"},
	
	/* Resident Evil : Director's Cut */
	{"SLUS00747"},
	{"SLPS01512"}
};

/* Flightstick/Dual Analog games */
static const char DualAnalogGames[93][10] =
{
	/* Ace Combat 2 */
	{"SLUS00404"},
	{"SLPS00830"},
	{"SCES00699"},
	{"SCES00901"},
	{"SCES00902"},
	
	/* Armored Trooper Votoms */
	{"SLPS01330"},
	{"SLPS01331"},
	
	/* Cyberia */
	{"SLES00233"},
	{"SLPS00218"},
	{"SLUS00053"},
	{"SLES00272"},
	
	/* Descent */
	{"SLUS00037"},
	{"SLPS00212"},
	{"SLES00055"},
	
	/* Descent Maximum */
	{"SLUS00460"},
	{"SLES00558"},
	
	/* Digital Glider Airman */
	{"SLPS02276"},
	{"SLPS91486"},
	
	/* Elemental Gearbolt */
	{"SLUS00654"},
	{"SCPS10038"},
	
	/* EOS Edge of Skyhigh */
	{"SLPS00820"},
	
	/* Formula 1 97 */ 
	{"SLES00859"},
	{"SIPS60023"},
	{"SLUS00546"},

	/* Galaxian 3 */ 
	{"SCES00269"},
	{"SLPS00270"},
	
	/* Gunship */ 
	{"SLUS00313"},
	{"SLPS00495"},
	{"SLES00027"},
	
	/* Independance Day */ 
	{"SLUS00221"},
	{"SLES00607"},
	
	/* Macross Digital Mission VF-X */ 
	{"SLPS00386"},
	{"SCPS45021"},
	{"SLPS91058"},
	
	/* Macross VF-X 2 */ 
	{"SLPS02237"},
	{"SCPS45427"},

	/* MDK */ 
	{"SLES00599"},
	{"SLUS00426"},
	{"SCPS10052"},
	
	/* MechWarrior 2 */ 
	{"SLPS00937"},
	{"SLUS00401"},
	{"SLES00340"},
	{"SLES00374"},
	{"SLES00375"},
	
	/* Arcade's Greatest Hits - The Midway Collection 2 */ 
	{"SLUS00450"},
	{"SLES00739"},

	/* Missile Command */ 
	{"SLUS00992"},
	{"SLES02245"},
	{"SLES02482"},
	
	/* Namco Museum Vol.4 */ 
	{"SLUS00416"},
	{"SLPS00540"},
	{"SLPS91161"},
	{"SCES00701"},
	
	/* Newman Haas Racing */ 
	{"SLUS00602"},
	{"SLES00933"},

	/* Rise 2 - Resurrection */ 
	{"SLES00164"},
	{"SLPS00259"},
	{"SLUS00186"},
	
	/* Shadow Master */ 
	{"SLUS00545"},
	{"SLES00888"},

	/* SideWinder II */ 
	{"SLPS91132"},
	{"SCPS45137"},
	{"SLPS00954"},
	
	/* SlamScape */ 
	{"SLUS00080"},
	{"SLES00427"},
	
	/* Steel Reign */ 
	{"SCUS94902"},
	{"SCES01023"},
	
	/* Treasures of the Deep */
	{"SLUS00430"},
	{"SCES00850"},
	
	{"SCES01070"},
	{"SCES01071"},
	{"SCES01072"},
	{"SCES01073"},
	
	/* Vehicule Cavalier */ 
	{"SLPS00232"},
	
	/* Wing Commander IV - The Price of Freedom */
	{"SLUS00270"},
	
	{"SLES00659"},
	{"SLES10659"},
	{"SLES20659"},
	{"SLES30659"},
	
	{"SLES00660"},
	{"SLES10660"},
	{"SLES20660"},
	{"SLES30660"},
	
	{"SLES00661"},
	{"SLES10661"},
	{"SLES20661"},
	{"SLES30661"},
	
	/* Wing Over 2 */
	{"SLES01375"},
	{"SLPS01600"},
	{"SLPS91165"},
	{"SLPS91498"},

	/* Zero Pilot */
	{"SCPS91127"},
	{"SCPS10049"}
};


/* Function for automatic patching according to GameID.
 * It's possible that some of these games have no IDs, like some japanese games i encountered.
 * I need to check whenever this matters or not for our games.
 * (Plus it can still be activated in the menu)
 * Let's hope the IDs are also not shared with other games ! (Homebrew, don't screw it up)
 * */
void CheckforCDROMid_applyhacks()
{
	uint32_t i;
	
	ishack_enabled = 0;
	/* Force DualShock mode for some games (Ape Escape, RE Dual shock edition) */
	for(i=0;i<ARRAY_SIZE(DualShockOnlyGames);i++)
	{
		if (strncmp(CdromId, DualShockOnlyGames[i], 9) == 0)
		{
			Config.AnalogMode = 2;
			ishack_enabled = 1;
		}
	}
	
	/* Don't enable this for consoles like the RS-97 that only has a D-PAD */
	#if !defined(NOJOYSTICK_AVAILABLE)
	/* Force Flightstick/DualAnalog mode on games that don't support the DualShock */
	for(i=0;i<ARRAY_SIZE(DualAnalogGames);i++)
	{
		if (strncmp(CdromId, DualAnalogGames[i], 9) == 0)
		{
			Config.AnalogMode = 1;
		}
	}
	#endif
	
	/* Apply Memory card hack for Codename Tenka for going past the screen asking to remove MC */
	for(i=0;i<ARRAY_SIZE(MemorycardHack);i++)
	{
		if (strncmp(CdromId, MemorycardHack[i], 9) == 0)
		{
			//Config.MemoryCardHack = 1;
		}
	}
	
	/* Apply hackfix for Parasite Eve 2, Vandal Hearts I/II */
	for(i=0;i<ARRAY_SIZE(CNTfix_table);i++)
	{
		if (strncmp(CdromId, CNTfix_table[i], 9) == 0)
		{
			Config.RCntFix = 1;
			ishack_enabled = 1;
		}
	}
}
