/*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*

For the NP08 practical, we have customized the picoscope 5000a example program, and 
have kept the structure the same as the picoscope version where possible.  This uses the
'simple' C trick of defining the functions starting with the lower ones, so the main()
program is at the end.  We have added the NP08 routines as close to the end as possible,
i.e. just above the main() program.   So it is best to start looking from the bottom.

The program is structured around the menu the user sees.  For each menu selection the
user has, there is a subroutine.  The original program had just one menu, but we divided
it to collect the most common items for the NP08 practical in one place.

*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*OX*/

/*******************************************************************************
 *
 * Filename: ps5000aCon.c
 *
 * Description:
 *   This is a console mode program that demonstrates how to use some of 
 *	 the PicoScope 5000 Series (ps5000a) driver API functions to perform operations
 *	 using a PicoScope 5000 Series Flexible Resolution Oscilloscope.
 *
 *	Supported PicoScope models:
 *
 *		PicoScope 5242A/B/D & 5442A/B/D
 *		PicoScope 5243A/B/D & 5443A/B/D
 *		PicoScope 5244A/B/D & 5444A/B/D
 *
 * Examples:
 *   Collect a block of samples immediately
 *   Collect a block of samples when a trigger event occurs
 *	 Collect a block of samples using Equivalent Time Sampling (ETS)
 *   Collect samples using a rapid block capture with trigger
 *   Collect a stream of data immediately
 *   Collect a stream of data when a trigger event occurs
 *   Set Signal Generator, using standard or custom signals
 *   Change timebase & voltage scales
 *   Display data in mV or ADC counts
 *	 Handle power source changes
 *
 *	To build this application:-
 *
 *		If Microsoft Visual Studio (including Express/Community Edition) is being used:
 *
 *			Select the solution configuration (Debug/Release) and platform (x86/x64)
 *			Ensure that the 32-/64-bit ps5000a.lib can be located
 *			Ensure that the ps5000aApi.h and PicoStatus.h files can be located
 *
 *		Otherwise:
 *
 *			 Set up a project for a 32-/64-bit console mode application
 *			 Add this file to the project
 *			 Add ps5000a.lib to the project (Microsoft C only)
 *			 Add ps5000aApi.h and PicoStatus.h to the project
 *			 Build the project
 *
 *  Linux platforms:
 *
 *		Ensure that the libps5000a driver package has been installed using the
 *		instructions from https://www.picotech.com/downloads/linux
 *
 *		Place this file in the same folder as the files from the linux-build-files
 *		folder. In a terminal window, use the following commands to build
 *		the ps5000aCon application:
 *
 *			./autogen.sh <ENTER>
 *			make <ENTER>
 *
 * Copyright (C) 2013-2018 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>

/* Headers for Windows */
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
#include "ps5000aApi.h"
#include <time.h>
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <libps5000a-1.1/ps5000aApi.h>
#ifndef PICO_STATUS
#include <libps5000a-1.1/PicoStatus.h>
#endif

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)

typedef enum enBOOL{FALSE,TRUE} BOOL;

/* A function to detect a keyboard press on Linux */
int32_t _getch()
{
  struct termios oldt, newt;
  int32_t ch;
  int32_t bytesWaiting;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~( ICANON | ECHO );
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  setbuf(stdin, NULL);
  do {
    ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);
    if (bytesWaiting)
      getchar();
  } while (bytesWaiting);
  
  ch = getchar();
  
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return ch;
}

int32_t _kbhit()
{
  struct termios oldt, newt;
  int32_t bytesWaiting;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~( ICANON | ECHO );
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  setbuf(stdin, NULL);
  ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);
  
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return bytesWaiting;
}

int32_t fopen_s(FILE ** a, const int8_t * b, const int8_t * c)
{
  FILE * fp = fopen(b,c);
  *a = fp;
  return (fp>0)?0:-1;
}

/* A function to get a single character on Linux */
#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)
#endif

int32_t cycles = 0;

#define BUFFER_SIZE 	1024

#define QUAD_SCOPE		4
#define DUAL_SCOPE		2

#define MAX_PICO_DEVICES 64
#define TIMED_LOOP_STEP 500

typedef struct
{
  int16_t DCcoupled;
  int16_t range;
  int16_t enabled;
  float analogueOffset;
} CHANNEL_SETTINGS;

typedef enum
{
  MODEL_NONE = 0,
  MODEL_PS5242A = 0xA242,
  MODEL_PS5242B = 0xB242,
  MODEL_PS5243A = 0xA243,
  MODEL_PS5243B = 0xB243,
  MODEL_PS5244A = 0xA244,
  MODEL_PS5244B = 0xB244,
  MODEL_PS5442A = 0xA442,
  MODEL_PS5442B = 0xB442,
  MODEL_PS5443A = 0xA443,
  MODEL_PS5443B = 0xB443,
  MODEL_PS5444A = 0xA444,
  MODEL_PS5444B = 0xB444
} MODEL_TYPE;

typedef enum
{
  SIGGEN_NONE = 0,
  SIGGEN_FUNCTGEN = 1,
  SIGGEN_AWG = 2
} SIGGEN_TYPE;

typedef struct tPwq
{
  PS5000A_CONDITION * pwqConditions;
  int16_t nPwqConditions;
  PS5000A_DIRECTION * pwqDirections;
  int16_t nPwqDirections;
  uint32_t lower;
  uint32_t upper;
  PS5000A_PULSE_WIDTH_TYPE type;
}PWQ;

typedef struct
{
  int16_t       handle;
  MODEL_TYPE    model;
  int8_t	modelString[8];
  int8_t	serial[10];
  int16_t	complete;
  int16_t	openStatus;
  int16_t	openProgress;
  PS5000A_RANGE	firstRange;
  PS5000A_RANGE	lastRange;
  int16_t	channelCount;
  int16_t	maxADCValue;
  SIGGEN_TYPE	sigGen;
  int16_t	hasHardwareETS;
  uint16_t	awgBufferSize;
  CHANNEL_SETTINGS channelSettings [PS5000A_MAX_CHANNELS];
  PS5000A_DEVICE_RESOLUTION resolution;
  int16_t	digitalPortCount;
}UNIT;

uint32_t timebase = 8;
BOOL scaleVoltages = TRUE;

uint16_t inputRanges [PS5000A_MAX_RANGES] = {
  10,
  20,
  50,
  100,
  200,
  500,
  1000,
  2000,
  5000,
  10000,
  20000,
  50000};

int16_t			g_autoStopped;
int16_t   	g_ready = FALSE;
uint64_t 		g_times [PS5000A_MAX_CHANNELS];
int16_t     g_timeUnit;
int32_t     g_sampleCount;
uint32_t		g_startIndex;
int16_t			g_trig = 0;
uint32_t		g_trigAt = 0;
int16_t			g_overflow = 0;

int8_t blockFile[20]  = "block.txt";
int8_t streamFile[20] = "stream.txt";

typedef struct tBufferInfo
{
  UNIT * unit;
  int16_t **driverBuffers;
  int16_t **appBuffers;
} BUFFER_INFO;

/****************************************************************************
* callbackStreaming
* Used by ps5000a data streaming collection calls, on receipt of data.
* Used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 callBackStreaming(	int16_t handle,
	int32_t noOfSamples,
	uint32_t startIndex,
	int16_t overflow,
	uint32_t triggerAt,
	int16_t triggered,
	int16_t autoStop,
	void	*pParameter)
{
  int32_t channel;
  BUFFER_INFO * bufferInfo = NULL;
  
  if (pParameter != NULL)
    {
      bufferInfo = (BUFFER_INFO *) pParameter;
    }
  
  // used for streaming
  g_sampleCount = noOfSamples;
  g_startIndex  = startIndex;
  g_autoStopped = autoStop;
  
  // flag to say done reading data
  g_ready = TRUE;
  
  // flags to show if & where a trigger has occurred
  g_trig = triggered;
  g_trigAt = triggerAt;
  
  g_overflow = overflow;
  
  if (bufferInfo != NULL && noOfSamples) {
    for (channel = 0; channel < bufferInfo->unit->channelCount; channel++) {
      if (bufferInfo->unit->channelSettings[channel].enabled) {
	if (bufferInfo->appBuffers && bufferInfo->driverBuffers) {
	  // Max buffers
	  if (bufferInfo->appBuffers[channel * 2]  && bufferInfo->driverBuffers[channel * 2]) {
	    memcpy_s (&bufferInfo->appBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t),
		      &bufferInfo->driverBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t));
	  }
		  
	  // Min buffers
	  if (bufferInfo->appBuffers[channel * 2 + 1] && bufferInfo->driverBuffers[channel * 2 + 1]) {
	    memcpy_s (&bufferInfo->appBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t),
		      &bufferInfo->driverBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t));
	  }
	}
      }
    }
  }
}

/****************************************************************************
* Callback
* used by ps5000a data block collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 callBackBlock( int16_t handle, PICO_STATUS status, void * pParameter)
{
  if (status != PICO_CANCELLED) {
    g_ready = TRUE;
  }
}

//DB Get current computer (not picoscope) time in microsecond resolution (not accuracy)
int64_t GetTime_MicroSecond() {
	struct timespec now;
	timespec_get(&now, TIME_UTC);
	return ((int64_t)now.tv_sec) * 1000000 + ((int64_t)now.tv_nsec) / 1000;
}

/****************************************************************************
* SetDefaults - restore default settings
****************************************************************************/
void setDefaults(UNIT * unit)
{
  PICO_STATUS status;
  PICO_STATUS powerStatus;
  int32_t i;
  
  status = ps5000aSetEts(unit->handle, PS5000A_ETS_OFF, 0, 0, NULL);					// Turn off hasHardwareETS
  if (status) printf("setDefaults:ps5000aSetEts------ 0x%08lx \n", status);

  powerStatus = ps5000aCurrentPowerSource(unit->handle);
  
  for (i = 0; i < unit->channelCount; i++) { // reset channels to most recent settings 
    if(i >= DUAL_SCOPE && powerStatus == PICO_POWER_SUPPLY_NOT_CONNECTED) {
      // No need to set the channels C and D if Quad channel scope and power not enabled.
    } else {
      status = ps5000aSetChannel(unit->handle, (PS5000A_CHANNEL)(PS5000A_CHANNEL_A + i),
				 unit->channelSettings[PS5000A_CHANNEL_A + i].enabled,
				 (PS5000A_COUPLING)unit->channelSettings[PS5000A_CHANNEL_A + i].DCcoupled,
				 (PS5000A_RANGE)unit->channelSettings[PS5000A_CHANNEL_A + i].range, 
				 unit->channelSettings[PS5000A_CHANNEL_A + i].analogueOffset);

      if (status) printf("SetDefaults:ps5000aSetChannel------ 0x%08lx \n", status);
      
    }
  }
}

/****************************************************************************
* adc_to_mv
*
* Convert an 16-bit ADC count into millivolts
****************************************************************************/
int32_t adc_to_mv(int32_t raw, int32_t rangeIndex, UNIT * unit)
{
  return (raw * inputRanges[rangeIndex]) / unit->maxADCValue;
}

/****************************************************************************
* mv_to_adc
*
* Convert a millivolt value into a 16-bit ADC count
*
*  (useful for setting trigger thresholds)
****************************************************************************/
int16_t mv_to_adc(int16_t mv, int16_t rangeIndex, UNIT * unit)
{
  return (mv * unit->maxADCValue) / inputRanges[rangeIndex];
}

/****************************************************************************************
* ChangePowerSource - function to handle switches between +5V supply, and USB only power
* Only applies to PicoScope 544xA/B units 
******************************************************************************************/
PICO_STATUS changePowerSource(int16_t handle, PICO_STATUS status, UNIT * unit)
{
  int8_t ch;
  
  switch (status) {
  case PICO_POWER_SUPPLY_NOT_CONNECTED:		// User must acknowledge they want to power via USB
    do {
      printf("\n5 V power supply not connected.");
      printf("\nDo you want to run using USB only Y/N?\n");
      ch = toupper(_getch());
      
      if(ch == 'Y') {
	printf("\nPowering the unit via USB\n");
	status = ps5000aChangePowerSource(handle, PICO_POWER_SUPPLY_NOT_CONNECTED);		// Tell the driver that's ok
				
	if(status == PICO_OK && unit->channelCount == QUAD_SCOPE) {
	  unit->channelSettings[PS5000A_CHANNEL_C].enabled = FALSE;
	  unit->channelSettings[PS5000A_CHANNEL_D].enabled = FALSE;
	}
	else if (status == PICO_POWER_SUPPLY_UNDERVOLTAGE) {
	  status = changePowerSource(handle, status, unit);
	} else {} // Do nothing
      }
    } while(ch != 'Y' && ch != 'N');
    printf(ch == 'N'?"Please use the +5V power supply to power this unit\n":"");
    break;

  case PICO_POWER_SUPPLY_CONNECTED:
    printf("\nUsing +5 V power supply voltage.\n");
    status = ps5000aChangePowerSource(handle, PICO_POWER_SUPPLY_CONNECTED);		// Tell the driver we are powered from +5V supply
    break;

  case PICO_USB3_0_DEVICE_NON_USB3_0_PORT:
    do {
      printf("\nUSB 3.0 device on non-USB 3.0 port.");
      printf("\nDo you wish to continue Y/N?\n");
      ch = toupper(_getch());
      
      if (ch == 'Y') {
	printf("\nSwitching to use USB power from non-USB 3.0 port.\n");
	status = ps5000aChangePowerSource(handle, PICO_USB3_0_DEVICE_NON_USB3_0_PORT);		// Tell the driver that's ok

	if (status == PICO_POWER_SUPPLY_UNDERVOLTAGE) {
	  status = changePowerSource(handle, status, unit);
	} else {} // Do nothing
      }
    } while (ch != 'Y' && ch != 'N');
    printf(ch == 'N' ? "Please use a USB 3.0 port or press 'Y'.\n" : "");
    break;

  case PICO_POWER_SUPPLY_UNDERVOLTAGE:
    do {
      printf("\nUSB not supplying required voltage");
      printf("\nPlease plug in the +5 V power supply\n");
      printf("\nHit any key to continue, or Esc to exit...\n");
      ch = _getch();
      
      if (ch == 0x1B) {	// ESC key
	exit(0);
      } else {
	status = ps5000aChangePowerSource(handle, PICO_POWER_SUPPLY_CONNECTED);		// Tell the driver that's ok
      }
    }
    while (status == PICO_POWER_SUPPLY_REQUEST_INVALID);
    break;
  }  // end of switch

  printf("\n");
  return status;
}

/****************************************************************************
* ClearDataBuffers
*
* stops GetData writing values to memory that has been released
****************************************************************************/
PICO_STATUS clearDataBuffers(UNIT * unit)
{
  int32_t i;
  PICO_STATUS status = 0;
  
  for (i = 0; i < unit->channelCount; i++) {
    if(unit->channelSettings[i].enabled) {
      if ((status = ps5000aSetDataBuffers(unit->handle, (PS5000A_CHANNEL) i, NULL, NULL, 0, 0, PS5000A_RATIO_MODE_NONE)) != PICO_OK) {
	printf("clearDataBuffers:ps5000aSetDataBuffers(channel %d) ------ 0x%08lx \n", i, status);
      }
    }
  }
  return status;
}

/****************************************************************************
* BlockDataHandler
* - Used by all block data routines
* - acquires data (user sets trigger mode before calling), displays 10 items
*   and saves all to block.txt
* Input :
* - unit : the unit to use.
* - text : the text to display before the display of data slice
* - offset : the offset into the data buffer to start the display's slice.
* - etsModeSet : a flag to indicate if ETS mode has been set on the device
****************************************************************************/
void blockDataHandler(UNIT * unit, int8_t * text, int32_t offset, int16_t etsModeSet)
{
  int16_t retry;
  int16_t triggerEnabled = 0;
  int16_t pwqEnabled = 0;
  
  int16_t * buffers[2 * PS5000A_MAX_CHANNELS];
  
  int32_t i, j;
  int32_t timeInterval;
  int32_t sampleCount = BUFFER_SIZE;
  int32_t maxSamples;
  int32_t timeIndisposed;
  
  uint32_t downSampleRatio = 1;
  
  int64_t * etsTime; // Buffer for ETS time data
  
  PICO_STATUS status;
  PICO_STATUS powerStatus;
  
  PS5000A_RATIO_MODE ratioMode = PS5000A_RATIO_MODE_NONE;
  
  FILE * fp = NULL;
  
  powerStatus = ps5000aCurrentPowerSource(unit->handle);
  
  for (i = 0; i < unit->channelCount; i++) {
    if (i >= DUAL_SCOPE && unit->channelCount == QUAD_SCOPE && powerStatus == PICO_POWER_SUPPLY_NOT_CONNECTED) {
      // No need to set the channels C and D if Quad channel scope and power supply not connected.
    } else {
      if (unit->channelSettings[i].enabled) {
	buffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
	buffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
	
	status = ps5000aSetDataBuffers(unit->handle, (PS5000A_CHANNEL)i, buffers[i * 2], buffers[i * 2 + 1], sampleCount, 0, ratioMode);
	printf(status ? "blockDataHandler:ps5000aSetDataBuffers(channel %d) ------ 0x%08lx \n":"", i, status);
      }
    }
  }
  
  // Set up ETS time buffers if ETS Block mode data is being captured
  if (etsModeSet) {
    etsTime = (int64_t *) calloc(sampleCount, sizeof (int64_t));   
    status = ps5000aSetEtsTimeBuffer(unit->handle, etsTime, sampleCount);
  }	
  
  /*  Find the maximum number of samples and the time interval (in nanoseconds).
   *	If the function returns PICO_OK, the timebase will be used.  */
  do {
    status = ps5000aGetTimebase(unit->handle, timebase, sampleCount, &timeInterval, &maxSamples, 0);
    
    if (status == PICO_INVALID_NUMBER_CHANNELS_FOR_RESOLUTION) {
      printf("BlockDataHandler: Error - Invalid number of channels for resolution.\n");
      return;
    } else if(status == PICO_OK) { // Do nothing
    } else {
      timebase++;
    }
  } while(status != PICO_OK);
  
  if (!etsModeSet) {
    printf("\nTimebase: %lu  SampleInterval: %ld ns\n", timebase, timeInterval);
  }

  /* Start it collecting, then wait for completion*/
  g_ready = FALSE;

  do {
    retry = 0;
    
    status = ps5000aRunBlock(unit->handle, 0, sampleCount, timebase, &timeIndisposed, 0, callBackBlock, NULL);

    if (status != PICO_OK) {
      // PicoScope 5X4XA/B/D devices...+5 V PSU connected or removed or
      // PicoScope 524XD devices on non-USB 3.0 port
      if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED || 
	  status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT || status == PICO_POWER_SUPPLY_UNDERVOLTAGE) {
	status = changePowerSource(unit->handle, status, unit);
	retry = 1;
      } else {
	printf("BlockDataHandler:ps5000aRunBlock ------ 0x%08lx \n", status);
	return;
      }
    }
  }
  while(retry);

  status = ps5000aIsTriggerOrPulseWidthQualifierEnabled(unit->handle, &triggerEnabled, &pwqEnabled);

  if (triggerEnabled || pwqEnabled) {
    printf("Waiting for trigger... Press any key to abort\n");
  } else {
    printf("Press any key to abort\n");
  }

  while (!g_ready && !_kbhit()) {
    Sleep(0);
  }

  if (g_ready) {

    // Can retrieve data using different ratios and ratio modes from driver
    status = ps5000aGetValues(unit->handle, 0, (uint32_t*) &sampleCount, downSampleRatio, ratioMode, 0, NULL);

    if (status != PICO_OK) {
      // PicoScope 5X4XA/B/D devices...+5 V PSU connected or removed or
      // PicoScope 524XD devices on non-USB 3.0 port
      if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED ||
	  status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT || status == PICO_POWER_SUPPLY_UNDERVOLTAGE) {
	if (status == PICO_POWER_SUPPLY_UNDERVOLTAGE) {
	  changePowerSource(unit->handle, status, unit);
	} else {
	  printf("\nPower Source Changed. Data collection aborted.\n");
	}
      } else {
	printf("blockDataHandler:ps5000aGetValues ------ 0x%08lx \n", status);
      }
    } else {
      /* Print out the first 10 readings, converting the readings to mV if required */
      printf("%s\n",text);
      
      printf("Channels are in (%s):-\n\n", ( scaleVoltages ) ? ("mV") : ("ADC Counts"));
      
      for (j = 0; j < unit->channelCount; j++) {
	if (unit->channelSettings[j].enabled) {
	  printf("Channel %c:    ", 'A' + j);
	}
      }	
      printf("\n\n");
      
      for (i = offset; i < offset+10; i++) {
	for (j = 0; j < unit->channelCount; j++) {
	  if (unit->channelSettings[j].enabled) {
	    printf("  %6d     ", scaleVoltages ? 
		   adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS5000A_CHANNEL_A + j].range, unit)  // If scaleVoltages, print mV value
		   : buffers[j * 2][i]);								   // else print ADC Count
	  }
	}
	printf("\n");
      }

      sampleCount = min(sampleCount, BUFFER_SIZE);
      
      fopen_s(&fp, blockFile, "w");
      
      if (fp != NULL) {
	if (etsModeSet) {
	  fprintf(fp, "ETS Block Data log\n\n");
	} else {
	  fprintf(fp, "Block Data log\n\n");
	}
				
	fprintf(fp,"Results shown for each of the %d Channels are......\n",unit->channelCount);
	fprintf(fp,"Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");

	if (etsModeSet) {
	  fprintf(fp, "Time (fs) ");
	} else {
	  fprintf(fp, "Time (ns) ");
	}

	for (i = 0; i < unit->channelCount; i++) {
	  if (unit->channelSettings[i].enabled) {
	    fprintf(fp," Ch    Max ADC   Max mV  Min ADC   Min mV   ");
	  }
	}
	fprintf(fp, "\n");

	for (i = 0; i < sampleCount; i++) {
	  if (etsModeSet) {
	    fprintf(fp, "%I64d ", etsTime[i]);
	  } else {
	    fprintf(fp, "%I64u ", g_times[0] + ((uint64_t) i) * ((uint64_t) timeInterval));
	  }

	  for (j = 0; j < unit->channelCount; j++) {
	    if (unit->channelSettings[j].enabled) {
	      fprintf(	fp,
			"Ch%C  %6d = %+6dmV, %6d = %+6dmV   ",
			'A' + j,
			buffers[j * 2][i],
			adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS5000A_CHANNEL_A + j].range, unit),
			buffers[j * 2 + 1][i],
			adc_to_mv(buffers[j * 2 + 1][i], unit->channelSettings[PS5000A_CHANNEL_A + j].range, unit));
	    }
	  }
	  
	  fprintf(fp, "\n");
	}
	
      } else {
	printf(	"Cannot open the file %s for writing.\n"
		"Please ensure that you have permission to access the file.\n", blockFile);
      }
    } 
  } else 
    {
      printf("Data collection aborted\n");
      _getch();
    }

  if ((status = ps5000aStop(unit->handle)) != PICO_OK) {
    printf("blockDataHandler:ps5000aStop ------ 0x%08lx \n", status);
  }

  if (fp != NULL) {
    fclose(fp);
  }
	
  for (i = 0; i < unit->channelCount; i++) {
    if (unit->channelSettings[i].enabled) {
      free(buffers[i * 2]);
      free(buffers[i * 2 + 1]);
    }
  }

  if (etsModeSet) {
    free(etsTime);
  }
	
  clearDataBuffers(unit);
}

