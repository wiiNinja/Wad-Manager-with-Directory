#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <malloc.h>
#include <ctype.h>

#include "globals.h"
#include "gui.h"
#include "menu.h"
#include "restart.h"
#include "sys.h"
#include "video.h"
#include "wpad.h"
#include "fat.h"

// Globals
CONFIG gConfig;

// Prototypes
extern u32 WaitButtons (void);
void CheckPassword (void);
void SetDefaultConfig (void);
int ReadConfigFile (char *configFilePath);
int GetPassword (char *retPassword, char *inputStr);
int GetStartupPath (char *startupPath, char *inputStr);

// Default password Up-Down-Left-Right-Up-Down
//#define PASSWORD "UDLRUD"
void CheckPassword (void)
{
	char curPassword [11]; // Max 10 characters password, NULL terminated
	int count = 0;

	if (strlen (gConfig.password) == 0)
		return;

	// Ask user for a password. Press "B" to restart Wii
	printf("[+] [Enter Password to Continue]:\n\n");

	printf(">>  Press A to continue.\n");
	printf(">>  Press B button to restart your Wii.\n");

	/* Wait for user answer */
	for (;;) 
	{
		u32 buttons = WaitButtons();

		if (buttons & WPAD_BUTTON_A)
		{
			// A button, validate the pw
			curPassword [count] = 0;
			//if (strcmp (curPassword, PASSWORD) == 0)
			if (strcmp (curPassword, gConfig.password) == 0)
			{
				printf(">>  Password Accepted...\n");
				break;
			}
			else
			{
				printf ("\n");
				printf(">>  Incorrect Password. Try again...\n");
				printf("[+] [Enter Password to Continue]:\n\n");
				printf(">>  Press A to continue.\n");
				printf(">>  Press B button to restart your Wii.\n");
				count = 0;
			}
		}
		else if (buttons & WPAD_BUTTON_B)
			// B button, restart
			Restart();
		else
		{
			if (count < 10)
			{
				// Other buttons, build the password
				if (buttons & WPAD_BUTTON_LEFT)
				{
					curPassword [count++] = 'L';
					printf ("*");
				}
				else if (buttons & WPAD_BUTTON_RIGHT)
				{
					curPassword [count++] = 'R';
					printf ("*");
				}
				else if (buttons & WPAD_BUTTON_UP)
				{
					curPassword [count++] = 'U';
					printf ("*");
				}
				else if (buttons & WPAD_BUTTON_DOWN)
				{
					curPassword [count++] = 'D';
					printf ("*");
				}
				else if (buttons & WPAD_BUTTON_1)
				{
					curPassword [count++] = '1';
					printf ("*");
				}
				else if (buttons & WPAD_BUTTON_2)
				{
					curPassword [count++] = '2';
					printf ("*");
				}
			}
		}
	}
}

void Disclaimer(void)
{
	/* Print disclaimer */
	printf("[+] [DISCLAIMER]:\n\n");

	printf("    THIS APPLICATION COMES WITH NO WARRANTY AT ALL,\n");
	printf("    NEITHER EXPRESS NOR IMPLIED.\n");
	printf("    I DO NOT TAKE ANY RESPONSIBILITY FOR ANY DAMAGE IN YOUR\n");
	printf("    WII CONSOLE BECAUSE OF A IMPROPER USAGE OF THIS SOFTWARE.\n\n");

	printf(">>  If you agree, press A button to continue.\n");
	printf(">>  Otherwise, press B button to restart your Wii.\n");

	/* Wait for user answer */
	for (;;) {
		//u32 buttons = Wpad_WaitButtons();
		u32 buttons = WaitButtons();

		/* A button */
		if (buttons & WPAD_BUTTON_A)
			break;

		/* B button */
		if (buttons & WPAD_BUTTON_B)
			Restart();
	}
}

