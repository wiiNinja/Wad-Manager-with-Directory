#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>

#include "gui.h"
#include "menu.h"
#include "restart.h"
#include "sys.h"
#include "video.h"
#include "wpad.h"

extern u32 WaitButtons (void);

void CheckPassword (void);

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
		u32 buttons = WaitButtons();

		/* A button */
		if (buttons & WPAD_BUTTON_A)
			break;

		/* B button */
		if (buttons & WPAD_BUTTON_B)
			Restart();
	}
}

#define PASSWORD "UDLR12"
void CheckPassword (void)
{
	char curPassword [11]; // Max 10 characters password, NULL terminated
	int count = 0;

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
			if (strcmp (curPassword, PASSWORD) == 0)
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

int main(int argc, char **argv)
{
    //int retval = 0;

	// Initialize subsystems
	Sys_Init();

	/* Set video mode */
	Video_SetMode();

	/* Initialize console */
	Gui_InitConsole();

	/* Draw background */
	Gui_DrawBackground();

	/* Initialize Wiimote */
	Wpad_Init();
    PAD_Init();

	/* Print disclaimer */
	//Disclaimer();
	
	CheckPassword ();

	/* Menu loop */
	Menu_Loop();

	/* Restart Wii */
	Restart_Wait();

	return 0;
}