void displaySettings(UNIT* unit, FILE* file);  // Exceptionally this is used before defined, so declare it here.

/****************************************************************************
* streamDataHandler
* - Used by the two stream data examples - untriggered and triggered
* Inputs:
* - unit - the unit to sample on
* - preTrigger - the number of samples in the pre-trigger phase 
*					(0 if no trigger has been set)
***************************************************************************/
void streamDataHandler(UNIT * unit, uint32_t preTrigger)
{
  int32_t i, j;
  uint32_t sampleCount = 50000; /* make sure overview buffer is large enough */
  FILE * fp = NULL;
  int16_t * buffers[2 * PS5000A_MAX_CHANNELS];
  int16_t * appBuffers[2 * PS5000A_MAX_CHANNELS];
  PICO_STATUS status;
  PICO_STATUS powerStatus;
  uint32_t sampleInterval;
  int32_t index = 0;
  int32_t totalSamples;
  uint32_t postTrigger;
  int16_t autostop;
  uint32_t downsampleRatio;
  uint32_t triggeredAt = 0;
  PS5000A_TIME_UNITS timeUnits;
  PS5000A_RATIO_MODE ratioMode;
  int16_t retry = 0;
  int16_t powerChange = 0;
  uint32_t numStreamingValues = 0;
  
  int k, ktotal;
#define STEPS 160
  int countk[4*STEPS];
  
  BUFFER_INFO bufferInfo;
  
  powerStatus = ps5000aCurrentPowerSource(unit->handle);
  
  for (i = 0; i < unit->channelCount; i++) {
    if (i >= DUAL_SCOPE && unit->channelCount == QUAD_SCOPE && powerStatus == PICO_POWER_SUPPLY_NOT_CONNECTED) {
      // No need to set the channels C and D if Quad channel scope and power supply not connected.
    } else {
      if (unit->channelSettings[i].enabled) {
	buffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
	buffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
	
	status = ps5000aSetDataBuffers(unit->handle, (PS5000A_CHANNEL)i, buffers[i * 2], buffers[i * 2 + 1], sampleCount, 0, PS5000A_RATIO_MODE_NONE);
	
	appBuffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
	appBuffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
	
	printf(status?"StreamDataHandler:ps5000aSetDataBuffers(channel %ld) ------ 0x%08lx \n":"", i, status);
      }
    }
  }
  
  downsampleRatio = 1;
  timeUnits = PS5000A_NS;
  sampleInterval = 8;
  ratioMode = PS5000A_RATIO_MODE_NONE;
  preTrigger = 0;
  postTrigger = 100002000;
  autostop = TRUE;
  
  bufferInfo.unit = unit;	
  bufferInfo.driverBuffers = buffers;
  bufferInfo.appBuffers = appBuffers;
  
  if (autostop) {
    printf("\nStreaming Data for %lu samples", postTrigger / downsampleRatio);
    
    if (preTrigger) {	// We pass 0 for preTrigger if we're not setting up a trigger 
      printf(" after the trigger occurs\nNote: %lu Pre Trigger samples before Trigger arms\n\n",preTrigger / downsampleRatio);
    } else {
      printf("\n\n");
    }
  } else {
    printf("\nStreaming Data continually.\n\n");
  }
  
  g_autoStopped = FALSE;
  
  do {
    retry = 0;
    
    status = ps5000aRunStreaming(unit->handle, &sampleInterval, timeUnits, preTrigger, postTrigger, autostop, 
				 downsampleRatio, ratioMode, sampleCount);
    
    if (status != PICO_OK) {
      // PicoScope 5X4XA/B/D devices...+5 V PSU connected or removed or
      // PicoScope 524XD devices on non-USB 3.0 port
      if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED ||
	  status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT || status == PICO_POWER_SUPPLY_UNDERVOLTAGE) {
	status = changePowerSource(unit->handle, status, unit);
	retry = 1;
      } else {
	printf("streamDataHandler:ps5000aRunStreaming ------ 0x%08lx \n", status);
	return;
      }
    }
  }
  while (retry);
  
  printf("Streaming data...Press a key to stop\n");
  
  fopen_s(&fp, streamFile, "w");
  
  for (k = 0; k < 4*STEPS; k++) countk[k] = 0;
  ktotal = 0;
  
  if (fp != NULL) {
    fprintf(fp,"Streaming Data Log\n\n");
    // fprintf(fp,"For each of the %d Channels, results shown are....\n",unit->channelCount);
    // fprintf(fp,"Maximum Aggregated value ADC Count & mV, Minimum Aggregated value ADC Count & mV\n\n");
    
    for (i = 0; i < unit->channelCount; i++) {
      if (unit->channelSettings[i].enabled) {
	// fprintf(fp,"   Max ADC    Max mV  Min ADC  Min mV   ");
      }
    }
    //fprintf(fp, "\n");
  }
  
  totalSamples = 0;
  
  while (!_kbhit() && !g_autoStopped) {
    /* Poll until data is received. Until then, GetStreamingLatestValues wont call the callback */
    g_ready = FALSE;
    
    status = ps5000aGetStreamingLatestValues(unit->handle, callBackStreaming, &bufferInfo);
    
    // PicoScope 5X4XA/B/D devices...+5 V PSU connected or removed or
    // PicoScope 524XD devices on non-USB 3.0 port
    if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED ||
	status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT || status == PICO_POWER_SUPPLY_UNDERVOLTAGE) {
      if (status == PICO_POWER_SUPPLY_UNDERVOLTAGE) {
	changePowerSource(unit->handle, status, unit);
      }
      printf("\n\nPower Source Change");
      powerChange = 1;
    }
    
    index ++;
    
    if (g_ready && g_sampleCount > 0) { /* Can be ready and have no data, if autoStop has fired */
      if (g_trig) {
	triggeredAt = totalSamples + g_trigAt;		// Calculate where the trigger occurred in the total samples collected
      }
      
      totalSamples += g_sampleCount;
      printf("\nCollected %3li samples, index = %5lu, Total: %6d samples ", g_sampleCount, g_startIndex, totalSamples);
      
      if (g_trig) {
	printf("Trig. at index %lu total %lu", g_trigAt, triggeredAt + 1);	// show where trigger occurred  
      }
      
      for (i = (g_startIndex == 0) ? 1 : g_startIndex; i < (int32_t)(g_startIndex + g_sampleCount); i++) {  // Start from 1 not 0 so we can look back one
	ktotal++;
	if (fp != NULL) {
	  for (j = 0; j < unit->channelCount; j++) {
	    if (unit->channelSettings[j].enabled) {
	      for (k = 0; k < STEPS; k++) {    // Thresholds at 200, 400, 600 ... 10000
		if (appBuffers[j * 2][i] < -200*(k + 1) && appBuffers[j * 2][i - 1] > -200*(k + 1)) countk[k+j*STEPS]++;
	      }   
	      /* fprintf(	fp,
		 "Ch%C  %5d = %+5dmV, %5d = %+5dmV   ",
		 (char)('A' + j),
		 appBuffers[j * 2][i],
		 adc_to_mv(appBuffers[j * 2][i], unit->channelSettings[PS5000A_CHANNEL_A + j].range, unit),
		 appBuffers[j * 2 + 1][i],
		 adc_to_mv(appBuffers[j * 2 + 1][i], unit->channelSettings[PS5000A_CHANNEL_A + j].range, unit));
	      */
	    }
	  }
	  // fprintf(fp, "\n");
	} else {
	  // printf("Cannot open the file %s for writing.\n", streamFile);
	}
	
      }
    }
  }
  
  printf("\n\n");
  
  ps5000aStop(unit->handle);
  
  if (fp != NULL) {
    fprintf(fp, "Index,Thld-ADC,   AThld-mV,ACounts,    BThld-mV, BCounts,    CThld-mV,CCounts,    DThld-mV,DCounts,  No-of-bins,  time-per-bin\n");
    for (k = 0; k < STEPS; k++) {
      fprintf(fp, "%5d,%6d,  ", k, (k + 1) * 200);
      for (j = 0; j < unit->channelCount; j++) {
	int tmv;
	tmv = adc_to_mv((k + 1) * 200, unit->channelSettings[PS5000A_CHANNEL_A + j].range, unit);
	fprintf(fp, " %c:,%7d,%7d, ",'A'+j,tmv,countk[k+j*STEPS]);
      }
      fprintf(fp, " %10d, %d,(ns)\n", ktotal, sampleInterval);
    }
    
    fprintf(fp, "\nPicoscope settings are (ignore the timebase which is not used in a streaming run)\n");
    displaySettings(unit, fp);
    
    fclose (fp);
  }
  
  if (!g_autoStopped && !powerChange) {
    printf("\nData collection aborted\n");
    _getch();
  } else {
    printf("\nData collection complete.\n\n");
  }
  
  for (i = 0; i < unit->channelCount; i++) {
    if(unit->channelSettings[i].enabled) {
      free(buffers[i * 2]);
      free(appBuffers[i * 2]);
      
      free(buffers[i * 2 + 1]);
      free(appBuffers[i * 2 + 1]);
    }
  }
  
  clearDataBuffers(unit);
}


/****************************************************************************
* setTrigger
*
* - Used to call all the functions required to set up triggering.
*
***************************************************************************/
PICO_STATUS setTrigger(UNIT * unit,
	PS5000A_TRIGGER_CHANNEL_PROPERTIES_V2 * channelProperties,
	int16_t nChannelProperties,
	PS5000A_CONDITION * triggerConditions,
	int16_t nTriggerConditions,
	PS5000A_DIRECTION * directions,
	uint16_t nDirections,
	struct tPwq * pwq,
	uint32_t delay,
	uint64_t autoTriggerUs)
{
  PICO_STATUS status;
  PS5000A_CONDITIONS_INFO info = PS5000A_CLEAR;
  PS5000A_CONDITIONS_INFO pwqInfo = PS5000A_CLEAR;
  
  int16_t auxOutputEnabled = 0; // Not used by function call
  
  status = ps5000aSetTriggerChannelPropertiesV2(unit->handle, channelProperties, nChannelProperties, auxOutputEnabled);
  
  if (status != PICO_OK) {
    printf("setTrigger:ps5000aSetTriggerChannelPropertiesV2 ------ Ox%08lx \n", status);
    return status;
  }

  if (nTriggerConditions != 0) {
    info = (PS5000A_CONDITIONS_INFO)(PS5000A_CLEAR | PS5000A_ADD); // Clear and add trigger condition specified unless no trigger conditions have been specified
  }
  
  status = ps5000aSetTriggerChannelConditionsV2(unit->handle, triggerConditions, nTriggerConditions, info);
  
  if (status != PICO_OK) {
    printf("setTrigger:ps5000aSetTriggerChannelConditionsV2 ------ 0x%08lx \n", status);
    return status;
  }
  
  status = ps5000aSetTriggerChannelDirectionsV2(unit->handle, directions, nDirections);
  
  if (status != PICO_OK) {
    printf("setTrigger:ps5000aSetTriggerChannelDirectionsV2 ------ 0x%08lx \n", status);
    return status;
  }
  
  status = ps5000aSetAutoTriggerMicroSeconds(unit->handle, autoTriggerUs);
  
  if (status != PICO_OK) {
    printf("setTrigger:ps5000aSetAutoTriggerMicroSeconds ------ 0x%08lx \n", status);
    return status;
  }
  
  status = ps5000aSetTriggerDelay(unit->handle, delay);
  
  if (status != PICO_OK) {
    printf("setTrigger:ps5000aSetTriggerDelay ------ 0x%08lx \n", status);
    return status;
  }

  // Clear and add pulse width qualifier condition, clear if no pulse width qualifier has been specified
  if (pwq->nPwqConditions != 0) {
    pwqInfo = (PS5000A_CONDITIONS_INFO)(PS5000A_CLEAR | PS5000A_ADD);
  }
  
  status = ps5000aSetPulseWidthQualifierConditions(unit->handle, pwq->pwqConditions, pwq->nPwqConditions, pwqInfo);

  if (status != PICO_OK) {
    printf("setTrigger:ps5000aSetPulseWidthQualifierConditions ------ 0x%08lx \n", status);
    return status;
  }
  
  status = ps5000aSetPulseWidthQualifierDirections(unit->handle, pwq->pwqDirections, pwq->nPwqDirections);

  if (status != PICO_OK) {
    printf("setTrigger:ps5000aSetPulseWidthQualifierDirections ------ 0x%08lx \n", status);
    return status;
  }

  status = ps5000aSetPulseWidthQualifierProperties(unit->handle, pwq->lower, pwq->upper, pwq->type);
  
  if (status != PICO_OK) {
    printf("setTrigger:ps5000aSetPulseWidthQualifierProperties ------ Ox%08lx \n", status);
    return status;
  }
  
  return status;
}

/****************************************************************************
* collectBlockImmediate
*  this function demonstrates how to collect a single block of data
*  from the unit (start collecting immediately)
****************************************************************************/
void collectBlockImmediate(UNIT * unit)
{
  PICO_STATUS status = PICO_OK;
  
  printf("Collect block immediate...\n");
  printf("Press a key to start\n");
  _getch();
  
  setDefaults(unit);
  
  /* Trigger disabled	*/
  status = ps5000aSetSimpleTrigger(unit->handle, 0, PS5000A_CHANNEL_A, 0, PS5000A_RISING, 0, 0);
  
  blockDataHandler(unit, (int8_t *) "First 10 readings\n", 0, FALSE);
}