int main(int argc, char **argv)
{
	/* Initialize subsystems */
	Sys_Init();

	/* Set video mode */
	Video_SetMode();

	/* Initialize console */
	Gui_InitConsole();

	/* Draw background */
	Gui_DrawBackground();

	/* Initialize Wiimote and GC Controller */
	Wpad_Init();
	PAD_Init ();

	/* Print disclaimer */
	//Disclaimer();
	
	// Set the defaults
	SetDefaultConfig ();

	// Read the config file
	ReadConfigFile (WM_CONFIG_FILE_PATH);

	// Check password
	CheckPassword ();

	/* Menu loop */
	Menu_Loop();

	/* Restart Wii */
	Restart_Wait();

	return 0;
}

extern fatDevice fdevList[];

int ReadConfigFile (char *configFilePath)
{
    int retval = 0;
	FILE *fptr;
	char *tmpStr = malloc (MAX_FILE_PATH_LEN);

	if (tmpStr == NULL)
		return (-1);

	fatDevice *fdev = &fdevList[0];
	s32 ret = Fat_Mount(fdev);
	
	if (ret < 0) 
	{
		printf(" ERROR! (ret = %d)\n", ret);
		// goto err;
		retval = -1;
	}
	else
	{
		// Read the file
		fptr = fopen (configFilePath, "rb");
		if (fptr != NULL)
		{	
			// Read the options
			char done = 0;

			while (!done)
			{
				if (fgets (tmpStr, MAX_FILE_PATH_LEN, fptr) == NULL)
					done = 1;
				else if (isalpha(tmpStr[0]))
				{
					// Get the password
					if (strncmp (tmpStr, "Password", 8) == 0)
					{
						// Get password
						GetPassword (gConfig.password, tmpStr);
					}
				
					// Get startup path
					else if (strncmp (tmpStr, "StartupPath", 11) == 0)
					{
						// Get startup Path
						GetStartupPath (gConfig.startupPath, tmpStr);
					}
				}
			} // EndWhile
		
			// Close the config file
			fclose (fptr);
		}
		else
		{
			// If the wm_config.txt file is not found, just take the default config params
			//printf ("Config file is not found\n");  // This is for testing only
			//WaitButtons();
		}
		Fat_Unmount(fdev);
	}

	// Free memory
	free (tmpStr);

	return (retval);
} // ReadConfig


void SetDefaultConfig (void)
{
	// Default password is NULL or no password
	gConfig.password [0] = 0;
	
	// Default startup folder
	strcpy (gConfig.startupPath, WAD_ROOT_DIRECTORY);

} // SetDefaultConfig


int GetPassword (char *retPassword, char *inputStr)
{
	int i = 0;
	int len = strlen (inputStr);
	
	// Find the "="
	while ((inputStr [i] != '=') && (i < len))
	{
		i++;
	}
	i++;
	
	// Get to the first alpha numeric
	while ((isalpha(inputStr [i]) == 0) && (i < len))
	{
		i++;
	}
	
	// Get the password
	int pwCount = 0;
	while ((isalpha(inputStr [i])) && (i < len))
	{
		retPassword [pwCount++] = inputStr [i++];
	}
	retPassword [pwCount] = 0; // NULL terminate

	// If password is too long, ignore it
	if (strlen (retPassword) > 10)
	{
		retPassword [0] = 0;
		printf ("Password longer than 10 characters; will be ignored. Press a button...\n");
		WaitButtons ();
	}
	
	return (0);
} // GetPassword

int GetStartupPath (char *startupPath, char *inputStr)
{
	int i = 0;
	int len = strlen (inputStr);
	
	// Find the "="
	while ((inputStr [i] != '=') && (i < len))
	{
		i++;
	}
	i++;

	// Get to the "/"
	while ((inputStr [i] != '/') && (i < len))
	{
		i++;
	}

	// Get the startup Path
	int count = 0;
	while (isascii(inputStr [i]) && (i < len) && (inputStr [i] != '\n') && 
	         (inputStr [i] != '\r') && (inputStr [i] != ' '))
	{
		startupPath [count++] = inputStr [i++];
	}
	startupPath [count] = 0; // NULL terminate

	return (0);
} // GetStartupPath