/****************************************************************************
* collectBlockEts
*  this function demonstrates how to collect a block of
*  data using equivalent time sampling (ETS).
****************************************************************************/
void collectBlockEts(UNIT * unit)
{
  PICO_STATUS status;
  int32_t ets_sampletime;
  int16_t triggerVoltage = 1000; // millivolts
  uint32_t delay = 0;
  int16_t etsModeSet = FALSE;
  
  PS5000A_CHANNEL triggerChannel = PS5000A_CHANNEL_A;
  int16_t voltageRange = inputRanges[unit->channelSettings[triggerChannel].range];
  int16_t triggerThreshold = 0;
  
  // Structures for setting up trigger - declare each as an array of multiple structures if using multiple channels
  struct tPS5000ATriggerChannelPropertiesV2 triggerProperties;
  struct tPS5000ACondition conditions;
  struct tPS5000ADirection directions;
  
  // Struct to hold Pulse Width Qualifier information
  struct tPwq pulseWidth;
  
  memset(&triggerProperties, 0, sizeof(struct tPS5000ATriggerChannelPropertiesV2));
  memset(&conditions, 0, sizeof(struct tPS5000ACondition));
  memset(&directions, 0, sizeof(struct tPS5000ADirection));
  memset(&pulseWidth, 0, sizeof(struct tPwq));
  
  // If the channel is not enabled, warn the User and return
  if (unit->channelSettings[triggerChannel].enabled == 0) {
    printf("collectBlockTriggered: Channel not enabled.");
    return;
  }

  // If the trigger voltage level is greater than the range selected, set the threshold to half
  // of the range selected e.g. for +/- 200mV, set the threshold to 100mV
  if (triggerVoltage > voltageRange) {
    triggerVoltage = (voltageRange / 2);
  }
  
  triggerThreshold = mv_to_adc(triggerVoltage, unit->channelSettings[triggerChannel].range, unit);

  // Set trigger channel properties
  triggerProperties.thresholdUpper = triggerThreshold;
  triggerProperties.thresholdUpperHysteresis = 256 * 10;
  triggerProperties.thresholdLower = triggerThreshold;
  triggerProperties.thresholdLowerHysteresis = 256 * 10;
  triggerProperties.channel = (PS5000A_CHANNEL)(triggerChannel);
  
  // Set trigger conditions
  conditions.source = (PS5000A_CHANNEL)triggerChannel;
  conditions.condition = PS5000A_CONDITION_TRUE;
  
  // Set trigger directions
  directions.source = (PS5000A_CHANNEL)triggerChannel;
  directions.direction = PS5000A_RISING;
  directions.mode = PS5000A_LEVEL;
  
  printf("Collect ETS block...\n");
  printf("Collects when value rises past %d", scaleVoltages? 
	 adc_to_mv(triggerProperties.thresholdUpper,	unit->channelSettings[PS5000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
	 : triggerProperties.thresholdUpper);																// else print ADC Count
  
  printf(scaleVoltages? " mV\n" : "ADC Counts\n");
  printf("Press a key to start...\n");
  _getch();
  
  setDefaults(unit);
  
  // Trigger enabled
  // Rising edge
  // Threshold = 1000 mV
  status = setTrigger(unit, &triggerProperties, 1, &conditions, 1, &directions, 1, &pulseWidth, delay, 0);
  
  status = ps5000aSetEts(unit->handle, PS5000A_ETS_FAST, 20, 4, &ets_sampletime);
  
  if (status == PICO_OK) {
    etsModeSet = TRUE;
  }
  
  printf("ETS Sample Time is %ld picoseconds\n", ets_sampletime);
  
  blockDataHandler(unit, (int8_t *) "Ten readings after trigger\n", BUFFER_SIZE / 10 - 5, etsModeSet); // 10% of data is pre-trigger
  
  status = ps5000aSetEts(unit->handle, PS5000A_ETS_OFF, 0, 0, &ets_sampletime);
  
  etsModeSet = FALSE;
}

/****************************************************************************
* collectBlockTriggered
*  this function demonstrates how to collect a single block of data from the
*  unit, when a trigger event occurs.
****************************************************************************/
void collectBlockTriggered(UNIT * unit)
{
  int16_t triggerVoltage					= 1000; // mV
  PS5000A_CHANNEL triggerChannel	= PS5000A_CHANNEL_A;
  int16_t voltageRange						= inputRanges[unit->channelSettings[triggerChannel].range];
  int16_t triggerThreshold				= 0;
  
  // Structures for setting up trigger - declare each as an array of multiple structures if using multiple channels
  struct tPS5000ATriggerChannelPropertiesV2 triggerProperties; 
  struct tPS5000ACondition conditions; 
  struct tPS5000ADirection directions; 
  
  // Struct to hold Pulse Width Qualifier information
  struct tPwq pulseWidth;
  
  memset(&triggerProperties, 0, sizeof(struct tPS5000ATriggerChannelPropertiesV2));
  memset(&conditions, 0, sizeof(struct tPS5000ACondition));
  memset(&directions, 0, sizeof(struct tPS5000ADirection));
  memset(&pulseWidth, 0, sizeof(struct tPwq));
  
  // If the channel is not enabled, warn the User and return
  if (unit->channelSettings[triggerChannel].enabled == 0) {
    printf("collectBlockTriggered: Channel not enabled.");
    return;
  }

  // If the trigger voltage level is greater than the range selected, set the threshold to half
  // of the range selected e.g. for ±200 mV, set the threshold to 10 0mV
  if (triggerVoltage > voltageRange) {
    triggerVoltage = (voltageRange / 2);
  }
  
  triggerThreshold = mv_to_adc(triggerVoltage, unit->channelSettings[triggerChannel].range, unit);
  
  // Set trigger channel properties
  triggerProperties.thresholdUpper						= triggerThreshold;
  triggerProperties.thresholdUpperHysteresis	= 256 * 10;
  triggerProperties.thresholdLower						= triggerThreshold;
  triggerProperties.thresholdLowerHysteresis	= 256 * 10;
  triggerProperties.channel										= triggerChannel;
  
  // Set trigger conditions
  conditions.source = triggerChannel;
  conditions.condition = PS5000A_CONDITION_TRUE;
  
  // Set trigger directions
  directions.source = triggerChannel;
  directions.direction = PS5000A_RISING;
  directions.mode = PS5000A_LEVEL;
  
  printf("Collect block triggered...\n");
  printf("Collects when value rises past %d", scaleVoltages?
	 adc_to_mv(triggerProperties.thresholdUpper, unit->channelSettings[PS5000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
	 : triggerProperties.thresholdUpper);																// else print ADC Count
  
  printf(scaleVoltages?"mV\n" : "ADC Counts\n");
  
  printf("Press a key to start...\n");
  _getch();
  
  setDefaults(unit);
  
  /* Trigger enabled
   * Rising edge
   * Threshold = 1000 mV */
  setTrigger(unit, &triggerProperties, 1, &conditions, 1, &directions, 1, &pulseWidth, 0, 0);

  blockDataHandler(unit, (int8_t *) "Ten readings after trigger\n", 0, FALSE);
}

/****************************************************************************
* collectRapidBlock
*  this function demonstrates how to collect a set of captures using
*  rapid block mode.
****************************************************************************/
void collectRapidBlock(UNIT * unit)
{
  uint32_t	nCaptures;
  uint32_t	nSegments;
  int32_t		nMaxSamples;
  uint32_t	nSamples = 1000;
  int32_t		timeIndisposed;
  uint32_t	capture;
  int16_t		channel;
  int16_t***	rapidBuffers;
  int16_t*	overflow;
  PICO_STATUS status;
  int16_t		i;
  uint32_t	nCompletedCaptures;
  int16_t		retry;
  
  int16_t		triggerVoltage = 1000; // mV
  PS5000A_CHANNEL triggerChannel = PS5000A_CHANNEL_A;
  int16_t		voltageRange = inputRanges[unit->channelSettings[triggerChannel].range];
  int16_t		triggerThreshold = 0;
  
  int32_t		timeIntervalNs = 0;
  int32_t		maxSamples = 0;
  uint32_t	maxSegments = 0;
  
  uint64_t timeStampCounterDiff = 0;
  
  PS5000A_TRIGGER_INFO * triggerInfo; // Struct to store trigger timestamping information
  
  // Structures for setting up trigger - declare each as an array of multiple structures if using multiple channels
  struct tPS5000ATriggerChannelPropertiesV2 triggerProperties;
  struct tPS5000ACondition conditions;
  struct tPS5000ADirection directions;
  
  // Struct to hold Pulse Width Qualifier information
  struct tPwq pulseWidth;
  
  memset(&triggerProperties, 0, sizeof(struct tPS5000ATriggerChannelPropertiesV2));
  memset(&conditions, 0, sizeof(struct tPS5000ACondition));
  memset(&directions, 0, sizeof(struct tPS5000ADirection));
  memset(&pulseWidth, 0, sizeof(struct tPwq));
  
  // If the channel is not enabled, warn the User and return
  if (unit->channelSettings[triggerChannel].enabled == 0) {
    printf("collectBlockTriggered: Channel not enabled.");
    return;
  }
  
  // If the trigger voltage level is greater than the range selected, set the threshold to half
  // of the range selected e.g. for ±200 mV, set the threshold to 10 0mV
  if (triggerVoltage > voltageRange) {
    triggerVoltage = (voltageRange / 2);
  }
  
  triggerThreshold = mv_to_adc(triggerVoltage, unit->channelSettings[triggerChannel].range, unit);
  
  // Set trigger channel properties
  triggerProperties.thresholdUpper = triggerThreshold;
  triggerProperties.thresholdUpperHysteresis = 256 * 10;
  triggerProperties.thresholdLower = triggerThreshold;
  triggerProperties.thresholdLowerHysteresis = 256 * 10;
  triggerProperties.channel = triggerChannel;
  
  // Set trigger conditions
  conditions.source = triggerChannel;
  conditions.condition = PS5000A_CONDITION_TRUE;
  
  // Set trigger directions
  directions.source = triggerChannel;
  directions.direction = PS5000A_RISING;
  directions.mode = PS5000A_LEVEL;
  
  printf("Collect rapid block triggered...\n");
  printf("Collects when value rises past %d ", scaleVoltages ?
	 adc_to_mv(triggerProperties.thresholdUpper, unit->channelSettings[PS5000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
	 : triggerProperties.thresholdUpper);																// else print ADC Count
  
  printf(scaleVoltages ? "mV\n" : "ADC Counts\n");
  printf("Press any key to abort\n");
  
  setDefaults(unit);
  
  // Trigger enabled
  status = setTrigger(unit, &triggerProperties, 1, &conditions, 1, &directions, 1, &pulseWidth, 0, 0);
  
  // Find the maximum number of segments
  status = ps5000aGetMaxSegments(unit->handle, &maxSegments);
  
  // Set the number of segments - this can be more than the number of waveforms to collect
  nSegments = 64;
  
  if (nSegments > maxSegments) {
    nSegments = maxSegments;
  }
  
  // Set the number of captures
  nCaptures = 10;
  
  // Segment the memory
  status = ps5000aMemorySegments(unit->handle, nSegments, &nMaxSamples);
  
  // Set the number of captures
  status = ps5000aSetNoOfCaptures(unit->handle, nCaptures);
  
  // Run
  timebase = 127;		// 1 MS/s at 8-bit resolution, ~504 kS/s at 12 & 16-bit resolution
  
  // Verify timebase and number of samples per channel for segment 0
  do {
    status = ps5000aGetTimebase(unit->handle, timebase, nSamples, &timeIntervalNs, &maxSamples, 0);
    
    if (status == PICO_INVALID_TIMEBASE) {
      timebase++;
    }
  } while (status != PICO_OK);

  do {
    retry = 0;
    status = ps5000aRunBlock(unit->handle, 0, nSamples, timebase, &timeIndisposed, 0, callBackBlock, NULL);
    
    if (status != PICO_OK) {
      // PicoScope 5X4XA/B/D devices...+5 V PSU connected or removed or
      // PicoScope 524XD devices on non-USB 3.0 port
      if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED || status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT) {
	status = changePowerSource(unit->handle, status, unit);
	retry = 1;
      } else {
	printf("collectRapidBlock:ps5000aRunBlock ------ 0x%08lx \n", status);
      }
    }
  } while (retry);
  
  // Wait until data ready
  g_ready = 0;
  
  while (!g_ready && !_kbhit()) {
    Sleep(0);
  }

  if (!g_ready) {
    _getch();
    status = ps5000aStop(unit->handle);
    status = ps5000aGetNoOfCaptures(unit->handle, &nCompletedCaptures);
    
    printf("Rapid capture aborted. %lu complete blocks were captured\n", nCompletedCaptures);
    printf("\nPress any key...\n\n");
    _getch();
    
    if (nCompletedCaptures == 0) {
      return;
    }

    // Only display the blocks that were captured
    nCaptures = (uint16_t)nCompletedCaptures;
  }

  // Allocate memory
  rapidBuffers = (int16_t ***)calloc(unit->channelCount, sizeof(int16_t*));
  overflow = (int16_t *)calloc(unit->channelCount * nCaptures, sizeof(int16_t));
  
  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      rapidBuffers[channel] = (int16_t **)calloc(nCaptures, sizeof(int16_t*));
    }
  }

  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      for (capture = 0; capture < nCaptures; capture++) {
	rapidBuffers[channel][capture] = (int16_t *)calloc(nSamples, sizeof(int16_t));
      }
    }
  }

  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      for (capture = 0; capture < nCaptures; capture++) {
	status = ps5000aSetDataBuffer(unit->handle, (PS5000A_CHANNEL)channel, rapidBuffers[channel][capture], nSamples, capture, PS5000A_RATIO_MODE_NONE);
      }
    }
  }

  // Allocate memory for the trigger timestamping
  triggerInfo = (PS5000A_TRIGGER_INFO *)malloc(nCaptures * sizeof(PS5000A_TRIGGER_INFO));
  memset(triggerInfo, 0, nCaptures * sizeof(PS5000A_TRIGGER_INFO));

  // Get data
  status = ps5000aGetValuesBulk(unit->handle, &nSamples, 0, nCaptures - 1, 1, PS5000A_RATIO_MODE_NONE, overflow);

  if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED ||
      status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT || status == PICO_POWER_SUPPLY_UNDERVOLTAGE) {
    printf("\nPower Source Changed. Data collection aborted.\n");
  }

  // Retrieve trigger timestamping information
  status = ps5000aGetTriggerInfoBulk(unit->handle, triggerInfo, 0, nCaptures - 1);
  
  if (status == PICO_OK) {
    //print first 10 samples from each capture
    for (capture = 0; capture < nCaptures; capture++) {
      printf("\n");
      
      printf("Capture index %d:-\n\n", capture);
      
      // Trigger Info status & Timestamp 
      printf("Trigger Info:- Status: %u  Trigger index: %u  Timestamp Counter: %I64u\n", triggerInfo[capture].status, triggerInfo[capture].triggerIndex, triggerInfo[capture].timeStampCounter);
      
      // Calculate time between trigger events - the first timestamp is arbitrary so is only used to calculate offsets
      
      // The structure containing the status code with bit flag PICO_DEVICE_TIME_STAMP_RESET will have an arbitrary timeStampCounter value. 
      // This should be the first segment in each run, so in this case segment 0 will be ignored.
      
      if (capture == 0) {
	// Nothing to display
	printf("\n");
      } else if (capture > 0 && triggerInfo[capture].status == PICO_OK) {
	timeStampCounterDiff = triggerInfo[capture].timeStampCounter - triggerInfo[capture - 1].timeStampCounter;
	printf("Time since trigger for last segment: %I64u ns\n\n", (timeStampCounterDiff * (uint64_t)timeIntervalNs));
      } else { }   // Do nothing
			
      for (channel = 0; channel < unit->channelCount; channel++) {
	if (unit->channelSettings[channel].enabled) {
	  printf("Channel %c:\t", 'A' + channel);
	}
      }
      
      printf("\n\n");
      
      for (i = 0; i < 10; i++) {
	for (channel = 0; channel < unit->channelCount; channel++) {
	  if (unit->channelSettings[channel].enabled) {
	    printf("   %6d       ", scaleVoltages ?
		   adc_to_mv(rapidBuffers[channel][capture][i], unit->channelSettings[PS5000A_CHANNEL_A + channel].range, unit)	// If scaleVoltages, print mV value
		   : rapidBuffers[channel][capture][i]);								        // else print ADC Count
	  }
	}
	
	printf("\n");
      }
    }
  }

  // Stop
  status = ps5000aStop(unit->handle);

  // Free memory
  free(overflow);

  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      for (capture = 0; capture < nCaptures; capture++) {
	free(rapidBuffers[channel][capture]);
      }
    }
  }

  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      free(rapidBuffers[channel]);
    }
  }

  free(rapidBuffers);
  free(triggerInfo);
}

/****************************************************************************
* Initialise unit' structure with Variant specific defaults
****************************************************************************/
void set_info(UNIT * unit)
{
  int8_t description [11][25]= { "Driver Version",
				 "USB Version",
				 "Hardware Version",
				 "Variant Info",
				 "Serial",
				 "Cal Date",
				 "Kernel Version",
				 "Digital HW Version",
				 "Analogue HW Version",
				 "Firmware 1",
				 "Firmware 2"};
  
  int16_t i = 0;
  int16_t requiredSize = 0;
  int8_t line [80];
  int32_t variant;
  PICO_STATUS status = PICO_OK;
  
  // Variables used for arbitrary waveform parameters
  int16_t			minArbitraryWaveformValue = 0;
  int16_t			maxArbitraryWaveformValue = 0;
  uint32_t		minArbitraryWaveformSize = 0;
  uint32_t		maxArbitraryWaveformSize = 0;
  
  //Initialise default unit properties and change when required
  unit->sigGen = SIGGEN_FUNCTGEN;
  unit->firstRange = PS5000A_10MV;
  unit->lastRange = PS5000A_20V;
  unit->channelCount = DUAL_SCOPE;
  unit->awgBufferSize = MIN_SIG_GEN_BUFFER_SIZE;
  unit->digitalPortCount = 0;
  
  if (unit->handle)  {
    printf("Device information:-\n\n");
    
    for (i = 0; i < 11; i++) {
      status = ps5000aGetUnitInfo(unit->handle, line, sizeof (line), &requiredSize, i);

      // info = 3 - PICO_VARIANT_INFO
      if (i == PICO_VARIANT_INFO) {
	variant = atoi(line);
	memcpy(&(unit->modelString), line, sizeof(unit->modelString)==5?5:sizeof(unit->modelString));
	
	unit->channelCount = (int16_t)line[1];
	unit->channelCount = unit->channelCount - 48; // Subtract ASCII 0 (48)
	
	// Determine if the device is an MSO
	if (strstr(line, "MSO") != NULL) {
	  unit->digitalPortCount = 2;
	} else {
	  unit->digitalPortCount = 0;
	}
      }	else if (i == PICO_BATCH_AND_SERIAL) {  // info = 4 - PICO_BATCH_AND_SERIAL
	memcpy(&(unit->serial), line, requiredSize);
      }
      
      printf("%s: %s\n", description[i], line);
    }

    printf("\n");

    // Set sig gen parameters
    // If device has Arbitrary Waveform Generator, find the maximum AWG buffer size
    status = ps5000aSigGenArbitraryMinMaxValues(unit->handle, &minArbitraryWaveformValue, &maxArbitraryWaveformValue, &minArbitraryWaveformSize, &maxArbitraryWaveformSize);
    unit->awgBufferSize = maxArbitraryWaveformSize;
    
    if (unit->awgBufferSize > 0) {
      unit->sigGen = SIGGEN_AWG;
    } else {
      unit->sigGen = SIGGEN_FUNCTGEN;
    }
  }
}

/****************************************************************************
* Select input voltage ranges for channels
****************************************************************************/
void setVoltages(UNIT * unit)
{
  PICO_STATUS powerStatus = PICO_OK;
  PICO_STATUS status = PICO_OK;
  PS5000A_DEVICE_RESOLUTION resolution = PS5000A_DR_8BIT;
  
  int32_t i, ch;
  int32_t count = 0;
  int16_t numValidChannels = unit->channelCount; // Dependent on power setting - i.e. channel A & B if USB powered on 4-channel device
  int16_t numEnabledChannels = 0;
  int16_t retry = FALSE;
  
  if (unit->channelCount == QUAD_SCOPE) {
    powerStatus = ps5000aCurrentPowerSource(unit->handle); 
    
    if (powerStatus == PICO_POWER_SUPPLY_NOT_CONNECTED) {
      numValidChannels = DUAL_SCOPE;
    }
  }

  // See what ranges are available... 
  for (i = unit->firstRange; i <= unit->lastRange; i++) {
    printf("%d -> %d mV\n", i, inputRanges[i]);
  }

  do {
    count = 0;
    
    do {
      // Ask the user to select a range
      printf("Specify voltage range (%d..%d)\n", unit->firstRange, unit->lastRange);
      printf("99 - switches channel off\n");
      
      for (ch = 0; ch < numValidChannels; ch++) {
	printf("\n");
	
	do {
	  printf("Channel %c: ", 'A' + ch);
	  fflush(stdin);
	  scanf_s("%hd", &(unit->channelSettings[ch].range));
	} while (unit->channelSettings[ch].range != 99 && (unit->channelSettings[ch].range < unit->firstRange || unit->channelSettings[ch].range > unit->lastRange));

	if (unit->channelSettings[ch].range != 99) {
	  printf(" - %d mV\n", inputRanges[unit->channelSettings[ch].range]);
	  unit->channelSettings[ch].enabled = TRUE;
	  count++;
	} else {
	  printf("Channel Switched off\n");
	  unit->channelSettings[ch].enabled = FALSE;
	  unit->channelSettings[ch].range = PS5000A_MAX_RANGES-1;
	}
      }
      printf(count == 0? "\n** At least 1 channel must be enabled **\n\n":"");
    } while (count == 0);	// must have at least one channel enabled

    status = ps5000aGetDeviceResolution(unit->handle, &resolution);
    
    // Verify that the number of enabled channels is valid for the resolution set.
    switch (resolution) {
    case PS5000A_DR_15BIT:
      if (count > 2) {
	printf("\nError: Only 2 channels may be enabled with 15-bit resolution set.\n");
	printf("Please switch off %d channel(s).\n", numValidChannels - 2);
	retry = TRUE;
      } else {
	retry = FALSE;
      }
      break;
      
    case PS5000A_DR_16BIT:
      if (count > 1) {
	printf("\nError: Only one channes may be enabled with 16-bit resolution set.\n");
	printf("Please switch off %d channel(s).\n", numValidChannels - 1);
	retry = TRUE;
      } else {
	retry = FALSE;
      }
      break;

    default:
      retry = FALSE;
      break;
    }
    
    printf("\n");
  } while (retry == TRUE);
  
  setDefaults(unit);	// Put these changes into effect
}

/****************************************************************************
* Select input voltage ranges from values on function (or print and return an error)
****************************************************************************/
int setVoltagesFunction(UNIT* unit, int achan, int bchan, int cchan, int dchan)
{
  int chans[4],c,e;
  
  PICO_STATUS powerStatus = PICO_OK;
  PICO_STATUS status = PICO_OK;
  PS5000A_DEVICE_RESOLUTION resolution = PS5000A_DR_8BIT;
  
  int16_t numValidChannels = unit->channelCount; // Dependent on power setting - i.e. channel A & B if USB powered on 4-channel device
  int16_t numEnabledChannels = 0;
  
  if (unit->channelCount == QUAD_SCOPE) {
    powerStatus = ps5000aCurrentPowerSource(unit->handle);
    if (powerStatus == PICO_POWER_SUPPLY_NOT_CONNECTED) numValidChannels = DUAL_SCOPE;
  }
  
  chans[0] = achan;
  chans[1] = bchan;
  chans[2] = cchan;
  chans[3] = dchan;
  e = 0;
  for (c = 0; c < numValidChannels; c++) {   // Check valid input
    if (chans[c] != 99 && (chans[c] < unit->firstRange || chans[c] > unit->lastRange)) {
      printf("setVoltagesFunction: Invalid input parameter value %d for channel %d\n",chans[c],c);
      if (chans[c] != 99) numEnabledChannels++;
      e++;
    }
  }
  if (e) return e;   // Something wrong
  if (numEnabledChannels == 0) {
    printf("setVoltagesFunction: Must have at least one enabled channel\n");
    return 1;
  }
  
  status = ps5000aGetDeviceResolution(unit->handle, &resolution);  // Only set if 8 or 12 bit
  switch (resolution) {
  case PS5000A_DR_12BIT:
  case PS5000A_DR_8BIT:
    break;   // Keep these
    
  case PS5000A_DR_14BIT:
  case PS5000A_DR_15BIT:
  case PS5000A_DR_16BIT:
  default:     // Not these
    return 1;
  }
  
  for (c = 0; c < numValidChannels; c++) {
    if (chans[c] != 99) {
      unit->channelSettings[c].enabled = TRUE;
      unit->channelSettings[c].range = chans[c];
    } else {
      unit->channelSettings[c].enabled = FALSE;
      unit->channelSettings[c].range = PS5000A_MAX_RANGES - 1;
    }
  }
  
  setDefaults(unit);	// Put these changes into effect
  return 0;
}

/****************************************************************************
* setTimebase
* Select timebase, set time units as nano seconds
*
****************************************************************************/
void setTimebase(UNIT * unit)
{
  PICO_STATUS status = PICO_OK;
  PICO_STATUS powerStatus = PICO_OK;
  int32_t timeInterval;
  int32_t maxSamples;
  int32_t ch;
  
  uint32_t shortestTimebase;
  double timeIntervalSeconds;
  
  PS5000A_CHANNEL_FLAGS enabledChannelOrPortFlags = (PS5000A_CHANNEL_FLAGS)0;
  
  int16_t numValidChannels = unit->channelCount; // Dependent on power setting - i.e. channel A & B if USB powered on 4-channel device
  
  if (unit->channelCount == QUAD_SCOPE) {
    powerStatus = ps5000aCurrentPowerSource(unit->handle);
    
    if (powerStatus == PICO_POWER_SUPPLY_NOT_CONNECTED) {
      numValidChannels = DUAL_SCOPE;
    }
  }
  
  // Find the analogue channels that are enabled - if an MSO model is being used, this will need to be
  // modified to add channel flags for enabled digital ports
  for (ch = 0; ch < numValidChannels; ch++) {
    if (unit->channelSettings[ch].enabled) {
      enabledChannelOrPortFlags = enabledChannelOrPortFlags | (PS5000A_CHANNEL_FLAGS)(1 << ch);
    }
  }
  
  // Find the shortest possible timebase and inform the user.
  status = ps5000aGetMinimumTimebaseStateless(unit->handle, enabledChannelOrPortFlags, &shortestTimebase, &timeIntervalSeconds, unit->resolution);
  
  if (status != PICO_OK) {
    printf("setTimebase:ps5000aGetMinimumTimebaseStateless ------ 0x%08lx \n", status);
    return;
  }
  
  printf("Shortest timebase index available %d (%.9f seconds).\n", shortestTimebase, timeIntervalSeconds);
  
  printf("Specify desired timebase: ");
  fflush(stdin);
  scanf_s("%lud", &timebase);
  
  do {
    status = ps5000aGetTimebase(unit->handle, timebase, BUFFER_SIZE, &timeInterval, &maxSamples, 0);
    
    if (status == PICO_INVALID_NUMBER_CHANNELS_FOR_RESOLUTION) {
      printf("SetTimebase: Error - Invalid number of channels for resolution.\n");
      return;
    }
    else if (status == PICO_OK) { // Do nothing
    } else {
      timebase++; // Increase timebase if the one specified can't be used. 
    }

  } while (status != PICO_OK);

  printf("Timebase used %lu = %ld ns sample interval\n", timebase, timeInterval);
}

/****************************************************************************
* printResolution
*
* Outputs the resolution in text format to the console window
****************************************************************************/
void printResolution(PS5000A_DEVICE_RESOLUTION * resolution, FILE * file)
{
  switch(*resolution) {
  case PS5000A_DR_8BIT:  fprintf(file, "8 bits");  break;
  case PS5000A_DR_12BIT: fprintf(file, "12 bits"); break;
  case PS5000A_DR_14BIT: fprintf(file, "14 bits"); break;
  case PS5000A_DR_15BIT: fprintf(file, "15 bits"); break;
  case PS5000A_DR_16BIT: fprintf(file, "16 bits"); break;
  default: break;
  }
  fprintf(file, "\n");
}

/****************************************************************************
* setResolution
* Set resolution for the device
*
****************************************************************************/
void setResolution(UNIT * unit)
{
  int16_t value;
  int16_t i;
  int16_t numEnabledChannels = 0;
  int16_t retry;
  int32_t resolutionInput;
  
  PICO_STATUS status;
  PS5000A_DEVICE_RESOLUTION resolution;
  PS5000A_DEVICE_RESOLUTION newResolution = PS5000A_DR_8BIT;
  
  // Determine number of channels enabled
  for(i = 0; i < unit->channelCount; i++) {
    if(unit->channelSettings[i].enabled == TRUE) {
      numEnabledChannels++;
    }
  }
  
  if(numEnabledChannels == 0) {
    printf("setResolution: Please enable channels.\n");
    return;
  }
  
  status = ps5000aGetDeviceResolution(unit->handle, &resolution);
  
  if(status == PICO_OK) {
    printf("Current resolution: ");
    printResolution(&resolution,stdout);
  } else {
    printf("setResolution:ps5000aGetDeviceResolution ------ 0x%08lx \n", status);
    return;
  }

  printf("\n");
  printf("Select device resolution:\n");
  printf("0: 8 bits\n");
  printf("1: 12 bits\n");
  printf("2: 14 bits\n");
  
  if(numEnabledChannels <= 2) {
    printf("3: 15 bits\n");
  }

  if(numEnabledChannels == 1) {
    printf("4: 16 bits\n\n");
  }

  retry = TRUE;

  do {
    if(numEnabledChannels == 1) {
      printf("Resolution [0...4]: ");
    } else if(numEnabledChannels == 2) {
      printf("Resolution [0...3]: ");
    } else {
      printf("Resolution [0...2]: ");
    }
	
    fflush(stdin);
    scanf_s("%lud", &resolutionInput);

    newResolution = (PS5000A_DEVICE_RESOLUTION)resolutionInput;

    // Verify if resolution can be selected for number of channels enabled
    if (newResolution == PS5000A_DR_16BIT && numEnabledChannels > 1) {
      printf("setResolution: 16 bit resolution can only be selected with 1 channel enabled.\n");
    } else if (newResolution == PS5000A_DR_15BIT && numEnabledChannels > 2) {
      printf("setResolution: 15 bit resolution can only be selected with a maximum of 2 channels enabled.\n");
    } else if(newResolution < PS5000A_DR_8BIT && newResolution > PS5000A_DR_16BIT) {
      printf("setResolution: Resolution index selected out of bounds.\n");
    } else {
      retry = FALSE;
    }
  } while(retry);
  printf("\n");

  status = ps5000aSetDeviceResolution(unit->handle, (PS5000A_DEVICE_RESOLUTION) newResolution);

  if (status == PICO_OK) {
    unit->resolution = newResolution;
    
    printf("Resolution selected: ");
    printResolution(&newResolution, stdout);
    
    // The maximum ADC value will change if transitioning from 8 bit to >= 12 bit or vice-versa
    ps5000aMaximumValue(unit->handle, &value);
    unit->maxADCValue = value;
  } else {
    printf("setResolution:ps5000aSetDeviceResolution ------ 0x%08lx \n", status);
  }
}

/*****************************************************************************************************************
* setSignalGenerator
* Sets the signal generator
* - allows user to set frequency and waveform
* - allows for custom waveform (values -32768..32767) 
* - of up to 16384 samples (PicoScope 5X42B), 32768 samples (PicoScope 5X43B), or 49152 samples (PicoScope 5X44B)
*****************************************************************************************************************/
void setSignalGenerator(UNIT * unit)
{
  PICO_STATUS status;
  int16_t waveform;
  double frequency = 1.0;
  int8_t fileName [128];
  FILE * fp = NULL;
  int16_t * arbitraryWaveform;
  int32_t waveformSize = 0;
  uint32_t pkpk = 4000000;	// ±2 V
  int32_t offset = 0;
  int8_t ch;
  int16_t choice;
  uint32_t deltaPhase = 0;
  
  while (_kbhit()) {			// use up keypress
    _getch();
  }
  

  do {
    printf("\nSignal Generator\n================\n");
    printf("0 - SINE         1 - SQUARE\n");
    printf("2 - TRIANGLE     3 - DC VOLTAGE\n");
    if(unit->sigGen == SIGGEN_AWG) {
      printf("4 - RAMP UP      5 - RAMP DOWN\n");
      printf("6 - SINC         7 - GAUSSIAN\n");
      printf("8 - HALF SINE    A - AWG WAVEFORM\n");
    }
    printf("F - SigGen Off\n\n");
    
    ch = _getch();
    
    if (ch >= '0' && ch <='9') {
      choice = ch -'0';
    } else {
      ch = toupper(ch);
    }
  } while((unit->sigGen == SIGGEN_FUNCTGEN && ch != 'F' && 
	   (ch < '0' || ch > '3')) || (unit->sigGen == SIGGEN_AWG && ch != 'A' && ch != 'F' && (ch < '0' || ch > '8')));

  if(ch == 'F') {			// If we're going to turn off siggen
    printf("Signal generator Off\n");
    waveform = PS5000A_DC_VOLTAGE;		// DC Voltage
    pkpk = 0;							// 0V
    waveformSize = 0;
  } else {
    if (ch == 'A' && unit->sigGen == SIGGEN_AWG) {		// Set the AWG
      arbitraryWaveform = (int16_t*)malloc( unit->awgBufferSize * sizeof(int16_t));
      memset(arbitraryWaveform, 0, unit->awgBufferSize * sizeof(int16_t));
      
      waveformSize = 0;
      
      printf("Select a waveform file to load: ");
      scanf_s("%s", fileName, 128);
      
      if (fopen_s(&fp, fileName, "r") == 0) { 
	// Having opened file, read in data - one number per line (max 16384 lines for PicoScope 5X42B device, 
	// 32768 for PicoScope 5X43B & 5000D devices, 49152 for PicoScope 5X44B), with values in (-32768...+32767)
	// The min and max arbitrary waveform values can also be obtained from the ps5000aSigGenArbitraryMinMaxValues()
	// function.
	while (EOF != fscanf_s(fp, "%hi", (arbitraryWaveform + waveformSize)) && waveformSize++ < unit->awgBufferSize - 1);
	fclose(fp);
	printf("File successfully loaded\n");
      } else {
	printf("Invalid filename\n");
	return;
      }
    } else {			// Set one of the built in waveforms
      switch (choice) {
      case 0: waveform = PS5000A_SINE;     break;
      case 1: waveform = PS5000A_SQUARE;   break;
      case 2: waveform = PS5000A_TRIANGLE; break;

      case 3:
	waveform = PS5000A_DC_VOLTAGE;
	do {
	  printf("\nEnter offset in uV: (0 to 2000000)\n"); // Ask user to enter DC offset level;
	  scanf_s("%lu", &offset);
	} while (offset < 0 || offset > 2000000);
	break;
	
      case 4: waveform = PS5000A_RAMP_UP;  break;
	
      case 5: waveform = PS5000A_RAMP_DOWN; break;
      case 6: waveform = PS5000A_SINC;     break;
      case 7: waveform = PS5000A_GAUSSIAN; break;
      case 8: waveform = PS5000A_HALF_SINE; break;
      default:  waveform = PS5000A_SINE;   break;
      }
    }
    
    if(waveform < 8 || (ch == 'A' && unit->sigGen == SIGGEN_AWG)) {				// Find out frequency if required
      do {
	printf("\nEnter frequency in Hz: ( >0 to 20000000)\n"); // Ask user to enter signal frequency;
	scanf_s("%lf", &frequency);
      } while (frequency <= 0 || frequency > 20000000);
    }

    if (waveformSize > 0) {
      // Find the delta phase value for the frequency selected
      status = ps5000aSigGenFrequencyToPhase(unit->handle, frequency, PS5000A_SINGLE, (uint32_t) waveformSize, &deltaPhase);
      
      status = ps5000aSetSigGenArbitrary(	
         unit->handle,
	 0,				// offset voltage
	 pkpk,				// PkToPk in microvolts. Max = 4uV  +2v to -2V
	 deltaPhase,			// start delta
	 deltaPhase,			// stop delta
	 0,
	 0, 
	 arbitraryWaveform, 
	 waveformSize, 
	 (PS5000A_SWEEP_TYPE)0,
	 (PS5000A_EXTRA_OPERATIONS)0,
	 PS5000A_SINGLE,
	 0, 
	 0, 
	 PS5000A_SIGGEN_RISING,
	 PS5000A_SIGGEN_NONE,
	 0);

      printf(status?"\nps5000aSetSigGenArbitrary: Status Error 0x%x \n":"", (uint32_t)status);	// If status != 0, show the error
    } else {
      status = ps5000aSetSigGenBuiltInV2(
	 unit->handle,
	 offset, 
	 pkpk, 
	 (PS5000A_WAVE_TYPE) waveform, 
	 frequency, 
	 frequency, 
	 0, 
	 0, 
	 (PS5000A_SWEEP_TYPE) 0,
	 (PS5000A_EXTRA_OPERATIONS) 0,
	 0, 
	 0, 
	 (PS5000A_SIGGEN_TRIG_TYPE) 0,
	 (PS5000A_SIGGEN_TRIG_SOURCE) 0,
	 0);
			
      printf(status?"\nps5000aSetSigGenBuiltIn: Status Error 0x%x \n":"", (uint32_t) status);		// If status != 0, show the error
    }
  }
}

/****************************************************************************
* collectStreamingImmediate
*  This function demonstrates how to collect a stream of data
*  from the unit (start collecting immediately)
***************************************************************************/
void collectStreamingImmediate(UNIT * unit)
{
  PICO_STATUS status = PICO_OK;
  
  setDefaults(unit);
  
  printf("Collect streaming...\n");
  printf("Data is written to disk file (stream.txt)\n");
  printf("Press a key to start\n");
  int c = _getch();
  if (c);    // Avoid unused data warning
  
  /* Trigger disabled	*/
  status = ps5000aSetSimpleTrigger(unit->handle, 0, PS5000A_CHANNEL_A, 0, PS5000A_RISING, 0, 0);
  
  streamDataHandler(unit, 0);
}

/****************************************************************************
* collectStreamingTriggered
*  This function demonstrates how to collect a stream of data
*  from the unit (start collecting on trigger)
***************************************************************************/
void collectStreamingTriggered(UNIT * unit)
{
  int16_t triggerVoltage = 1000; // mV
  PS5000A_CHANNEL triggerChannel = PS5000A_CHANNEL_A;
  int16_t voltageRange = inputRanges[unit->channelSettings[triggerChannel].range];
  int16_t triggerThreshold = 0;
  
  // Structures for setting up trigger - declare each as an array of multiple structures if using multiple channels
  struct tPS5000ATriggerChannelPropertiesV2 triggerProperties;
  struct tPS5000ACondition conditions;
  struct tPS5000ADirection directions;
  
  // Struct to hold Pulse Width Qualifier information
  struct tPwq pulseWidth;
  
  memset(&triggerProperties, 0, sizeof(struct tPS5000ATriggerChannelPropertiesV2));
  memset(&conditions, 0, sizeof(struct tPS5000ACondition));
  memset(&directions, 0, sizeof(struct tPS5000ADirection));
  memset(&pulseWidth, 0, sizeof(struct tPwq));
  
  // If the channel is not enabled, warn the User and return
  if (unit->channelSettings[triggerChannel].enabled == 0) {
    printf("collectStreamingTriggered: Channel not enabled.");
    return;
  }
  
  // If the trigger voltage level is greater than the range selected, set the threshold to half
  // of the range selected e.g. for ±200 mV, set the threshold to 100 mV
  if (triggerVoltage > voltageRange) {
    triggerVoltage = (voltageRange / 2);
  }
  
  triggerThreshold = mv_to_adc(triggerVoltage, unit->channelSettings[triggerChannel].range, unit);
  
  // Set trigger channel properties
  triggerProperties.thresholdUpper = triggerThreshold;
  triggerProperties.thresholdUpperHysteresis = 256 * 10;
  triggerProperties.thresholdLower = triggerThreshold;
  triggerProperties.thresholdLowerHysteresis = 256 * 10;
  triggerProperties.channel = triggerChannel;
  
  // Set trigger conditions
  conditions.source = triggerChannel;
  conditions.condition = PS5000A_CONDITION_TRUE;
  
  // Set trigger directions
  directions.source = triggerChannel;
  directions.direction = PS5000A_RISING;
  directions.mode = PS5000A_LEVEL;
  
  printf("Collect streaming triggered...\n");
  printf("Data is written to disk file (stream.txt)\n");
  printf("Press a key to start\n");
  int c = _getch();
  if (c);  // Avoid compiler warning 
  
  setDefaults(unit);
  
  /* Trigger enabled
   * Rising edge
   * Threshold = 1000 mV */
  setTrigger(unit, &triggerProperties, 1, &conditions, 1, &directions, 1, &pulseWidth, 0, 0);
  
  streamDataHandler(unit, 0);
}


/****************************************************************************
* displaySettings 
* Displays information about the user configurable settings in this example
* Parameters 
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
void displaySettings(UNIT *unit, FILE * file)   // Note this is used higher up, so there is a definition
{
  int32_t ch;
  int32_t voltage;
  PICO_STATUS status = PICO_OK;
  PS5000A_DEVICE_RESOLUTION resolution = PS5000A_DR_8BIT;
  
  // printf("Readings will be scaled in %s\n", (scaleVoltages)? ("millivolts") : ("ADC counts"));
  
  fprintf(file, "Channels");
  for (ch = 0; ch < unit->channelCount; ch++) {
    if (!(unit->channelSettings[ch].enabled)) {
      fprintf(file, " %c (Off),", 'A' + ch);
    } else {
      voltage = inputRanges[unit->channelSettings[ch].range];
      fprintf(file, " %c (Range = ", 'A' + ch);
      
      if (voltage < 1000) {
	fprintf(file, "%dmV),", voltage);
      } else {
	fprintf(file, "%dV),", voltage / 1000);
      }
    }
  }

  fprintf(file, "\n");
  fprintf(file, "Timebase setting: %d    ",timebase);
  
  status = ps5000aGetDeviceResolution(unit->handle, &resolution);
  
  fprintf(file, "Device Resolution: ");
  printResolution(&resolution, file);
  
}

/****************************************************************************
* openDevice 
* Parameters 
* - unit        pointer to the UNIT structure, where the handle will be stored
* - serial		pointer to the int8_t array containing serial number
*
* Returns
* - PICO_STATUS to indicate success, or if an error occurred
***************************************************************************/
PICO_STATUS openDevice(UNIT *unit, int8_t *serial)
{
  PICO_STATUS status;
  unit->resolution = PS5000A_DR_8BIT;
  
  if (serial == NULL) {
    status = ps5000aOpenUnit(&unit->handle, NULL, unit->resolution);
  } else {
    status = ps5000aOpenUnit(&unit->handle, serial, unit->resolution);
  }
  
  unit->openStatus = (int16_t) status;
  unit->complete = 1;
  
  return status;
}

/****************************************************************************
* handleDevice
* Parameters
* - unit        pointer to the UNIT structure, where the handle will be stored
*
* Returns
* - PICO_STATUS to indicate success, or if an error occurred
***************************************************************************/
PICO_STATUS handleDevice(UNIT * unit)
{
  int16_t value = 0;
  int32_t i;
  struct tPwq pulseWidth;
  PICO_STATUS status;
  
  printf("Handle: %d\n", unit->handle);
  
  if (unit->openStatus != PICO_OK) {
    printf("Unable to open device\n");
    printf("Error code : 0x%08x\n", (uint32_t) unit->openStatus);
    while(!_kbhit());
    exit(99); // exit program
  }

  printf("Device opened successfully, cycle %d\n\n", ++cycles);
	
  // Setup device info - unless it's set already
  if (unit->model == MODEL_NONE) {
    set_info(unit);
  }

  // Turn off any digital ports (MSO models only)
  if (unit->digitalPortCount > 0) {
    printf("Turning off digital ports.");
    
    for (i = 0; i < unit->digitalPortCount; i++) {
      status = ps5000aSetDigitalPort(unit->handle, (PS5000A_CHANNEL)(i + PS5000A_DIGITAL_PORT0), 0, 0);
    }
  }
	
  timebase = 1;

  ps5000aMaximumValue(unit->handle, &value);
  unit->maxADCValue = value;
  
  status = ps5000aCurrentPowerSource(unit->handle);
	
  for (i = 0; i < unit->channelCount; i++) {
    // Do not enable channels C and D if power supply not connected for PicoScope 544XA/B devices
    if(unit->channelCount == QUAD_SCOPE && status == PICO_POWER_SUPPLY_NOT_CONNECTED && i >= DUAL_SCOPE)  {
      unit->channelSettings[i].enabled = FALSE;
    } else {
      unit->channelSettings[i].enabled = TRUE;
    }
    
    unit->channelSettings[i].DCcoupled = TRUE;
    unit->channelSettings[i].range = PS5000A_5V;
    unit->channelSettings[i].analogueOffset = 0.0f;
  }

  memset(&pulseWidth, 0, sizeof(struct tPwq));
  
  setDefaults(unit);
  
  /* Trigger disabled	*/
  status = ps5000aSetSimpleTrigger(unit->handle, 0, PS5000A_CHANNEL_A, 0, PS5000A_RISING, 0, 0);
  
  return unit->openStatus;
}

/****************************************************************************
* closeDevice 
****************************************************************************/
void closeDevice(UNIT *unit)
{
  ps5000aCloseUnit(unit->handle);
}

/****************************************************************************
* PicoscopeMenu
* Controls default functions of the selected unit
*  [This routine is the same as in the picoscope example, except that the
*   exit option 'X' has been replaced by an option 'P' to return to the
*   NP08 practical menu] 
* Parameters
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
void PicoscopeMenu(UNIT *unit)
{
  int8_t ch = '.';
  while (ch != 'P') {    /* Changed, was 'X' */
    displaySettings(unit, stdout);

    printf("\n\n");
    printf("Please select operation:\n\n");
    
    printf("B - Immediate block                           V - Set voltages\n");
    printf("T - Triggered block                           I - Set timebase\n");
    printf("E - Collect a block of data using ETS         A - ADC counts/mV\n");
    printf("R - Collect set of rapid captures\n");
    printf("S - Immediate streaming\n");
    printf("W - Triggered streaming\n");
    
    if(unit->sigGen != SIGGEN_NONE) {
      printf("G - Signal generator\n");
    }
		
    printf("D - Set resolution\n");
    printf("                                              P - Return to NP08 main menu\n");
    printf("Operation:");
    
    ch = toupper(_getch());
    
    printf("\n\n");
    
    switch (ch) {
    case 'B': collectBlockImmediate(unit);     break;
    case 'T': collectBlockTriggered(unit);     break;
    case 'R': collectRapidBlock(unit);         break;
    case 'S': collectStreamingImmediate(unit); break;
    case 'W': collectStreamingTriggered(unit); break;
    case 'E': collectBlockEts(unit);           break;
      
    case 'G':
      if(unit->sigGen == SIGGEN_NONE) {
	printf("This model does not have a signal generator\n\n");
	break;
      }
      setSignalGenerator(unit);
      break;
      
    case 'V': setVoltages(unit);               break;
    case 'I': setTimebase(unit);               break;
    case 'A': scaleVoltages = !scaleVoltages;  break;
    case 'D': setResolution(unit);             break;
    case 'P':                                  break;
    default: printf("Invalid operation\n");    break;
    }
  }
}

/****************************************************************************
 This is where the NP08 customisation begins
****************************************************************************/

typedef struct NP08Variables {
  // Here are the important run parameters needed in NP08CollectRapidBlock()
  uint32_t nSegments;    // Number of segments desired
  uint32_t nCaptures;    // Number of captures desired, must be less than nSegments/#active channels
  int32_t nSamples;     // Number of samples to take per capture (i.e. how big is the time window in ticks) 
  int32_t nPreSamples;  // Number of samples to take before the trigger (nSamples = nPreSamples+nPostSamples,
                        //   we don't have a variable for nPostSamples but calculate it each time)
  // Note the resolution selection is set in the device itself, we don't store in a local variable.  TODO print it
  // Note the timebase setting is a global variable.  In collectRapidMode, it gets set to 127, which is probably
  //  a hack put in by the programmers.   It gets checked and increased if it is too fast depending on the number of
  //  channels enabled and the resolution setting, so we store the value used for data collection in the variable below.
  PS5000A_CHANNEL trigChannel;  // Which channel to trigger on
  int16_t trigThreshold;        // Trigger threshold in picoscope-defined ADC counts
  PS5000A_THRESHOLD_DIRECTION trigDirection;  // Rising edge, falling edge etc
  uint32_t trigDelay;           // Trigger delay (shift the nPreSamples/nSamples-nPreSamples window later in ticks)
  int16_t trigAuto_ms;          // If zero, no auto trigger, if non-zero, number of ms to look for trigger before triggering
  int16_t trigUseSimple;        // Non-zero means use the simple trigger setup call; 0=use the full one (not debugged)
  uint32_t maxLoopGroups;       // Number of events to collect in loop function
  uint32_t maxFileSize;         // Maximum file size in loop function
  uint32_t vetoB;               // Number of clock ticks around the AB coincidence to avoid looking for the second B
  uint32_t vetoC;               // Number of clock ticks around the AB coincidence to look for the C veto
  uint32_t waveOnOff;           // 1=Write wave info in records, 0 = don't write wave data
  
  int32_t secondChan;        // Channel number to hunt for second peak
  int32_t secondMinDelay;    //  Minimum delay from first peak to consider (was fixed at 50 ticks)
  int32_t writePeakCount;    // (exists, but currently ignored.  Value 0->3 = require peak on this channel to be above np08->writePeakHeight
                             //    Value 11->14 = require the number of channels with peaks to be bigger than this)
  int32_t writePeakHeight;   // Used as threshold on peak to write out (only works if higher than the time threshold, so may be useless)
  int32_t cfdOnOff;          // Enables writing CFD values

  // Peak finding discriminator thresholds in picoscope defined ADC counts (same as trigThreshold)
  int16_t peakThreshold[4];

  // These next three variables were local to NP08CollectRapidBlock, put them here to preserve between calls
  int16_t*** rapidBuffers;
  int16_t *  overflow;
  PS5000A_TRIGGER_INFO * triggerInfo;  // Struct to store trigger timestamping info
  int64_t triggerTimeLast;     // Time stamp index tick value of last capture in last group (may be useful for calculating time gap).
  int32_t    isMemAllocated;   // 0 = the above variables point nowhere, 1 = they have been calloc/malloced

  // Info from when data are collected
  uint32_t nCapturesM;  // Number of captures received (smaller if key press stops data taking)
  int32_t nSamplesM;   // Number of samples received, not sure how this can be smaller
  uint32_t timebaseD;   // Desired timebase, timebaseM is measurement timebase, same unless desired was too fast
  uint32_t timebaseM;   // A number from 0 to 2^32-1 (8 bit mode 0=1ns, 1=2ns, 2=4ns, then (n-2)/0.125 (so 3=8ns)),
                        //   12 bit mode is half this, 14 bit mode is the same as this, but 0,1,2 are unavailable
  PICO_STATUS statusBulk;  // Status from the bulk data transfer
  PICO_STATUS statusTrig;  // Status from the trigger info data transfer
  int32_t  timeIntervalNs; // Filled before data collection when time base set
  uint32_t currentFileSize; // Current file size for loop function
  uint32_t currentLoopGroup; // Current group number in loop function
  int32_t countCut2;      //   At end of 'O' command store the number of output events 9for rate calculation)
  uint32_t runNumber;     // 
} NP08VARS;

// Allocate memory 
void NP08AllocateBuffers(UNIT * unit, NP08VARS * np08)
{
  int16_t channel;
  uint32_t capture;
  uint32_t ncap = 1000;   // These are the maximum values allowed for np08->nCaptures
  uint32_t nsam = 2500;   // and np08->nSamples

  if (np08->isMemAllocated) return;   // Already allocated (it is OK to call this to check)
  printf("[Info] Allocating memory buffers\n");
  
  // Allocate memory
  np08->rapidBuffers = (int16_t ***)calloc(unit->channelCount, sizeof(int16_t*));
  np08->overflow = (int16_t *)calloc(unit->channelCount * ncap, sizeof(int16_t));

  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      np08->rapidBuffers[channel] = (int16_t **)calloc(ncap, sizeof(int16_t*));
    }
  }
  
  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      for (capture = 0; capture < ncap; capture++) {
	np08->rapidBuffers[channel][capture] = (int16_t *)calloc(nsam, sizeof(int16_t));
      }
    }
  }

  // Allocate memory for the trigger timestamping
  np08->triggerInfo = (PS5000A_TRIGGER_INFO *)malloc(ncap * sizeof(PS5000A_TRIGGER_INFO));
  
  np08->isMemAllocated = 1;
}

// Frees the memory alloacted by NP08AllocateBuffers()
void NP08FreeBuffers(UNIT * unit, NP08VARS * np08)
{
  int16_t channel;
  uint32_t capture;
  
  if (!np08->isMemAllocated) return;
  printf("[Info] Deallocating memory buffers\n");
  
  // Free memory
  free(np08->overflow);
  
  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      for (capture = 0; capture < np08->nCaptures; capture++) {
	free(np08->rapidBuffers[channel][capture]);
      }
    }
  }
  
  for (channel = 0; channel < unit->channelCount; channel++) {
      if (unit->channelSettings[channel].enabled) {
	free(np08->rapidBuffers[channel]);
      }
  }
  
  free(np08->rapidBuffers);
  free(np08->triggerInfo);

  np08->isMemAllocated = 0;
}

void setNP08Default(UNIT* unit, NP08VARS* np08)
{
  // Currently these are the values from the example collectRapidBlock, eventually we want to make nSamples stretch 10us
  np08->nSegments = 4000;    // Number of segments desired
  np08->nCaptures = 1000;    // Number of captures desired, must be less than nSegments/#active channels
  np08->nSamples = 2500;   // Number of samples to take per capture (i.e. how big is the time window in ticks) 
  np08->nPreSamples = 250;   // Number of samples to take before trigger (must be < nSamples)
  np08->trigChannel = PS5000A_CHANNEL_B;   // Which channel to trigger on
  np08->trigThreshold = -3500;   // Trigger threshold in raw ADC units
  np08->peakThreshold[0] = -2000;   // Ch A
  np08->peakThreshold[1] = -3500;   // Ch B
  np08->peakThreshold[2] = -2000;   // Ch C
  np08->peakThreshold[3] = -5000;   // Ch D
  np08->trigDirection = PS5000A_FALLING;   // Trigger direction, rising, falling etc
  np08->trigAuto_ms = 1000;   // Set >0 for an auto mode scope (number is the number of ms to wait)
  np08->trigUseSimple = 1; // Non-zero means o use the simplified trigger setup
  np08->maxLoopGroups = 3600000;
  np08->maxFileSize = 1000;  // In units of MB
  np08->vetoB = 30;
  np08->vetoC = 10;
  np08->waveOnOff = 0;   // 0=off, 1 = on
  np08->writePeakCount = 2;   // Peak multiplicity (# channels to have at least 1 peak on)

  np08->secondChan = 1;      // Channel B:   Channel number to hunt for second peak
  np08->secondMinDelay = 50; // Minimum delay from first peak to consider (was fixed at 50 ticks)
  np08->writePeakCount = 0;  // (exists, but currently ignored.  Value 0->3 = require peak on this channel to be above np08->writePeakHeight
                             //   Value 11->14 = require the number of channels with peaks to be bigger than this)
  np08->writePeakHeight = -2000;  // Used as threshold on peak to write out (only works if higher than the time threshold, so may be useless)
  np08->cfdOnOff = 0;        // Enables writing CFD values
}

void printNP08Things(UNIT* unit, NP08VARS* np08, FILE * file) {
  char ch;
  
  fprintf(file, " N Number of captures %d\n", np08->nCaptures);
  fprintf(file, " S Number of samples per capture %d\n", np08->nSamples);
  fprintf(file, " P Number of samples in pretrigger %d\n", np08->nPreSamples);
  fprintf(file, " L Number of groups (of captures) in long run %d\n", np08->maxLoopGroups);

  switch (np08->trigChannel) {
  case PS5000A_CHANNEL_A: ch = 'A'; break;
  case PS5000A_CHANNEL_B: ch = 'B'; break;
  case PS5000A_CHANNEL_C: ch = 'C'; break;
  case PS5000A_CHANNEL_D: ch = 'D'; break;
  case PS5000A_EXTERNAL:  ch = 'E'; break;
  default: ch = '?';
  }
  fprintf(file, " T Trigger channel %c\n", ch);

  switch (np08->trigDirection) {
  case PS5000A_ABOVE: ch = 'A'; break;
  case PS5000A_BELOW: ch = 'B'; break;
  case PS5000A_RISING: ch = 'R'; break;
  case PS5000A_FALLING: ch = 'F'; break;
  default: ch = '?';
  }
  fprintf(file, " E Above/Below/Rising/Falling: %c\n", ch);

  fprintf(file, " K Trigger threshold: %d (%dmV)\n", np08->trigThreshold, adc_to_mv(np08->trigThreshold, unit->channelSettings[np08->trigChannel].range, unit));
  
  fprintf(file, "Peak finding settings:\n");
  for (ch = 0; ch < unit->channelCount; ch++) {
    fprintf(file, " %c Peak finding threshold channel %c %d (%dmV)\n", ch + 'A', ch + 'A',
	    np08->peakThreshold[ch], adc_to_mv(np08->peakThreshold[ch], unit->channelSettings[PS5000A_CHANNEL_A + ch].range, unit));
  }
  if (np08->writePeakCount >= 0 && np08->writePeakCount <= 3) {
	  fprintf(file, " M Cut2 selection when peak found on channel %c\n", 'A' + np08->writePeakCount);
  } else {
	  fprintf(file, " M Cut2 selection when at least %d channels active\n", np08->writePeakCount-10);
  }
}


void setNP08Things(UNIT * unit, NP08VARS * np08) {
  int32_t retry;
  int i;
  char ch;
  
  do {
    fflush(stdin);
    printf("\nNP08 setup menu.  Enter character to select item to change:\n");
    printNP08Things(unit, np08, stdout);
    printf(" Z Set all parameters to default\n");
    printf(" X Exit back to main menu\n");
    
    fflush(stdin);
    ch = toupper(_getch());
    switch (ch) {
    case 'N':
      do {
	printf("Give number of captures (e.g. 1000 for long run, 10 for test) [max 1000]: ");
	fflush(stdin);
	scanf_s("%lud", &i);
      } while (i > 1000);
      np08->nCaptures = i;
      np08->nSegments = 4 * np08->nCaptures;  // Need to check where the factor comes in for the channels
      printf("Number of captures set to %d, number of segments set to %d\n", np08->nCaptures, np08->nSegments);
      break;
      
    case 'S':
      do {
	printf("Give number of samples per capture (with 8ns ticks, 2500 gives 20us):");
	fflush(stdin);
	scanf_s("%lud", &i);
      } while (i > 2500);
      np08->nSamples = i;
      break;
      
    case 'P':
      do {
	printf("Give number of samples in presample window (must be below %d, e.g. 10%%):", np08->nSamples);
	fflush(stdin);
	scanf_s("%lud", &np08->nPreSamples);
      } while (np08->nPreSamples >= np08->nSamples);
      printf("Number of samples per capture is %d, number of presamples is %d, postsamples is %d)\n"
	     , np08->nSamples, np08->nPreSamples, np08->nSamples - np08->nPreSamples);
      break;
      
    case 'L':
		printf("Give number of groups to collect in long run (special if negative, will restart)\n");
      fflush(stdin);
      scanf_s("%lud", &np08->maxLoopGroups);
      printf("Number of groups to colect in long run is %d\n", np08->maxLoopGroups);
      break;
      
    case 'T':
      printf("Select the trigger channel: A,B,C,D or E=external input:");
      fflush(stdin);
      retry = 0;
      do {
	ch = toupper(_getch());
	switch (ch) {
	case 'A': np08->trigChannel = PS5000A_CHANNEL_A; printf("Selected channel A\n"); break;
	case 'B': np08->trigChannel = PS5000A_CHANNEL_B; printf("Selected channel B\n"); break;
	case 'C': np08->trigChannel = PS5000A_CHANNEL_C; printf("Selected channel C\n"); break;
	case 'D': np08->trigChannel = PS5000A_CHANNEL_D; printf("Selected channel D\n"); break;
	case 'E': np08->trigChannel = PS5000A_EXTERNAL;  printf("Selected extrnal trigger\n"); break;
	default: printf("Invalid selection, choose A,B,C,D or E:"); retry = 1; break;
	}
      } while (retry);
      break;
      
    case 'E':
      printf("Select the trigger direction A,B,R,F [A]bove, [B]elow, [R]ising, [F]alling:");
      fflush(stdin);
      retry = 0;
      do {
	ch = toupper(_getch());
	printf("\n");
	switch (ch) {
	case 'A': np08->trigDirection = PS5000A_ABOVE;   printf("Selected: Trigger when above threshold\n"); break;
	case 'B': np08->trigDirection = PS5000A_BELOW;   printf("Selected: Trigger when below threshold\n"); break;
	case 'R': np08->trigDirection = PS5000A_RISING;  printf("Selected: Trigger on rising edge\n"); break;
	case 'F': np08->trigDirection = PS5000A_FALLING; printf("Selected: Trigger on falling edge\n"); break;
	default: printf("Invalid selection, choose A,B,R or F:"); retry = 1; break;
	}
      } while (retry);
      break;
      
    case 'K':
      do {
	printf("Give trigger threshold in ADC counts [-32768,32767]:");
	fflush(stdin);
	scanf_s("%hd", &np08->trigThreshold);
      } while (np08->trigThreshold > 32767 || np08->trigThreshold < -32768);
      printf("Trigger threshold set to %d (which is %d in mV I think)\n", np08->trigThreshold
	     , adc_to_mv(np08->trigThreshold, unit->channelSettings[np08->trigChannel].range, unit));
      break;
      
      // Peak threshold channel A,B,C,D
    case 'C':
    case 'D':
      if (unit->channelCount <4) break;    // Disable the C and D commands on 2 channel scope
      // fall through
    case 'A':
    case 'B':
      do {
	i = ch - 'A';
	printf("Give channel %c peak threshold in ADC counts [-32768,32767]:", i + 'A');
	fflush(stdin);
	scanf_s("%hd", &np08->peakThreshold[i]);
      } while (np08->peakThreshold[i] > 32767 || np08->peakThreshold[i] < -32768);
      printf("Trigger threshold set to %d (which is %d in mV I think)\n", np08->peakThreshold[i]
	     , adc_to_mv(np08->peakThreshold[i], unit->channelSettings[PS5000A_CHANNEL_A + i].range, unit));
      break;
    
	case 'M':
		do {
			printf("Channel selection for cut2 (SW cut to write out).\n");
			printf("A, B, C, D = require a pulse on given channel\n");
			printf("1, 2, 3, 4 = require n channels with pulses\n");
			fflush(stdin);
			ch = toupper(_getch());
			switch (ch) {
			case 'A': np08->writePeakCount = PS5000A_CHANNEL_A; printf("Selected channel A\n"); break;
			case 'B': np08->writePeakCount = PS5000A_CHANNEL_B; printf("Selected channel B\n"); break;
			case 'C': np08->writePeakCount = PS5000A_CHANNEL_C; printf("Selected channel C\n"); break;
			case 'D': np08->writePeakCount = PS5000A_CHANNEL_D; printf("Selected channel D\n"); break;
			case '1': np08->writePeakCount = 11; printf("Select if 1 or more channels active\n"); break;
			case '2': np08->writePeakCount = 12; printf("Select if 2 or more channels active\n"); break;
			case '3': np08->writePeakCount = 13; printf("Select if 3 or more channels active\n"); break;
			case '4': np08->writePeakCount = 14; printf("Select if all four channels active\n"); break;
			default: ch = '\0'; break;
			}
		} while (ch == '\0');
	    break;

#if 0
    case 'M':
		printf("Criterion for cut 2 to write to file\n");
      fflush(stdin);
      scanf_s("%lud", &np08->writePeakCount);
      printf("Require %d channels to have at least one peak to write capture to file\n", np08->writePeakCount);
      break;
#endif

    case 'Z':
      printf("Reset all parameters\n");
      setNP08Default(unit, np08);
      break;
      
    case 'X':
      return;
      
    default:
      break;
    }  // End Switch
  } while (1); // End do
}

void setNP08Expert(UNIT * unit, NP08VARS * np08) {
  char ch = 'X';
  do {
    switch(ch) {
      
    case 'Q':
      printf("Give debug-auto-mode setting (0 is off which is best):");
      fflush(stdin);
      scanf_s("%hud", &np08->trigAuto_ms);
      printf("debug-auto-mode set to %d\n", np08->trigAuto_ms);
      break;
      
    case '*':
      printf("Choose trigger setting method 1=simple, 0=complicated (1 is best):");
      fflush(stdin);
      scanf_s("%hud", &np08->trigUseSimple);
      if (np08->trigUseSimple != 0) np08->trigUseSimple = 1;
      printf("Trigget setting methid set to %s\n", np08->trigUseSimple ? "Simple" : "Complicated");
      break;
      
    case 'X':
      return;
      
    }  // End switch
  } while (1);  // End do
}

/****************************************************************
Routine to process the received data
****************************************************************/

#if 0
void NP08ProcessData(UNIT* unit, NP08VARS* np08)
{
  uint32_t capture;
  int16_t channel;
  int32_t i;
  uint64_t timeStampCounterDiff = 0;
  
  if (!np08->isMemAllocated) {
    printf("No data collected\n");
    return;
  }
  
  if (np08->statusBulk != PICO_OK || np08->statusTrig != PICO_OK) {
    printf("Error when collecting data, not analysing it.  Codes are %d %d\n", np08->statusBulk, np08->statusTrig);
    return;
  }
  
  //print first 10 samples from each capture
  for (capture = 0; capture < np08->nCapturesM; capture++) {
    printf("\n");
    
    printf("Capture index %d:-\n\n", capture);
    
    // Trigger Info status & Timestamp 
    printf("Trigger Info:- Status: %u  Trigger index: %u  Timestamp Counter: %I64u\n", np08->triggerInfo[capture].status,
	   np08->triggerInfo[capture].triggerIndex, np08->triggerInfo[capture].timeStampCounter);
    
    // Calculate time between trigger events - the first timestamp is arbitrary so is only used to calculate offsets
    
    // The structure containing the status code with bit flag PICO_DEVICE_TIME_STAMP_RESET will have an arbitrary timeStampCounter value. 
    // This should be the first segment in each run, so in this case segment 0 will be ignored.
    
    if (capture == 0) { printf("\n"); } // Nothing to display
    else if (capture > 0 && np08->triggerInfo[capture].status == PICO_OK) {
      timeStampCounterDiff = np08->triggerInfo[capture].timeStampCounter - np08->triggerInfo[capture - 1].timeStampCounter;
      printf("Time since trigger for last segment: %I64u ns\n\n", (timeStampCounterDiff * (uint64_t)np08->timeIntervalNs));
    }
    else {} // Do nothing
    
    for (channel = 0; channel < unit->channelCount; channel++) {
      if (unit->channelSettings[channel].enabled) { printf("Channel %c:\t", 'A' + channel); }
    }
    printf("\n\n");
    
    for (i = 0; i < 10; i++) {
      for (channel = 0; channel < unit->channelCount; channel++) {
	if (unit->channelSettings[channel].enabled) {
	  printf("   %6d       ", scaleVoltages ?         // scaleVoltages is a global variable
		 adc_to_mv(np08->rapidBuffers[channel][capture][i], unit->channelSettings[PS5000A_CHANNEL_A + channel].range, unit)	// If scaleVoltages, print mV value
		 : np08->rapidBuffers[channel][capture][i]);									// else print ADC Count
	}
      }
      printf("\n");
    }
  }
}
#endif

#if 0
void NP08ProcessData2(UNIT * unit, NP08VARS * np08)
{
  uint32_t capture;
  int16_t channel;
  int32_t i,i0;
  uint64_t timeStampCounterDiff = 0;
  int32_t min[4],max[4],mint[4],maxt[4];
  
  if (!np08->isMemAllocated) { printf("No data collected\n"); return; }
  if (np08->statusBulk != PICO_OK || np08->statusTrig != PICO_OK) {
    printf("Error when collecting data, not analysing it.  Codes are %d %d\n",np08->statusBulk,np08->statusTrig);
    return;
  }
  
  // Print the min/max for each capture (up to max first 20) and then prompt to print out samples from
  // one capture around a certain time index
  for (capture=0; capture<np08->nCapturesM; capture++) {
    for (channel=0; channel<unit->channelCount; channel++) {
      min[channel] = 0;   max[channel] = 0;      // pulse height
      mint[channel] = 0;  maxt[channel] = 0;    // Time index
      if (unit->channelSettings[channel].enabled) {
        min[channel] = np08->rapidBuffers[channel][capture][0]; 
	max[channel] = np08->rapidBuffers[channel][capture][0];  // Initialise with first sample
	for (i=1; i<np08->nSamples; i++) {
	  if (np08->rapidBuffers[channel][capture][i] < min[channel]) { min[channel] = np08->rapidBuffers[channel][capture][i]; mint[channel] = i; }
	  if (np08->rapidBuffers[channel][capture][i] > max[channel]) { max[channel] = np08->rapidBuffers[channel][capture][i]; maxt[channel] = i; }
	}
      }
    }
    printf("%2d min t[raw]mV A:%d[%d]%d B:%d[%d]%d C:%d[%d]%d D:%d[%d]%d max t[raw]mV A:%d[%d]%d B:%d[%d]%d C:%d[%d]%d D:%d[%d]%d\n",
	   capture,
	   mint[0], min[0], adc_to_mv(min[0], unit->channelSettings[PS5000A_CHANNEL_A].range, unit),
	   mint[1], min[1], adc_to_mv(min[1], unit->channelSettings[PS5000A_CHANNEL_B].range, unit),
	   mint[2], min[2], adc_to_mv(min[2], unit->channelSettings[PS5000A_CHANNEL_C].range, unit),
	   mint[3], min[3], adc_to_mv(min[3], unit->channelSettings[PS5000A_CHANNEL_D].range, unit),
	   maxt[0], max[0], adc_to_mv(max[0], unit->channelSettings[PS5000A_CHANNEL_A].range, unit),
	   maxt[1], max[1], adc_to_mv(max[1], unit->channelSettings[PS5000A_CHANNEL_B].range, unit),
	   maxt[2], max[2], adc_to_mv(max[2], unit->channelSettings[PS5000A_CHANNEL_C].range, unit),
	   maxt[3], max[3], adc_to_mv(max[3], unit->channelSettings[PS5000A_CHANNEL_D].range, unit));
    if (capture > 19) { printf("Printing first 20, there are %d captures received\n",np08->nCapturesM); break; }
  }
  
  printf("Print 10 ticks of data on the 4 channels.  Select capture to print:\n");
  fflush(stdin);
  scanf_s("%lud", &capture);
  printf("Capture %d. Select time tick to start print:\n",capture);
  fflush(stdin);
  scanf_s("%lud", &i0);
  if (capture > np08->nCapturesM-1 || i0 > np08->nSamples-11) {
    printf("Invalid entry capture = %d (max %d) tick = %d (max %d)\n",capture,np08->nCapturesM-1,i0,np08->nSamples-11);
    return;
  }
  
  printf("      t       ");
  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) { printf("Channel %c       ", 'A' + channel); }
  }
  printf("\n");
  
  for (i = i0; i < i0+10; i++) {
    printf(" %9d",i);
    for (channel = 0; channel < unit->channelCount; channel++) {
      if (unit->channelSettings[channel].enabled) {
	printf("   %6d/%6d",
	       np08->rapidBuffers[channel][capture][i],
	       adc_to_mv(np08->rapidBuffers[channel][capture][i], unit->channelSettings[PS5000A_CHANNEL_A + channel].range, unit));
      }
    }	
    printf("\n");
  }
  
}
#endif

#if 0
// Write an entry for each peak.  wave=0 don't write the wave info, wave=1 do write the wave info
void NP08PeakFind1(UNIT * unit, NP08VARS * np08)
{
  uint32_t capture;
  int16_t channel;
  int32_t i, k;
  int32_t inpeak;
  
  // May modify these to be arguments to the function call, so we can automate web page write
  //printf("Select capture to find peaks:\n");
  //fflush(stdin);
  //scanf_s("%lud", &capture);
  //printf("Capture %d. Select window size in trace:\n",capture);
  //fflush(stdin);
  //scanf_s("%lud", &tracelen);
  //if (capture > np08->nCapturesM-1 || tracelen > np08->nSamples-11) {
  //  printf("Invalid entry capture = %d (max %d) tracelen = %d (max %d)\n",capture,np08->nCapturesM-1,tracelen,np08->nSamples-11);
  //  return;
  //}

  if (!np08->isMemAllocated) { printf("No data collected\n"); return; }
  if (np08->statusBulk != PICO_OK || np08->statusTrig != PICO_OK) {
    printf("Error when collecting data, not analysing it.  Codes are %d %d\n",np08->statusBulk,np08->statusTrig);
    return;
  }

  for (capture = 0; capture < np08->nCapturesM; capture++) {
    for (channel = 0; channel < unit->channelCount; channel++) {
      if (!unit->channelSettings[channel].enabled) continue;
      inpeak = 0;
      for (i = 0; i < np08->nSamples; i++) {
	if (inpeak > 0) inpeak--;
	if (inpeak > 0) continue;
	if (np08->rapidBuffers[channel][capture][i] > np08->peakThreshold[channel]) continue;
	inpeak = 5;
	printf("%d,%d,%d,%d", np08->currentLoopGroup, capture, channel, i);
	if (np08->waveOnOff) {
	  for (k = i - 5; k < i + 5; k++) {// Include exactly 11 channels in waveform
	    if (k < 0 || k >= np08->nSamples) printf(",0");
	    else printf(",%d", np08->rapidBuffers[channel][capture][k]);
	  }
	}
	printf("\n");
      }  // Sample loop
    }  // Channel loop
  }
}	  
#endif

#if 0
// Write an entry for each capture.  wave=0 don't write the wave info, wave=1 do write the wave info, cntr only write when at least cntr peaks
void NP08PeakFind2(UNIT * unit, NP08VARS * np08, FILE * file)
{
  uint32_t capture;
  int16_t channel;
  int32_t vetoB = 30;  // Number of time ticks around the b1 to disable finding of b2
  int32_t vetoC = 10;   // Number of time ticks around the a.b1 for the c1 to veto
  int32_t coincAB = 5;   // Coincidence time in ticks for A and B to form coincidence
  int32_t i,j,k, k9;

  int index[4];
  int height[4], height1;     // Pulse height
  double interp[4], interp1;  // Interpolated time
  double p1, p2, p0;  // interp contains interpolated time
  int ok[4], okall;
  int close,dist,dist1,inpeak;
  int searchchannel[4] = {1,0,2,1};  // The four searches are on B,A,C,B respectively

  // This routine emulates the hardware way to do this as much as possible,
  // so it doesn't use all the information that is available.
  // In all these, we are defining the time of the pulse as the leading edge time
  //  A & B & notC.  Give the time index of the nearest peak on B to the trigger time
  //                 Give the time index of the nearest peak to B on each of A and C
  //                 If there was no peak found on B, find the nearest time to the
  //                 trigger on A and C.  (This allows us to do a coincidence
  //                 measurement of A and C with B disabled)
  //  Second B       Give the earliest peak time on B from the start excluding a window
  //                 around the triggered B.
  // Reconstructed time: Time difference between first and second pulse on B
  // Flag good event: A&B&notC satisfied
  // If wave is requested, we write four waveforms out, the selected A, B, C and second B.

  if (!np08->isMemAllocated) { printf("No data collected\n"); return; }
  if (np08->statusBulk != PICO_OK || np08->statusTrig != PICO_OK) {
    printf("Error when collecting data, not analysing it.  Codes are %d %d\n",np08->statusBulk,np08->statusTrig);
    return;
  }

  for (capture = 0; capture < np08->nCapturesM; capture++) {
    for (j = 0; j < 4; j++) {    // We do this procedure 4 times with only a few variations, so I made a loop rather than 4 copies   
      index[j] = np08->nPreSamples;    // Initial setup
      height[j] = 0;
      interp[j] = 0.;
      ok[j] = 0;                      // Initial setup
      close = index[0];      // The first search is for closest to trigger time, the later searches are for closest to the b1
      
      // Now prepare to find the peaks
      inpeak = 0;
      dist = np08->nSamples * 2;   // How close are we, start way out.
      channel = searchchannel[j];    // Look on channel B,A,C,B for the four searches.
      for (i = 1; i < np08->nSamples; i++) {    // start at 1, not 0
	if (!unit->channelSettings[channel].enabled) break;   // Don't find any peaks if channel is disabled.
	if (inpeak > 0) inpeak--;
	if (inpeak > 0) continue;     // Skip if we are close to previous peak
	p0 = np08->peakThreshold[channel];                   // Threshold, = desirred ADC value to interpolate to
	p1 = np08->rapidBuffers[channel][capture][i - 1];    // ADC value of previous
	p2 = np08->rapidBuffers[channel][capture][i];        // ADC value of next
	if (np08->rapidBuffers[channel][capture][i] > np08->peakThreshold[channel]) continue;  // Exit if pulse too small [It is if (p2 > p0) but in integer]
	if (np08->rapidBuffers[channel][capture][i - 1] <= np08->peakThreshold[channel]) continue;  // Exit if not leading edge
	p2 = p2 - p1;
	if (p2 == 0.) p2 = 0.1;  // Avoid divide by zero in interpolation
	interp1 = (p0 - p1) / p2 + i;
	
	// Search onward to find peak height, a maximum of 10 samples
	k9 = i + 10;
	if (k9 > np08->nSamples) k9 = np08->nSamples;
	height1 = np08->rapidBuffers[channel][capture][i];
	for (k=i+1; k<k9; k++) {    // In this comparison, remember the peaks are negaive, i.e. '<' means 'higher amplitude'
	  if (np08->rapidBuffers[channel][capture][k] < height1) height1 = np08->rapidBuffers[channel][capture][k];
	  else break;    // Stop looking for peak as soon as it starts dipping down
	}
	
	// Above threshold
	inpeak = 5;
	dist1 = i - close;
	if (dist1 < 0) dist1 = -dist1;  // abs(dist1)
	if (j == 3 && dist1 < vetoB) continue;     // Fourth time, we want to veto pulses close to the main triggered B, so skip if in veto window
	if (dist1 < dist) { index[j] = i; interp[j] = interp1;  height[j] = height1;  ok[j] = 1; dist = dist1; }    // Closer than others, accept
      }
      // At this point after first time through loop, index[0] contains the index of the closest and ok=1.
      // If there was no pulse, index[0] is the trigger time and ok[0]=0
    }
    
    okall = 0;                               // It is bad trigger unless it passes all the following
    do {     // This never loops round, see comment about C++ trick at end
      if (!ok[0]) continue;                  // No B1 pulse
      if (!ok[1]) continue;                  // No A1 pulse
      dist1 = index[0] - index[1];
      if (dist1 < 0) dist1 = -dist1;
      if (dist1 > coincAB) continue;         // AB coincidence failed
      dist1 = index[0] - index[2];
      if (dist1 < 0) dist1 = -dist1;
      if (ok[2] && dist1 < vetoC) continue;  // C pulse exists and is close to AB, so veto
      okall = 1;                             // We made it to the end, it is a good trigger
    } while (0);   // C++ trick.  The loop here just defines a section we can jump out of with continue.  It never loops round   
    
    // Write out the info in the line.
    if (ok[0] + ok[1] + ok[2] + ok[3] >= np08->writePeakCount) {
      np08->currentFileSize += fprintf(file, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%6.2lf,%6.2lf,%6.2lf,%6.2lf,%d,%d,%d,%d", np08->currentLoopGroup, capture, okall, ok[0], ok[1], ok[2], ok[3], 
				       index[0], index[1], index[2], index[3], interp[0], interp[1], interp[2], interp[3], height[0], height[1], height[2], height[3]);
      if (np08->waveOnOff) {
	for (j = 0; j < 4; j++) {    // Write out the waveforms around the four signals, each fixed to 11 samples
	  channel = searchchannel[j];    // Look on channel B,A,C,B for the four searches.
	  for (k = index[j] - 7; k < index[j] + 7; k++) {// Include exactly 15 channels in waveform
	    if (k < 0 || k >= np08->nSamples) np08->currentFileSize += fprintf(file, ",0");
	    else np08->currentFileSize += fprintf(file, ",%d", np08->rapidBuffers[channel][capture][k]);
	  }
	}
      }
      np08->currentFileSize += fprintf(file, "\n");  // Finally end the line
    }
  }
}
#endif

// NP08PeakFind4:  This is almost the same as NP08PeakFind2, both of them loop to find the peak on B (B1) and then 
// the nearby peaks A1 and C1 and then to find a second peak on B (B2).  NP08PeakFind4 has two additional attempts to
// find a peak on B (B3 and B4) and also has a go at looking for any 'clairvoyant' peaks on B
// B3 = find crossing points at all locations where the B waveform crosses the desired threshold for channel B, 
//      look backwards for up to 10 slices for the lowest waveform value and look forward for up to 10 slices for the
//      highest waveform value and take the peak for which this difference (high-low) is the largest.  The aim is to 
//      avoid peaks on the falling B1 pulse.   The 10 slice parameter is in int32_t np08->rangeB3
// B4 = Like B3, but require that there is a longish interval in front of the pulse where the waveform is 
//      below the threshold.  The Longish interval is in variable int32_t np08->rangeB4
// Clairvoyant peak:  Sometimes we see a huge B1 pulse about 100ns behind the A and C pulses in a through going muon.
// However we also see a small peak on B about 100ns earlier 'clairvoyance', it can predict the future.  Or maybe the
// light has just hit a later dynode and skipped the first one.  For this monitoring to work, these peaks need to be below
// the selection threshold so they do not count as the main B1.  We look in front of the B1 peak a certain distance and
// look for the largest peak that is earlier.
// The data record for NP08PeakFinder2 is 
// #g,#c,allok,4x(flags if found peaks B1,A1,C1,B2), 4x(time bins where found), 4x(interpolated time), 4x(peak height)
// and the data record for NP08PeakFinder4 is additionally
// 4x(flags if found peaks B3,B4,B5,B6), 4x(time bins where found), 4x(interpolated time), 4x(peak height), 4x(end-index)
// The end-index is the index where the pulse goes back down below the threshold.
// Also, the group number for the first event in the file is set to 10000 + the real group number (usually 0, so first 
// g is usually 10000), this is done in the loop program that calls this one. 

// Write an entry for each capture.  wave=0 don't write the wave info, wave=1 do write the wave info, cntr only write when at least cntr peaks
void NP08PeakFind4(UNIT* unit, NP08VARS* np08, FILE* file)
{
  uint32_t capture;
  int16_t channel;
  int32_t vetoB = 30;  // Number of time ticks around the b1 to disable finding of b2
  int32_t vetoC = 10;   // Number of time ticks around the a.b1 for the c1 to veto
  int32_t coincAB = 5;   // Coincidence time in ticks for A and B to form coincidence
  int32_t i, j, k, k9;
  
  int index[4];
  int height[4], height1;     // Pulse height
  double interp[4], interp1;  // Interpolated time
  double p1, p2, p0;  // interp contains interpolated time
  int ok[4], okall;
  int close, dist, dist1, inpeak;
  int searchchannel[4] = { 1,0,2,1 };  // The four searches are on B,A,C,B respectively
  
  // Info for the peaks B3,B4,B5,B6 These variables all start with ex_ (for 'extra')
  int ex_ok[4];
  int ex_index[4];
  int ex_height[4];
  int ex_base[4], base1;  // amplitude at base (lowest amplitude from the 20 samples before threshold)
  int ex_hb[4], hb1, hb2, ihb2;   // hb = height - base.  This is the metric for picking the peaks to write
  int ex_endindex[4], end1;
  double ex_interp[4];
  int j1;
  int channelt;    // Special channel number to pick threshold for B3,B4,B5,B6.  If this works, make it a separate parameter
  
  // This routine emulates the hardware way to do this as much as possible,
  // so it doesn't use all the information that is available.
  // In all these, we are defining the time of the pulse as the leading edge time
  //  A & B & notC.  Give the time index of the nearest peak on B to the trigger time
  //                 Give the time index of the nearest peak to B on each of A and C
  //                 If there was no peak found on B, find the nearest time to the
  //                 trigger on A and C.  (This allows us to do a coincidence
  //                 measurement of A and C with B disabled)
  //  Second B       Give the earliest peak time on B from the start excluding a window
  //                 around the triggered B.
  // Reconstructed time: Time difference between first and second pulse on B
  // Flag good event: A&B&notC satisfied
  // If wave is requested, we write four waveforms out, the selected A, B, C and second B.
  
  if (!np08->isMemAllocated) { printf("No data collected\n"); return; }
  if (np08->statusBulk != PICO_OK || np08->statusTrig != PICO_OK) {
    printf("Error when collecting data, not analysing it.  Codes are %d %d\n", np08->statusBulk, np08->statusTrig);
    return;
  }
  
  // This section finds the B3, B4, B5 and B6 pulses.  It acts indpendently of the section finding the B1, A1, C1 abd B2 pulses,
  // i.e. there are no times or pulse heights used in one that are needed in the other of thesse two algorithms.
  // The fours pulses found here are essentially unordered.  
  // However, there is a section lower down that swaps the most favoured pulse into the B3 position.  It uses the time of B1 for that.
  for (capture = 0; capture < np08->nCapturesM; capture++) {
    // Loop through finding the ex pulses one by one and keep the top four
    for (j = 0; j < 4; j++) {  // First zero everything
      ex_ok[j] = 0;
      ex_index[j] = 0;
      ex_height[j] = 0;
      ex_interp[j] = 0.;
      ex_base[j] = 0;
      ex_endindex[j] = 0;
      ex_hb[j] = 0;
    }
    // Now identify each time it crosses the threshold in the increasing direction
    channel = 1;   // All ex_ peaks are found on CHANNEL_B
    channelt = 1;  // (24.10.2020 Turn this trial off). Normally this is the same as channel; can set it to 3 to use the channel D threshold to indentify B3,B4,B5,B6 peaks
    for (i = 1; i < np08->nSamples; i++) {    // start at 1, not 0
      if (!unit->channelSettings[channel].enabled) break;   // Don't find any peaks if channel is disabled.
      p0 = np08->peakThreshold[channelt];                   // Threshold, = desirred ADC value to interpolate to
      p1 = np08->rapidBuffers[channel][capture][i - 1];    // ADC value of previous
      p2 = np08->rapidBuffers[channel][capture][i];        // ADC value of next
      if (np08->rapidBuffers[channel][capture][i] > np08->peakThreshold[channelt]) continue;  // Exit if pulse too small [It is if (p2 > p0) but in integer]
      if (np08->rapidBuffers[channel][capture][i - 1] <= np08->peakThreshold[channelt]) continue;  // Exit if not leading edge
      p2 = p2 - p1;
      if (p2 == 0.) p2 = 0.1;  // Avoid divide by zero in interpolation
      interp1 = (p0 - p1) / p2 + i;
      
      // Search onward to find peak height, a maximum of 10 samples
      k9 = i + 10;
      if (k9 > np08->nSamples) k9 = np08->nSamples;
      height1 = np08->rapidBuffers[channel][capture][i];
      for (k = i + 1; k < k9; k++) {    // In this comparison, remember the peaks are negaive, i.e. '<' means 'higher amplitude'
	if (np08->rapidBuffers[channel][capture][k] < height1) height1 = np08->rapidBuffers[channel][capture][k];
	else break;    // Stop looking for peak as soon as it starts dipping down
      }
      
      // Search back to find the base, it is the lowest amplitude tick among the previous 20
      k9 = i - 20;
      if (k9 < 0) k9 = 0;  // Don't go beyond start of buffer
      base1 = np08->rapidBuffers[channel][capture][i];
      for (k = k9; k < i; k++) {
	if (np08->rapidBuffers[channel][capture][k] > base1) base1 = np08->rapidBuffers[channel][capture][k];
      }
      hb1 = -(height1 - base1);    // Postive number, the bigger number is the bigger pulse
      
      // Search forward to find the next place the pulse goes back below threshold
      end1 = np08->nSamples;
      for (k = i + 1; k < np08->nSamples; k++) {
	end1 = k;
	if (np08->rapidBuffers[channel][capture][k] > np08->peakThreshold[channel]) break;   // Amplitude went back below threshold
      }
      
      // If our new pulse is bigger than the smallest of the four we have already, replace it
      hb2 = ex_hb[0];
      ihb2 = 0;  // Index of smallest
      for (j = 1; j < 4; j++) {
	if (ex_hb[j] < hb2) { hb2 = ex_hb[j]; ihb2 = j; }
      }
      // printf("DEBUG: capture %d hb1 %d hb2 %d ihb2 %d height1 %d index %d ex_hb[0..3] %d,%d,%d,%d\n", capture, hb1, hb2, ihb2,height1,i,ex_hb[0],ex_hb[1],ex_hb[2],ex_hb[3]);
      
      if (hb1 > hb2) {     // New peak has larger hb than previous, replace it
	ex_ok[ihb2] = 1;  
	ex_index[ihb2] = i; 
	ex_interp[ihb2] = interp1; 
	ex_height[ihb2] = height1; 
	ex_base[ihb2] = base1;
	ex_hb[ihb2] = hb1;
	ex_endindex[ihb2] = end1;
      }
      
    }
    // ---------- End extra (new) loops    ------------------------------
    
    // This section finds the B1, A1, C1 and B2 pulses
    for (j = 0; j < 4; j++) {    // We do this procedure 4 times with only a few variations, so I made a loop rather than 4 copies   
      index[j] = np08->nPreSamples;    // Initial setup
      height[j] = 0;
      interp[j] = 0.;
      ok[j] = 0;                      // Initial setup
      close = index[0];      // The first search is for closest to trigger time, the later searches are for closest to the b1
      
      // Now prepare to find the peaks
      inpeak = 0;
      dist = np08->nSamples * 2;   // How close are we, start way out.
      channel = searchchannel[j];    // Look on channel B,A,C,B for the four searches.
      for (i = 1; i < np08->nSamples; i++) {    // start at 1, not 0
        if (!(channel < unit->channelCount)) break;           // Don't find any peaks if channel is disabled.
	if (!unit->channelSettings[channel].enabled) break;   // Don't find any peaks if channel is disabled.
	if (inpeak > 0) inpeak--;
	if (inpeak > 0) continue;     // Skip if we are close to previous peak
	p0 = np08->peakThreshold[channel];                   // Threshold, = desirred ADC value to interpolate to
	p1 = np08->rapidBuffers[channel][capture][i - 1];    // ADC value of previous
	p2 = np08->rapidBuffers[channel][capture][i];        // ADC value of next
	if (np08->rapidBuffers[channel][capture][i] > np08->peakThreshold[channel]) continue;  // Exit if pulse too small [It is if (p2 > p0) but in integer]
	if (np08->rapidBuffers[channel][capture][i - 1] <= np08->peakThreshold[channel]) continue;  // Exit if not leading edge
	p2 = p2 - p1;
	if (p2 == 0.) p2 = 0.1;  // Avoid divide by zero in interpolation
	interp1 = (p0 - p1) / p2 + i;
	
	// Search onward to find peak height, a maximum of 10 samples
	k9 = i + 10;
	if (k9 > np08->nSamples) k9 = np08->nSamples;
	height1 = np08->rapidBuffers[channel][capture][i];
	for (k = i + 1; k < k9; k++) {    // In this comparison, remember the peaks are negaive, i.e. '<' means 'higher amplitude'
	  if (np08->rapidBuffers[channel][capture][k] < height1) height1 = np08->rapidBuffers[channel][capture][k];
	  else break;    // Stop looking for peak as soon as it starts dipping down
	}
	
	// Above threshold
	inpeak = 5;
	dist1 = i - close;
	if (dist1 < 0) dist1 = -dist1;  // abs(dist1)
	if (j == 3 && dist1 < vetoB) continue;     // Fourth time, we want to veto pulses close to the main triggered B, so skip if in veto window
	if (dist1 < dist) { index[j] = i; interp[j] = interp1;  height[j] = height1;  ok[j] = 1; dist = dist1; }    // Closer than others, accept
      }
      // At this point after first time through loop, index[0] contains the index of the closest and ok=1.
      // If there was no pulse, index[0] is the trigger time and ok[0]=0
    }
    
    // This little section helps the analysis clode by finding the preferred pulse from among B3,B4,B5,B6 (the ones in the ex_* variables here)
    // Since the ex_ list was essentially unordered, nothing is lost, but it allows a decent analysis to hopefully be done by using B3 and ignoring the others.
    
    hb1 = 0;     // These are the ones that should work.Biggest peak - base when skipping to tick 300
    j1 = -3;	
    for (j = 0; j < 4; j++) {
      if (ex_index[j] == 0) continue;
      if (ex_index[j] - index[0] < 50) continue;      // (timeThisPulse - timeB1) < 50 Attempt to chop out the big pulse after an early pulse
      if (ex_hb[j] > hb1) {             // ex_hb More positive = bigger pulse
	hb1 = ex_hb[j];
	j1 = j;
      }
    }
    
    // j1 is the preferred pulse, swap it with the B3 one
    if (j1 > 0) {    // Not -3 (no preferred found) or 0 (preferred is already in B3 position)
      int tmp;
      double dmp;
      tmp = ex_ok[j1];       ex_ok[j1]       = ex_ok[0];       ex_ok[0]       = tmp;
      tmp = ex_index[j1];    ex_index[j1]    = ex_index[0];    ex_index[0]    = tmp;
      tmp = ex_height[j1];   ex_height[j1]   = ex_height[0];   ex_height[0]   = tmp;
      dmp = ex_interp[j1];   ex_interp[j1]   = ex_interp[0];   ex_interp[0]   = dmp;
      tmp = ex_base[j1];     ex_base[j1]     = ex_base[0];     ex_base[0]     = tmp;
      tmp = ex_endindex[j1]; ex_endindex[j1] = ex_endindex[0]; ex_endindex[0] = tmp;
      tmp = ex_hb[j1];       ex_hb[j1]       = ex_hb[0];       ex_hb[0]       = tmp;
    }
    
    // It is possible there is no preferred, but that the B3 pulse exists, set it's OK value to 2 to flag it is not preferred
    if (j1 == -3 && ex_ok[0] == 1) ex_ok[0] = 2;
    
    okall = 0;                               // It is bad trigger unless it passes all the following
    do {     // This never loops round, see comment about C++ trick at end of this loop
      if (!ok[0]) continue;                  // No B1 pulse
      if (!ok[1]) continue;                  // No A1 pulse
      dist1 = index[0] - index[1];
      if (dist1 < 0) dist1 = -dist1;
      if (dist1 > coincAB) continue;         // AB coincidence failed
      dist1 = index[0] - index[2];
      if (dist1 < 0) dist1 = -dist1;
      if (ok[2] && dist1 < vetoC) continue;  // C pulse exists and is close to AB, so veto
      okall = 1;                             // We made it to the end, it is a good trigger
    } while (0);   // C++ trick.  The loop here just defines a section we can jump out of with continue.  It never loops round   
    
    // Write out the info in the line.
    //if (ok[0] + ok[1] + ok[2] + ok[3] >= np08->writePeakCount) {
    if (ok[0] != 0 && ok[1] != 0) {
      // Add the info that is the same as in NP08FindPeak2() first [So the start of the line is the same format]
      np08->currentFileSize += fprintf(file, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%6.2lf,%6.2lf,%6.2lf,%6.2lf,%d,%d,%d,%d", np08->currentLoopGroup, capture, okall, ok[0], ok[1], ok[2], ok[3],
				       index[0], index[1], index[2], index[3], interp[0], interp[1], interp[2], interp[3], height[0], height[1], height[2], height[3]);
      // Now add the ex_things
      np08->currentFileSize += fprintf(file, ",%d,%d,%d,%d,%d,%d,%d,%d,%6.2lf,%6.2lf,%6.2lf,%6.2lf,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", ex_ok[0], ex_ok[1], ex_ok[2], ex_ok[3],
				       ex_index[0], ex_index[1], ex_index[2], ex_index[3], ex_interp[0], ex_interp[1], ex_interp[2], ex_interp[3], ex_height[0], ex_height[1], ex_height[2], ex_height[3],
				       ex_base[0], ex_base[1], ex_base[2], ex_base[3], ex_endindex[0], ex_endindex[1], ex_endindex[2], ex_endindex[3]);
      if (np08->waveOnOff) {
	for (j = 0; j < 4; j++) {    // Write out the waveforms around the four signals, each fixed to 11 samples
	  channel = searchchannel[j];    // Look on channel B,A,C,B for the four searches.
	  for (k = index[j] - 7; k < index[j] + 7; k++) {// Include exactly 15 channels in waveform
	    if (k < 0 || k >= np08->nSamples) np08->currentFileSize += fprintf(file, ",0");
	    else np08->currentFileSize += fprintf(file, ",%d", np08->rapidBuffers[channel][capture][k]);
	  }
	}
      }
      np08->currentFileSize += fprintf(file, "\n");  // Finally end the line
    }
  }
}

// NP08PeakFind5:  This is almost the same as NP08PeakFind4
// Find the extra peaks on the channel selected by np08->secondChan, disabled if secondChan=-1 
//                                                                       [was fixed to channel 1, i.e. B]
// Find the principle peak on each of the four channels, closest to trigger time [was just 
//               the first three, and involved an extra search on channel B, and found nearest the first B found]
// Sort the extra peaks, choose main candidate (largest at least np08->secondMinDelay ticks away) [was fixed to 50 ticks]
// Calculate the ok[] values to decide whether to read out (enhanced selection modes)
// Add CFD if requested [New]
// Add waves if requested 

// The data record for NP08PeakFinder4 is 
// #g,#c,allok,4x(flags if found peaks B1,A1,C1,B2), 4x(time bins where found), 4x(interpolated time), 4x(peak height)
// followed by
// 4x(flags if found peaks B3,B4,B5,B6), 4x(time bins where found), 4x(interpolated time), 4x(peak height), 4x(end-index)

// The data record for NP08PeakFinder5 is:      ABCD for 4 channel scope, AB for 2channel scope
// #g,#c,allok,[A1,B1,C1,D1]*(flag-if-peak-found,interpolated-time,peak-height       )
//      if np08->cfdOnOff is set, bracket also contains (    ,time-bin-of-peak,cfd-front,cfd-back,end-time)
// If np08->secondChan is not -1, i.e. second peak enabled => a fifth peak record is added for this
// If waveOnOff is on, it writes 15 channels of waveform for each of the 2,4 or 5 peaks  

void NP08PeakFind5(UNIT* unit, NP08VARS* np08, FILE* file)
{
  uint32_t capture;
  int16_t channel;
  int32_t vetoB = 30;  // Number of time ticks around the b1 to disable finding of b2
  int32_t vetoC = 10;   // Number of time ticks around the a.b1 for the c1 to veto
  int32_t coincAB = 5;   // Coincidence time in ticks for A and B to form coincidence
  int32_t i, j, k, k9;
  
  int index[4];
  int height[4], height1;     // Pulse height
  double interp[4], interp1;  // Interpolated time
  double p1, p2, p0;  // interp contains interpolated time
  int ok[4], okall;
  int close, dist, dist1, inpeak;
  
  // Info for the peaks B3,B4,B5,B6 These variables all start with ex_ (for 'extra')
  int ex_ok[4];
  int ex_index[4];
  int ex_height[4];
  int ex_base[4], base1;  // amplitude at base (lowest amplitude from the 20 samples before threshold)
  int ex_hb[4], hb1, hb2, ihb2;   // hb = height - base.  This is the metric for picking the peaks to write
  int ex_endindex[4], end1;
  double ex_interp[4];
  int j1;
  
  // Parameters:
  //     np08->secondChan         Channel number to hunt for second peak
  //     np08->secondMinDelay     Minimum delay from first peak to consider (was fixed at 50 ticks)
  //     np08->writePeakCount     (exists, but currently ignored.  Value 0->3 = require peak on this channel to be above np08->writePeakHeight
  //                                 Value 11->14 = require the number of channels with peaks to be bigger than this)
  //     np08->writePeakHeight    Used as threshold on peak to write out (only works if higher than the time threshold, so may be useless)
  //     np08->cfdOnOff           Enables writing CFD values
  //     np08->waveOnOff          Enables writing wave samples

  if (!np08->isMemAllocated) { printf("No data collected\n"); return; }
  if (np08->statusBulk != PICO_OK || np08->statusTrig != PICO_OK) {
    printf("Error when collecting data, not analysing it.  Codes are %d %d\n", np08->statusBulk, np08->statusTrig);
    return;
  }

  int32_t countCut2 = 0;
  for (capture = 0; capture < np08->nCapturesM; capture++) {
  
    // This section finds the B3, B4, B5 and B6 pulses.  It acts indpendently of the section finding the B1, A1, C1 etc pulses,
    // i.e. there are no times or pulse heights used in one that are needed in the other of thesse two algorithms.
    // The four pulses found here are essentially unordered.   However, there is a section lower down that picks the 
    // most favoured pulse using the same algorithm as PeakFinder4 used in 2020 & TT2021 for the B3 pulse.
    
    // Loop through finding the ex pulses one by one and keep the top four
    for (j = 0; j < 4; j++) {  // First zero everything
      ex_ok[j] = 0;
      ex_index[j] = 0;
      ex_height[j] = 0;
      ex_interp[j] = 0.;
      ex_base[j] = 0;
      ex_endindex[j] = 0;
      ex_hb[j] = 0;
    }

    // Now identify each time it crosses the threshold in the increasing direction 
    channel = np08->secondChan;   // Was fixed to CHANNEL_B
    if ((int)channel >= (int)0 && (int)channel < (int)unit->channelCount) {     // Value of -1 or too big disables secondary peak finding
      for (i = 1; i < np08->nSamples; i++) {    // start at 1, not 0

		if (!unit->channelSettings[channel].enabled) break;   // Don't find any peaks if channel is disabled.
		p0 = np08->peakThreshold[channel];                   // Threshold, = desired ADC value to interpolate to
		p1 = np08->rapidBuffers[channel][capture][i - 1];    // ADC value of previous
		p2 = np08->rapidBuffers[channel][capture][i];        // ADC value of next
		if (np08->rapidBuffers[channel][capture][i] > np08->peakThreshold[channel]) continue;  // Exit if pulse too small [It is if (p2 > p0) but in integer]
		if (np08->rapidBuffers[channel][capture][i - 1] <= np08->peakThreshold[channel]) continue;  // Exit if not leading edge
		p2 = p2 - p1;
		if (p2 == 0.) p2 = 0.1;  // Avoid divide by zero in interpolation
		interp1 = (p0 - p1) / p2 + i;
	
		// Search onward to find peak height, a maximum of 10 samples
		k9 = i + 10;
		if (k9 > np08->nSamples) k9 = np08->nSamples;
		height1 = np08->rapidBuffers[channel][capture][i];
		for (k = i + 1; k < k9; k++) {    // In this comparison, remember the peaks are negaive, i.e. '<' means 'higher amplitude'
			if (np08->rapidBuffers[channel][capture][k] < height1) height1 = np08->rapidBuffers[channel][capture][k];
			else break;    // Stop looking for peak as soon as it starts dipping down
		}
	
		// Search back to find the base, it is the lowest amplitude tick among the previous 20
		k9 = i - 20;
		if (k9 < 0) k9 = 0;  // Don't go beyond start of buffer
		base1 = np08->rapidBuffers[channel][capture][i];
		for (k = k9; k < i; k++) {
			if (np08->rapidBuffers[channel][capture][k] > base1) base1 = np08->rapidBuffers[channel][capture][k];
		}
		hb1 = -(height1 - base1);    // Postive number, the bigger number is the bigger pulse
	
		// Search forward to find the next place the pulse goes back below threshold
		end1 = np08->nSamples;
		for (k = i + 1; k < np08->nSamples; k++) {
			end1 = k;
			if (np08->rapidBuffers[channel][capture][k] > np08->peakThreshold[channel]) break;   // Amplitude went back below threshold
		}
	
		// If our new pulse is bigger than the smallest of the four we have already, replace it
		hb2 = ex_hb[0];
		ihb2 = 0;  // Index of smallest
		for (j = 1; j < 4; j++) {
			if (ex_hb[j] < hb2) { hb2 = ex_hb[j]; ihb2 = j; }
		}
		//printf("DEBUG: capture %d hb1 %d hb2 %d ihb2 %d height1 %d index %d ex_hb[0..3] %d,%d,%d,%d\n", capture, hb1, hb2, ihb2,height1,i,ex_hb[0],ex_hb[1],ex_hb[2],ex_hb[3]);
	
		if (hb1 > hb2) {     // New peak has larger hb than previous, replace it
			ex_ok[ihb2] = 1;  
			ex_index[ihb2] = i; 
			ex_interp[ihb2] = interp1; 
			ex_height[ihb2] = height1; 
			ex_base[ihb2] = base1;
			ex_hb[ihb2] = hb1;
			ex_endindex[ihb2] = end1;
		}
	  }  // End of loop over samples
    }    // End if we have enabled extra peak finding

    // This section finds the A! B1 C1 D1 pulses (was B1, A1, C1 and B2 pulses (B2 was useless))
    for (j = 0; j < unit->channelCount; j++) {    // We do this procedure on each channel
      index[j] = np08->nPreSamples;    // Initial setup
      height[j] = 0;
      interp[j] = 0.;
      ok[j] = 0;                       // Initial setup
      close = index[j];                // Start searching from the trigger time     
      
      // Now prepare to find the peaks
      inpeak = 0;
      dist = np08->nSamples * 2;   // How close are we, start way out.
      channel = j;                 // Was = searchchannel[j]; to look on channel B,A,C,B for the four searches.
      for (i = 1; i < np08->nSamples; i++) {    // start at 1, not 0
        if (!(channel < unit->channelCount)) break;           // Don't find any peaks if channel is disabled.
	if (!unit->channelSettings[channel].enabled) break;   // Don't find any peaks if channel is disabled.
	if (inpeak > 0) inpeak--;     // Count down as we move away from previous peak, to ensure a gap
	if (inpeak > 0) continue;     // Skip if we are close to previous peak
	p0 = np08->peakThreshold[channel];                   // Threshold, = desired ADC value to interpolate to
	p1 = np08->rapidBuffers[channel][capture][i - 1];    // ADC value of previous
	p2 = np08->rapidBuffers[channel][capture][i];        // ADC value of next
	if (np08->rapidBuffers[channel][capture][i]      > np08->peakThreshold[channel]) continue;  // Exit if pulse too small [It is if (p2 > p0) but in integer]
	if (np08->rapidBuffers[channel][capture][i - 1] <= np08->peakThreshold[channel]) continue;  // Exit if not leading edge
	p2 = p2 - p1;
	if (p2 == 0.) p2 = 0.1;  // Avoid divide by zero in interpolation
	interp1 = (p0 - p1) / p2 + i;
	
	// Search onward to find peak height, a maximum of 10 samples
	k9 = i + 10;
	if (k9 > np08->nSamples) k9 = np08->nSamples;
	height1 = np08->rapidBuffers[channel][capture][i];
	for (k = i + 1; k < k9; k++) {    // In this comparison, remember the peaks are negaive, i.e. '<' means 'higher amplitude'
	  if (np08->rapidBuffers[channel][capture][k] < height1) height1 = np08->rapidBuffers[channel][capture][k];
	  else break;    // Stop looking for peak as soon as it starts dipping down
	}
	
        // TODO:  Insert CFD in here

	// Above threshold
	inpeak = 5;
	dist1 = i - close;
	if (dist1 < 0) dist1 = -dist1;  // abs(dist1)
	if (dist1 < dist) { index[j] = i; interp[j] = interp1;  height[j] = height1;  ok[j] = 1; dist = dist1; }    // Closer than others, accept
      }  // End of loop over sample times
    }   // End of loop over scope channels
    
    // This little section helps the analysis clode by finding the preferred pulse from among B3,B4,B5,B6 (the ones in the ex_* variables here)
    // Since the ex_ list was essentially unordered, nothing is lost, but it allows a decent analysis to hopefully be done by using B3 and ignoring the others.
    
    hb1 = 0;     // These are the ones that should work.  Biggest peak - base when skipping to tick 300
    j1 = -3;	
    for (j = 0; j < 4; j++) {   // Loop over the four extra pulses found
		if (ex_index[j] == 0) continue;
		if (ex_index[j] - index[0] < np08->secondMinDelay) continue;      // (timeThisPulse - timeB1) < 50 Attempt to chop out the big pulse after an early pulse
		if (ex_hb[j] > hb1) {             // ex_hb More positive = bigger pulse
			hb1 = ex_hb[j];
			j1 = j;
		}
    }

    if (j1 == -3) {    // No preferred peak found
      ex_ok[0]=0; ex_index[0]=0; ex_height[0]=0; ex_interp[0]=0; ex_base[0]=0; ex_endindex[0]=0; ex_hb[0]=0;
    } else if (j1 == 0) {    // Nothing to do, the preferred on is in position 0 already
    } else {           // Move the preferred one to position 0
      ex_ok[0]=ex_ok[j1]; ex_index[0]=ex_index[j1]; ex_height[0]=ex_height[j1]; ex_interp[0]=ex_interp[j1]; 
      ex_base[0]=ex_base[j1]; ex_endindex[0]=ex_endindex[j1]; ex_hb[0]=ex_hb[j1];
    }

    // TODO   Insert the CFD for the extra pulse here

    // Decide whether to write this one out
    okall = 0;                               // It is bad trigger unless it passes all the following
    if (np08->writePeakCount >=0 && np08->writePeakCount < unit->channelCount) {
      if (ok[np08->writePeakCount]) okall = 1;
    } else if (np08->writePeakCount >=10 && np08->writePeakCount < 14) {
      if (ok[0]+ok[1]+ok[2]+ok[3] >= np08->writePeakCount-10) okall = 1;
    }
    
    if (okall) {

      //TODO We can implemnt the SW threshold trigger on one channel (writeCoun = 1->4) right at the start - zip through the samples of the selected channel anc hint for one big enough, then skip all the processing in the event if not satisfied.
      // TODO write the new format, this is the old one still

      // Add the info that is the same as in NP08FindPeak2() first [So the start of the line is the same format]
      np08->currentFileSize += fprintf(file, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%6.2lf,%6.2lf,%6.2lf,%6.2lf,%d,%d,%d,%d", np08->currentLoopGroup, capture, okall, ok[0], ok[1], ok[2], ok[3],
				       index[0], index[1], index[2], index[3], interp[0], interp[1], interp[2], interp[3], height[0], height[1], height[2], height[3]);
      // Now add the ex_things
      np08->currentFileSize += fprintf(file, ",%d,%d,%d,%d,%d,%d,%d,%d,%6.2lf,%6.2lf,%6.2lf,%6.2lf,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", ex_ok[0], ex_ok[1], ex_ok[2], ex_ok[3],
				       ex_index[0], ex_index[1], ex_index[2], ex_index[3], ex_interp[0], ex_interp[1], ex_interp[2], ex_interp[3], ex_height[0], ex_height[1], ex_height[2], ex_height[3],
				       ex_base[0], ex_base[1], ex_base[2], ex_base[3], ex_endindex[0], ex_endindex[1], ex_endindex[2], ex_endindex[3]);
	  countCut2++;
      if (np08->waveOnOff) {
	for (j = 0; j < unit->channelCount; j++) {    // Write out the waveforms around the four signals
	  channel = j;    // Look on channel B,A,C,B for the four searches.
	  for (k = index[j] - 7; k < index[j] + 7; k++) {// Include exactly 15 channels in waveform
	    if (k < 0 || k >= np08->nSamples) np08->currentFileSize += fprintf(file, ",0");
	    else np08->currentFileSize += fprintf(file, ",%d", np08->rapidBuffers[channel][capture][k]);
	  }
	}
      }
      np08->currentFileSize += fprintf(file, "\n");  // Finally end the line
    }
  }  // End loop over captures
  np08->countCut2 = countCut2;
}

#if 0
void NP08PeakFind9(UNIT * unit, NP08VARS * np08)
{
  uint32_t capture;
  int16_t channel;
  int32_t i,j,tracelen;
  char yes[100000];
  
  // Warn if the yes array is not big enough
  if (np08->nSamples > 100000) {
	  printf("Fixed length array is dimensioned at 100000 which is smaller than the number of samples, sorry\n");
	  return;
  }

  // May modify these to be arguments to the function call, so we can automate web page write
  printf("Select capture to find peaks:\n");
  fflush(stdin);
  scanf_s("%lud", &capture);
  printf("Capture %d. Select window size in trace:\n",capture);
  fflush(stdin);
  scanf_s("%lud", &tracelen);
  if (capture > np08->nCapturesM-1 || tracelen > np08->nSamples-11) {
    printf("Invalid entry capture = %d (max %d) tracelen = %d (max %d)\n",capture,np08->nCapturesM-1,tracelen,np08->nSamples-11);
    return;
  }
  
  if (!np08->isMemAllocated) { printf("No data collected\n"); return; }
  if (np08->statusBulk != PICO_OK || np08->statusTrig != PICO_OK) {
    printf("Error when collecting data, not analysing it.  Codes are %d %d\n",np08->statusBulk,np08->statusTrig);
    return;
  }

  for (i=0;i<np08->nSamples;i++) yes[i]=0;

  // Flag yes[] for the channels with intereting pulses on them
  for (channel=0; channel<unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      for (i=0; i<np08->nSamples; i++) {
	if (np08->rapidBuffers[channel][capture][i] < np08->peakThreshold[channel]) {
	  int i0=i-tracelen/2;
	  int i1=i+tracelen/2;
	  if (i0<0) i0=0;
	  if (i1>np08->nSamples) i1=np08->nSamples;
	  for (j=i0;j<i1;j++) yes[j]=1;
	}
      }
    }
  }

  // Write out all the flagged channels
  j=0;
  for (i=0; i<np08->nSamples; i++) {
    if (yes[i]) {
      printf("[%d,%d",j,i);
      for (channel=0; channel<unit->channelCount; channel++) {
	if (unit->channelSettings[channel].enabled) {
	  printf(",%d",np08->rapidBuffers[channel][capture][i]);
	}
      }
      printf("]\n");
    }
  }
}
#endif

/****************************************************************************
* NP08CollectRapidBlock
*  Collects set of captures for the NP08 experiment and calls NP08AnalyseBlock()
*  for analysis.  Uses rapid block mode.  Based on the sample program 
*  collectRapidBlock() by picoscope (which is higher up in this file)
****************************************************************************/
// Returns 0 for normal completion.  Returns 1 if the sequence was interrupted by a key press
int NP08CollectRapidBlock(UNIT * unit, NP08VARS * np08, int init, int prnt)
{

  ///  uint32_t nCaptures;
  uint32_t nActiveChannels;
  /// uint32_t nSegments;
  int32_t  nMaxSamples;
  /// uint32_t nSamples = 1000;   // TODO Important, number of samples to take per trigger
  int32_t  timeIndisposed;
  uint32_t capture;
  int16_t  channel;
  /// int16_t*** rapidBuffers;
  /// int16_t* overflow;
  PICO_STATUS status;
  /// int16_t  i;
  uint32_t nCompletedCaptures;
  int16_t  retry;
  char ch = 'N';   // If this is X at the end, it will return 1 which will jump out of the main run loop
  
  int16_t  triggerVoltage = 1000; // mV
  PS5000A_CHANNEL triggerChannel = PS5000A_CHANNEL_A;
  int16_t  voltageRange = inputRanges[unit->channelSettings[triggerChannel].range];
  int16_t  triggerThreshold = 0;

  /// int32_t  timeIntervalNs = 0;
  int32_t  maxSamples = 0;
  uint32_t maxSegments = 0;
  
  /// uint64_t timeStampCounterDiff = 0;  
  /// PS5000A_TRIGGER_INFO * triggerInfo; // Struct to store trigger timestamping information

  // Structures for setting up trigger - declare each as an array of multiple structures if using multiple channels
  struct tPS5000ATriggerChannelPropertiesV2 triggerProperties;
  struct tPS5000ACondition conditions;
  struct tPS5000ADirection directions;
  
  // Struct to hold Pulse Width Qualifier information
  struct tPwq pulseWidth;

  setDefaults(unit);

  memset(&triggerProperties, 0, sizeof(struct tPS5000ATriggerChannelPropertiesV2));
  memset(&conditions, 0, sizeof(struct tPS5000ACondition));
  memset(&directions, 0, sizeof(struct tPS5000ADirection));
  memset(&pulseWidth, 0, sizeof(struct tPwq));
  
  // If the channel is not enabled, warn the User and return
  if (unit->channelSettings[np08->trigChannel].enabled == 0) {
    printf("collectBlockTriggered: The trigger channel is not enabled.");
    return 1;
  }

  if (np08->trigUseSimple) {   // Recommended, the complicated way may not be well debugged yet

    // The 1 in the second argument is 'enable' (any non-zero), the 0 in sixth argument is the delay in ticks. 
    status = ps5000aSetSimpleTrigger(unit->handle,1,np08->trigChannel,np08->trigThreshold,np08->trigDirection,0,np08->trigAuto_ms);

  } else { 

    // This code is from the original example, it uses the complicated trigger setup and is not fully debugged yet
    
    // If the trigger voltage level is greater than the range selected, set the threshold to half
    // of the range selected e.g. for ±200 mV, set the threshold to 10 0mV
    if (triggerVoltage > voltageRange) {
      triggerVoltage = (voltageRange / 2);
    }
    
    triggerThreshold = mv_to_adc(triggerVoltage, unit->channelSettings[triggerChannel].range, unit);

    // Set trigger channel properties
    triggerProperties.thresholdUpper = triggerThreshold;
    triggerProperties.thresholdUpperHysteresis = 256 * 10;
    triggerProperties.thresholdLower = triggerThreshold;
    triggerProperties.thresholdLowerHysteresis = 256 * 10;
    triggerProperties.channel = triggerChannel;
  
    // Set trigger conditions
    conditions.source = triggerChannel;
    conditions.condition = PS5000A_CONDITION_TRUE;

    // Set trigger directions
    directions.source = triggerChannel;
    directions.direction = PS5000A_RISING;
    directions.mode = PS5000A_LEVEL;

    printf("Collect rapid block triggered...\n");
    printf("Collects when value rises past %d ", scaleVoltages ?
	   adc_to_mv(triggerProperties.thresholdUpper, unit->channelSettings[PS5000A_CHANNEL_A].range, unit)	// If scaleVoltages, print mV value
	   : triggerProperties.thresholdUpper);									// else print ADC Count
    
    printf(scaleVoltages ? "mV\n" : "ADC Counts\n");
    printf("Press any key to abort\n");
  
    // Trigger enabled
    status = setTrigger(unit, &triggerProperties, 1, &conditions, 1, &directions, 1, &pulseWidth, 0, 0);
  }   // endif use simple trigger setup
    
  nActiveChannels = 0;
  for (channel=0; channel<unit->channelCount; channel++) if (unit->channelSettings[channel].enabled) nActiveChannels++;  // Count active channels
  if (nActiveChannels > 2) nActiveChannels = 4;   // When calculating the segments, 3 channels is as limited as 4 channels
  if (nActiveChannels == 0) {
    printf("No channels are enabled, enable at least 1 channel to take data\n");
    return 1;
  }
  status = ps5000aGetMaxSegments(unit->handle, &maxSegments);
  if (prnt) printf("maxSegments = %d, nActiveChannels = %d, nSegments = %d, nCaptures = %d"     // No newline
	 ,maxSegments,nActiveChannels,np08->nSegments,np08->nCaptures);
  if (np08->nSegments > maxSegments || np08->nCaptures > np08->nSegments/nActiveChannels) {
    printf("\nBad configuration, unable to segment the picoscope memory, not enough space\n");  // Extra newline at start
    return 1;
  }
  status = ps5000aMemorySegments(unit->handle, np08->nSegments, &nMaxSamples);  // Segment the memory
  status = ps5000aSetNoOfCaptures(unit->handle, np08->nCaptures);  // Set the number of captures
  if (prnt) printf(", nMaxSamples = %d\n",nMaxSamples);   // This finishes the line from above
  np08->nCapturesM=np08->nCaptures;  // Hopefully the actual number are what we want, but if user presses key to stop, it will be less
  np08->nSamplesM =np08->nSamples;   // Not sure why the number of samples may be different

  // Run
  np08->timebaseD = timebase;   // Record the timebase that was desired before any checks here
  // timebase = 127;		// 1 MS/s at 8-bit resolution, ~504 kS/s at 12 & 16-bit resolution

  // Verify timebase and number of samples per channel for segment 0
  do {
    status = ps5000aGetTimebase(unit->handle, timebase, np08->nSamples, &(np08->timeIntervalNs), &maxSamples, 0);
    if (status == PICO_INVALID_TIMEBASE) { timebase++; }
  } while (status != PICO_OK);
  np08->timebaseM = timebase;
  if (np08->timebaseD != np08->timebaseM) printf("Desired timebase %d too fast, changed to %d\n",np08->timebaseD, np08->timebaseM);

  do {
    retry = 0;
    g_ready = 0;  // Doing this here to make sure, was done below.
    status = ps5000aRunBlock(unit->handle, np08->nPreSamples, np08->nSamples - np08->nPreSamples, timebase, &timeIndisposed, 0, callBackBlock, NULL);

    if (status != PICO_OK) {
      // PicoScope 5X4XA/B/D devices...+5 V PSU connected or removed or
      // PicoScope 524XD devices on non-USB 3.0 port
      if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED || status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT) {
	status = changePowerSource(unit->handle, status, unit);
	retry = 1;
      } else {
	printf("NP08CollectRapidBlock:ps5000aRunBlock ------ 0x%08lx \n", status);
	retry = 0;
      }
    }
  } while (retry);

  // Wait until data ready (the callback routine will set g_ready non-zero) or keyboard hit
  // g_ready = 0;  // Moved this to before the call to ps5000aRunBlock()
  while (!g_ready && !_kbhit()) {
    Sleep(0);
  }

  if (!g_ready) {
    _getch();
    status = ps5000aStop(unit->handle);
    status = ps5000aGetNoOfCaptures(unit->handle, &nCompletedCaptures);

    printf("Rapid capture aborted. %lu complete blocks were captured\n", nCompletedCaptures);
    printf("\nNow press X to stop or any other key to continue...\n\n");
    ch = toupper(_getch());

	if (nCompletedCaptures == 0) { return (ch == 'X')?1:0; }   // Return 1 if ch is x or X
    np08->nCapturesM = (uint16_t)nCompletedCaptures;  // Only display the blocks that were captured
  }

  NP08AllocateBuffers(unit, np08);
  
  // Register the buffers to receive the data in the call to ps5000GetValuesBulk() below
  for (channel = 0; channel < unit->channelCount; channel++) {
    if (unit->channelSettings[channel].enabled) {
      for (capture = 0; capture < np08->nCapturesM; capture++) {
	status = ps5000aSetDataBuffer(unit->handle, (PS5000A_CHANNEL)channel, np08->rapidBuffers[channel][capture], np08->nSamples, capture, PS5000A_RATIO_MODE_NONE);
      }
    }
  }

  // Get data  (np08->nSamplesM is the number of samples obtained, normally equal to np08->nSamples)
  status = ps5000aGetValuesBulk(unit->handle, &(np08->nSamplesM), 0, np08->nCapturesM - 1, 1, PS5000A_RATIO_MODE_NONE, np08->overflow);
  np08->statusBulk = status;
  
  if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED ||
      status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT || status == PICO_POWER_SUPPLY_UNDERVOLTAGE) {
    printf("\nPower Source Changed. Data collection aborted.\n");
  }

  // Retrieve trigger timestamping information
  //memset(np08->triggerInfo, 0, np08->nCapturesM * sizeof(PS5000A_TRIGGER_INFO));
  //status = ps5000aGetTriggerInfoBulk(unit->handle, np08->triggerInfo, 0, np08->nCapturesM - 1);
  //np08->statusTrig = status;
  np08->statusTrig = 0;   // Comment this line if you uncomment the above to get the trigger info
  
  // Stop
  status = ps5000aStop(unit->handle);

#if 0
  if (prnt) NP08ProcessData(unit,np08);
#endif  

  /// NP08FreeBuffers(unit, np08);
  return (ch == 'X') ? 1 : 0;   // If the user stopped by pressing a key and then an 'X' return 1 otherwise return 0
}

void printTriggerTimeInfo(NP08VARS * np08, int level) {    // Print info from ps5000aGetTriggerInfoBulk
	int k = np08->nCapturesM - 1;
	printf("Last   %10lld %d %d, First    %10lld %d %d, diff %10lld\n", 
		np08->triggerInfo[k].triggerTime, np08->triggerInfo[k].timeUnits, (np08->triggerInfo[k].status)?1:0,
		np08->triggerInfo[0].triggerTime, np08->triggerInfo[0].timeUnits, (np08->triggerInfo[0].status)?1:0,
		np08->triggerInfo[k].triggerTime - np08->triggerInfo[0].triggerTime);
	if (level == 0) return;
	printf("Second %10lld %d %d, First    %10lld %d %d, diff %10lld\n", 
		np08->triggerInfo[1].triggerTime, np08->triggerInfo[1].timeUnits, (np08->triggerInfo[1].status)?1:0,
		np08->triggerInfo[0].triggerTime, np08->triggerInfo[0].timeUnits, (np08->triggerInfo[0].status)?1:0,
		np08->triggerInfo[1].triggerTime - np08->triggerInfo[0].triggerTime);
	printf("First  %10lld %d %d, LastPrev %10lld,     diff %10lld\n",
		np08->triggerInfo[0].triggerTime, np08->triggerInfo[0].timeUnits, (np08->triggerInfo[0].status)?1:0, 
		np08->triggerTimeLast, np08->triggerInfo[0].triggerTime-np08->triggerTimeLast);
	np08->triggerTimeLast = np08->triggerInfo[k].triggerTime;
}

// Loop over calls to NP08CollectRapidMode() and NP08PeakFind2()
void NP08Loop(UNIT * unit, NP08VARS * np08)
{
	int ngroup = np08->maxLoopGroups;  // 3600000; //36000;
	int igroup;
	int st = 0;
	int cntr = 2;
	char filename[1000], logname[1000], ratename[1000];
	FILE* file;
	FILE* ratefile;
	int64_t StartTime_micros;
	int64_t EndTime_micros;
	double DiffTime_micros;
	double DiffTime_micros_Total = 0; //Total Time
	int32_t countCut2_Total = 0; //Cut2 Total Triggers
	int32_t nSamples_Total = 0;  //Cut1 Total Triggers
	int32_t countCut2; //Cut2 Triggers
	int32_t nSamples;  //Cut1 Triggers
	double Rate_Cut1 = -999;
	double Rate_Cut2 = -999;
	double Rate_Cut1_Avg = -999;
	double Rate_Cut2_Avg = -999;

	struct timespec now;

	timespec_get(&now, TIME_UTC);
	char RunStartTime[100];
	strftime(RunStartTime, sizeof RunStartTime, "%D %T", gmtime(&now.tv_sec));

	if (ngroup < 0) ngroup = -ngroup;

	// comented out this.  printf("**WARNING** Special version of code in use.  The channel D threshold (main-menu-S->D) is used for the B3,4,5,6 peak finding.\n"); 

	do {
		printf("We suggest a run number of the form wccrrr where w is the week number in the term,\n");
		printf("cc is the computer number 33, 34, 35, 36 and rrr is a sequential number you choose\n");
		printf("e.g. 435001 if you are in week 4, on computer PTLWT35 and this is your first run\n");
		printf("Run number [0 to 999999]:\n");
		fflush(stdin);
		scanf_s("%lud", &np08->runNumber);
	} while (np08->runNumber > 999999);

	np08->currentFileSize = 0;
	do {
		snprintf(filename, 1000, "runD_%6.6d.dat", np08->runNumber);
		snprintf(logname, 1000, "runD_%6.6d.log", np08->runNumber);
		snprintf(ratename, 1000, "runD_%6.6d_rate.log", np08->runNumber);

		fopen_s(&file, logname, "w");
		fprintf(file, "Settings used for run %d are\n\n", np08->runNumber);
		printNP08Things(unit, np08, file);
		fprintf(file, "\n");
		displaySettings(unit, file);
		fprintf(file, "RunStartTime = %s.%09ld", RunStartTime);
		fclose(file);

		printf("Run number is %d, data will be written to %s.  Settings are written to %s.  Data collection starting, processing with PeakFind5.\n",
			np08->runNumber, filename, logname);

		fopen_s(&file, filename, "w");
		fopen_s(&ratefile, ratename, "w");

		timespec_get(&now, TIME_UTC);
		char CurrTime[100];
		strftime(CurrTime, sizeof CurrTime, "%D %T", gmtime(&now.tv_sec));
		fprintf(ratefile, "%s.%09ld,%g,%g,%g,%g\n", CurrTime, now.tv_nsec, -999.999, -999.999, -999.999, -999.999); //Print dummy rates to file to ensure that first entry includes start time

		for (igroup = 0; igroup < ngroup; igroup++) {
			np08->currentLoopGroup = igroup;
			Rate_Cut1 = -999;
			Rate_Cut2 = -999;
			Rate_Cut1_Avg = -999;
			Rate_Cut2_Avg = -999;

			StartTime_micros = GetTime_MicroSecond();

			st = NP08CollectRapidBlock(unit, np08, (igroup == 0) ? 1 : 0, 0);
			// printTriggerTimeInfo(np08, 1);  // To use this, also uncomment the GetTriggerInfoBulk() call in NP08CollectRapidBlock()
			// NP08PeakFind2(unit, np08, file);
			NP08PeakFind5(unit, np08, file);

			EndTime_micros = GetTime_MicroSecond();
			
			DiffTime_micros = ((double) (EndTime_micros - StartTime_micros))/1000000.;
			DiffTime_micros_Total += DiffTime_micros;

			countCut2 = np08->countCut2;
			nSamples = np08->nSamples;
			countCut2_Total += np08->countCut2;
			nSamples_Total += np08->nSamples;

			if (DiffTime_micros != 0) {
				Rate_Cut1 = (double)nSamples / DiffTime_micros;
				Rate_Cut2 = (double)countCut2 / DiffTime_micros;
			}
			if (DiffTime_micros_Total != 0) {
				Rate_Cut1_Avg = (double)nSamples_Total / DiffTime_micros_Total;
				Rate_Cut2_Avg = (double)countCut2_Total / DiffTime_micros_Total;
			}

			timespec_get(&now, TIME_UTC);
			char CurrTime[100];
			strftime(CurrTime, sizeof CurrTime, "%D %T", gmtime(&now.tv_sec));

			fprintf(ratefile,"%s.%09ld,%g,%g,%g,%g\n", CurrTime, now.tv_nsec, Rate_Cut1, Rate_Cut2, Rate_Cut1_Avg, Rate_Cut2_Avg); //Print rates to file

			printf("Done loop %d of %d | File size is %dkB of max %dMB | CUT1 rate (Hz) %g | CUT2 rate (Hz) %g | CUT1 Avg rate (Hz) %g | CUT2 Avg rate (Hz) %g\n", igroup, ngroup, np08->currentFileSize / 1024, np08->maxFileSize, Rate_Cut1, Rate_Cut2, Rate_Cut1_Avg, Rate_Cut2_Avg);
			if (st == 1) {
				printf("Requested stop\n");
				break;
			}
			if (np08->currentFileSize / 1024 / 1024 > np08->maxFileSize) {
				st = 2;
				printf("Stop because max file size reached\n");
				break;
			}
			st = 0;
		}    // Exits this loop with st=0 (ngroup limit reached) st=1 (requested stop) st=2 (space limit) 

		printf("%d bytes written to file %s in %d groups\n", np08->currentFileSize, filename, np08->currentLoopGroup);
		fclose(file);
		fclose(ratefile);

		if (st != 0) {
			break;
		}
		if (!(np08->maxLoopGroups<0)) break;    // Why does this not break out of the do{}while(1) when positive. 

		// Doing a sequence of runs, increment run number and loop back
		/// np08->runNumber++;
		/// printf("Run sequence with groups of size %d, press any key to stop.\n",np08->maxLoopGroups);
	} while (0);  /// Temporary - jump out.   // Loop exit is via a break immediately above here (to allow sequence of runs)
}

/****************************************************************************
* NP08Menu
* Controls most common functions of the selected unit of the NP08 practical
* Parameters
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/

void NP08Menu(UNIT* unit) {

	NP08VARS np08struct;   // This allocate the memory for our parameter structure
	NP08VARS* np08 = &np08struct;  // We refer to it with this, so it is the same in all our routines

	int8_t ch = '.';
	int st;

	setNP08Default(unit, np08);

  // Initialise the operation part of np08 structure.  These get allocated when needed and filled when data arrives
  np08->rapidBuffers = NULL;
  np08->overflow = NULL;
  np08->triggerInfo = NULL;
  np08->triggerTimeLast = 0;  // From last capture (since there isn' one, 0 is the best we can do).
  np08->isMemAllocated = 0;   // 0 = not allocated, 1 = they have been calloc/malloced
  np08->nCapturesM = np08->nCaptures;  // Number of captures received (smaller if key pressed)
  np08->nSamplesM = np08->nSamples;    // Number received, not sure how this can be different from desired?
  np08->statusBulk = 0;
  np08->statusTrig = 0;
  np08->currentLoopGroup = 0;
  np08->currentFileSize = 0;

  while (ch != 'X') {
    displaySettings(unit, stdout);

	printf("\n");
    printf("C - Collect set of Rapid captures   D - Set resolution\n");
    printf("O - Output from rapid captures      I - Set timebase\n");
    printf("L - Loop for long run to disk       V - Set voltage ranges\n");
    printf("E - Extra function menu (none yet)  S - Set NP08 trigger and peak finding\n");
    printf("M - Picoscope SDK example Menu      X - Exit\n");
    printf("Operation:");

	//printf("C - Collect set of Rapid captures  L - Loop   D - Set resolution\n");
	//printf("P - Retry Process of previous rapid captures  I - Set timebase\n");
	//printf("S - Setup NP08 things                         V - Set voltage ranges\n");
	//printf("Q - Look at specific samples in previous      A - Set something we dont want\n");
	//printf("F - Plot function 1                           H - Plot function 3\n");
	//printf("O - Plot function 2\n");
	//printf("M - Picoscope SDK example Menu                X - Exit\n");

    ch = toupper(_getch());

    printf("\n");

    switch (ch) {
    
	case 'L':
		NP08Loop(unit, np08);
		break;

	case 'M':
      NP08FreeBuffers(unit,np08);
      PicoscopeMenu(unit);
      break;

    case 'C':
		np08->currentLoopGroup = 0;
      st = NP08CollectRapidBlock(unit,np08,1,1);
      break;

#if 0
    case 'P':
      NP08ProcessData(unit,np08);
      break;
      
    case 'Q':
      NP08ProcessData2(unit, np08);
      break;
#endif     
 
    case 'S':
      setNP08Things(unit,np08);
      break;

    case 'V':
      setVoltages(unit);
      break;

    case 'I':
      setTimebase(unit);
      break;

    case 'D':
      setResolution(unit);
      break;

#if 0
	case 'F':
		NP08PeakFind1(unit, np08);
		break;

	case 'H':
		NP08PeakFind9(unit, np08);
		break;
#endif

	case 'O':
		NP08PeakFind5(unit, np08, stdout);
		break;

    case 'X':
      break;

    default:
      printf("Invalid operation.\n");
      break;
    }
  }
  NP08FreeBuffers(unit,np08);    
}


/****************************************************************************
* main  [Mostly unmodified from picoscope example]
*
***************************************************************************/
int32_t main(void)
{
	int8_t ch;
	uint16_t devCount = 0, listIter = 0,	openIter = 0;
	//device indexer -  64 chars - 64 is maximum number of picoscope devices handled by driver
	int8_t devChars[] =
			"1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#";
	PICO_STATUS status = PICO_OK;
	UNIT allUnits[MAX_PICO_DEVICES];

	printf("PicoScope 5000 Series (ps5000a) Driver Example Program\n");
	printf("\nEnumerating Units...\n");

	do
	{
		status = openDevice(&(allUnits[devCount]), NULL);
		
		if (status == PICO_OK || status == PICO_POWER_SUPPLY_NOT_CONNECTED 
					|| status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
		{
			allUnits[devCount++].openStatus = (int16_t) status;
		}

	} while(status != PICO_NOT_FOUND);

	if (devCount == 0)
	{
		printf("Picoscope devices not found\n");
		printf("Press any character to close window (check if another program is using the picoscope)\n");  ch = _getch();
		return 1;
	}
	
	// if there is only one device, open and handle it here
	if (devCount == 1)
	{
		printf("Found one device, opening...\n\n");
		status = allUnits[0].openStatus;

		if (status == PICO_OK || status == PICO_POWER_SUPPLY_NOT_CONNECTED
					|| status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
		{
			if (allUnits[0].openStatus == PICO_POWER_SUPPLY_NOT_CONNECTED || allUnits[0].openStatus == PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
			{
				allUnits[0].openStatus = (int16_t)changePowerSource(allUnits[0].handle, allUnits[0].openStatus, &allUnits[0]);
			}

			set_info(&allUnits[0]);
			status = handleDevice(&allUnits[0]);
		}

		if (status != PICO_OK)
		{
			printf("Picoscope devices open failed, error code 0x%x\n",(uint32_t)status);
                        printf("Press any character to close window (check if another program is using the picoscope)\n");  ch = _getch();
			return 1;
		}

		NP08Menu(&allUnits[0]);   /*  Changed this line */
		closeDevice(&allUnits[0]);
		printf("Exit...\n");
		return 0;
	}
	else
	{
		// More than one unit
		printf("Found %d devices, initializing...\n\n", devCount);

		for (listIter = 0; listIter < devCount; listIter++)
		{
			if (allUnits[listIter].openStatus == PICO_OK || allUnits[listIter].openStatus == PICO_POWER_SUPPLY_NOT_CONNECTED)
			{
				set_info(&allUnits[listIter]);
				openIter++;
			}
		}
	}
	
	// None
	if (openIter == 0)
	{
		printf("Picoscope devices init failed\n");
		printf("Press any character to close window (check if another program is using the picoscope)\n");  ch = _getch();
		return 1;
	}
	// Just one - handle it here
	if (openIter == 1)
	{
		for (listIter = 0; listIter < devCount; listIter++)
		{
			if (!(allUnits[listIter].openStatus == PICO_OK || allUnits[listIter].openStatus == PICO_POWER_SUPPLY_NOT_CONNECTED))
			{
				break;
			}
		}
		
		printf("One device opened successfully\n");
		printf("Model\t: %s\nS/N\t: %s\n", allUnits[listIter].modelString, allUnits[listIter].serial);
		status = handleDevice(&allUnits[listIter]);
		
		if (status != PICO_OK)
		{
			printf("Picoscope device open failed, error code 0x%x\n", (uint32_t)status);
                        printf("Press any character to close window (check if another program is using the picoscope)\n");  ch = _getch();
			return 1;
		}
		
		NP08Menu(&allUnits[listIter]);   /* Changed this line */
		closeDevice(&allUnits[listIter]);
		printf("Exit...\n");
		return 0;
	}
	printf("Found %d devices, pick one to open from the list:\n", devCount);

	for (listIter = 0; listIter < devCount; listIter++)
	{
		printf("%c) Picoscope %7s S/N: %s\n", devChars[listIter],
				allUnits[listIter].modelString, allUnits[listIter].serial);
	}

	printf("ESC) Cancel\n");

	ch = '.';
	
	// If escape
	while (ch != 27)
	{
		ch = _getch();
		
		// If escape
		if (ch == 27)
			continue;
		for (listIter = 0; listIter < devCount; listIter++)
		{
			if (ch == devChars[listIter])
			{
				printf("Option %c) selected, opening Picoscope %7s S/N: %s\n",
						devChars[listIter], allUnits[listIter].modelString,
						allUnits[listIter].serial);
				
				if ((allUnits[listIter].openStatus == PICO_OK || allUnits[listIter].openStatus == PICO_POWER_SUPPLY_NOT_CONNECTED))
				{
					status = handleDevice(&allUnits[listIter]);
				}
				
				if (status != PICO_OK)
				{
					printf("Picoscope devices open failed, error code 0x%x\n", (uint32_t)status);
					printf("Press any character to close window (check if another program is using the picoscope)\n");  ch = _getch();
					return 1;
				}

				NP08Menu(&allUnits[listIter]);   /* Changed this line */

				printf("Found %d devices, pick one to open from the list:\n",devCount);
				
				for (listIter = 0; listIter < devCount; listIter++)
				{
					printf("%c) Picoscope %7s S/N: %s\n", devChars[listIter],
							allUnits[listIter].modelString,
							allUnits[listIter].serial);
				}
				
				printf("ESC) Cancel\n");
			}
		}
	}

	for (listIter = 0; listIter < devCount; listIter++)
	{
		closeDevice(&allUnits[listIter]);
	}

	printf("Exit...\n");
	
	return 0;
}
