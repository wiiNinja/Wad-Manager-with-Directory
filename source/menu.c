#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <ogcsys.h>
#include <wiilight.h>

#include "fat.h"
#include "nand.h"
#include "restart.h"
#include "title.h"
#include "usbstorage.h"
#include "utils.h"
#include "video.h"
#include "wad.h"
#include <ogc/pad.h>
#include "wpad.h"
#include "globals.h"

/* FAT device list  */
fatDevice fdevList[] = {
	{ "sd",		"Wii SD Slot",			&__io_wiisd },
	{ "usb",	"USB Mass Storage Device",	&__io_usbstorage },
	{ "usb2",	"USB 2.0 Mass Storage Device",	&__io_usb2storage },
	{ "gcsda",	"SD Gecko (Slot A)",		&__io_gcsda },
	{ "gcsdb",	"SD Gecko (Slot B)",		&__io_gcsdb },
};

/* NAND device list */
nandDevice ndevList[] = {
	{ "Disable",				0,	0x00,	0x00 },
	{ "SD/SDHC Card",			1,	0xF0,	0xF1 },
	{ "USB 2.0 Mass Storage Device",	2,	0xF2,	0xF3 },
};

// Flag to allow user to escape to the top of the menu loop to manually set everything again
bool gEscapeToTop;

/* FAT device */
static fatDevice  *fdev = NULL;
static nandDevice *ndev = NULL;

/* Macros */
#define NB_FAT_DEVICES		(sizeof(fdevList) / sizeof(fatDevice))
#define NB_NAND_DEVICES		(sizeof(ndevList) / sizeof(nandDevice))

/* Constants */
#define CIOS_VERSION		249

// Local prototypes: wiiNinja
void WaitPrompt (char *prompt);
u32 WaitButtons(void);
u32 Pad_GetButtons(void);
void WiiLightControl (int state);

s32 __Menu_IsGreater(const void *p1, const void *p2)
{
	u32 n1 = *(u32 *)p1;
	u32 n2 = *(u32 *)p2;

	/* Equal */
	if (n1 == n2)
		return 0;

	return (n1 > n2) ? 1 : -1;
}

s32 __Menu_EntryCmp(const void *p1, const void *p2)
{
	fatFile *f1 = (fatFile *)p1;
	fatFile *f2 = (fatFile *)p2;

	s32 ret;

	/* Compare attributes */
	ret = (f1->filestat.st_mode - f2->filestat.st_mode);
	if (ret)
		return ret;

	/* Compare names */
	return strcmp(f1->filename, f2->filename);
}

s32 __Menu_RetrieveList(fatFile **outbuf, u32 *outlen)
{
	fatFile  *buffer = NULL;
	DIR_ITER *dir    = NULL;

	struct stat filestat;

	char filename[768];
	u32  cnt;

	/* Open directory */
    dir = diropen(".");
	if (!dir)
		return -1;

	/* Count entries */
	for(cnt = 0; !dirnext(dir, filename, &filestat); cnt++);

	if (cnt > 0) {
		/* Allocate memory */
		buffer = malloc(sizeof(fatFile) * cnt);
		if (!buffer) {
			dirclose(dir);
			return -2;
		}

		/* Reset directory */
		dirreset(dir);

		/* Get entries */
		for (cnt = 0; !dirnext(dir, filename, &filestat);) 
		{
			bool addFlag = false;
			
			if ((filestat.st_mode & S_IFDIR) && (strcmp (filename, ".") != 0))  // wiiNinja
            {
                // Add only the item ".." which is the previous directory
                // AND if we're not at the root directory
                //if ((strcmp (filename, "..") == 0) && (gDirLevel > 1))
                //    addFlag = true;
                //else if (strcmp (filename, ".") != 0)
				// if (strcmp (filename, ".") != 0)
                    addFlag = true;
            }
            else if((strlen(filename)>4) && (!stricmp(filename+strlen(filename)-4, ".wad")))
				addFlag = true;

            if (addFlag == true)
            {
				fatFile *file = &buffer[cnt++];

				/* File name */
				strcpy(file->filename, filename);

				/* File stats */
				file->filestat = filestat;
			}
		}

		/* Sort list */
		qsort(buffer, cnt, sizeof(fatFile), __Menu_EntryCmp);
	}

	/* Close directory */
	dirclose(dir);

	/* Set values */
	*outbuf = buffer;
	*outlen = cnt;

	return 0;
}

s32 __Menu_ChangeDir(fatFile *file, fatFile **outbuf, u32 *outlen)
{
	/* Change directory */
	Fat_ChangeDir(file->filename);

	/* Free memory */
	if (*outbuf)
		free(*outbuf);

	/* Retrieve list */
	return __Menu_RetrieveList(outbuf, outlen);
}


void Menu_SelectIOS(void)
{
	u8 *iosVersion = NULL;
	u32 iosCnt;
	u32 cnt;
	s32 ret, selected = 0;
	bool found = false;
    u8 tmpVersion;

	if (gEscapeToTop == true)
		return;

	/* Get IOS versions */
	ret = Title_GetIOSVersions(&iosVersion, &iosCnt);
	if (ret < 0)
		return;

	/* Sort list */
	qsort(iosVersion, iosCnt, sizeof(u8), __Menu_IsGreater);

	if (gConfig.cIOSVersion < 0)
		tmpVersion = CIOS_VERSION;
	else
	{
		tmpVersion = (u8)gConfig.cIOSVersion;
		// For debugging only
		//printf ("User pre-selected cIOS: %i\n", tmpVersion);
		//WaitButtons();
	}

	/* Set default version */
	for (cnt = 0; cnt < iosCnt; cnt++) {
		u8 version = iosVersion[cnt];

		/* Custom IOS available */
		//if (version == CIOS_VERSION) 
		if (version == tmpVersion)
		{
			selected = cnt;
			found = true;
			break;
		}

		/* Current IOS */
		if (version == IOS_GetVersion())
			selected = cnt;
	}

	/* Ask user for IOS version */
	if ((gConfig.cIOSVersion < 0) || (found == false))
	{
		for (;;)
		{
			/* Clear console */
			Con_Clear();

			printf("\t>> Select IOS version to use: < IOS%d >\n\n", iosVersion[selected]);

			printf("\t   Press LEFT/RIGHT to change IOS version.\n\n");

			printf("\t   Press A button to continue.\n");
			printf("\t   Press HOME button to restart.\n\n");

			u32 buttons = WaitButtons();

			/* LEFT/RIGHT buttons */
			if (buttons & WPAD_BUTTON_LEFT) {
				if ((--selected) <= -1)
					selected = (iosCnt - 1);
			}
			if (buttons & WPAD_BUTTON_RIGHT) {
				if ((++selected) >= iosCnt)
					selected = 0;
			}

			/* HOME button */
			if (buttons & WPAD_BUTTON_HOME)
				Restart();

			/* Button 1 means escape to top */
			if (buttons & WPAD_BUTTON_1)
			{
				gEscapeToTop = true;
				break;
			}

			/* A button */
			if (buttons & WPAD_BUTTON_A)
				break;
		}
	}


	u8 version = iosVersion[selected];

	if (IOS_GetVersion() != version) {
		/* Shutdown subsystems */
		Wpad_Disconnect();

		printf("\tLoading IOS%d...\n", version);
		sleep(1);

		/* Load IOS */
		ret = IOS_ReloadIOS(version);

		/* Initialize subsystems */
		Wpad_Init();
	}
}

void Menu_FatDevice(void)
{
	s32 selected = 0;
	s32 ret;

	if (gEscapeToTop == true)
		return;
		
	/* Unmount FAT device */
	if (fdev)
		Fat_Unmount(fdev);

	/* Select source device */
	if (gConfig.fatDeviceIndex < 0)
	{
		for (;;) {
			/* Clear console */
			Con_Clear();

			/* Selected device */
			fdev = &fdevList[selected];

			printf("\t>> Select source device: < %s >\n\n", fdev->name);

			printf("\t   Press LEFT/RIGHT to change the selected device.\n\n");

			printf("\t   Press A button to continue.\n");
			printf("\t   Press HOME button to restart.\n\n");

			u32 buttons = WaitButtons();

			/* LEFT/RIGHT buttons */
			if (buttons & WPAD_BUTTON_LEFT) {
				if ((--selected) <= -1)
					selected = (NB_FAT_DEVICES - 1);
			}
			if (buttons & WPAD_BUTTON_RIGHT) {
				if ((++selected) >= NB_FAT_DEVICES)
					selected = 0;
			}

			/* HOME button */
			if (buttons & WPAD_BUTTON_HOME)
				Restart();

			// Button 1 means to escape to top
			if (buttons & WPAD_BUTTON_1)
			{
				gEscapeToTop = true;
				break;
			}

			/* A button */
			if (buttons & WPAD_BUTTON_A)
				break;
		}
	}
	else
	{
		fdev = &fdevList[gConfig.fatDeviceIndex];
	}

	if (gEscapeToTop == false)
	{
		printf("[+] Mounting device, please wait...");
		fflush(stdout);

		/* Mount FAT device */
		ret = Fat_Mount(fdev);
		if (ret < 0) {
			printf(" ERROR! (ret = %d)\n", ret);
			goto err;
		} else
			printf(" OK!\n");
	}

	return;

err:
	WiiLightControl (WII_LIGHT_OFF);
	printf("\n");
	printf("    Press any button to continue...\n");

	WaitButtons();

	/* Prompt menu again */
	Menu_FatDevice();
}

void Menu_NandDevice(void)
{
	s32 selected = 0;
	s32 ret;

	/* Disable NAND emulator */
	if (ndev) {
		Nand_Unmount(ndev);
		Nand_Disable();
	}

	/* Select source device */
	if (gConfig.nandDeviceIndex < 0)
	{
		/* Select source device */
		for (;;) {
			/* Clear console */
			Con_Clear();

			/* Selected device */
			ndev = &ndevList[selected];

			printf("\t>> Select NAND emulator device: < %s >\n\n", ndev->name);

			printf("\t   Press LEFT/RIGHT to change the selected device.\n\n");

			printf("\t   Press A button to continue.\n");
			printf("\t   Press HOME button to restart.\n\n");

			u32 buttons = WaitButtons();

			/* LEFT/RIGHT buttons */
			if (buttons & WPAD_BUTTON_LEFT) {
				if ((--selected) <= -1)
					selected = (NB_NAND_DEVICES - 1);
			}
			if (buttons & WPAD_BUTTON_RIGHT) {
				if ((++selected) >= NB_NAND_DEVICES)
					selected = 0;
			}

			/* HOME button */
			if (buttons & WPAD_BUTTON_HOME)
				Restart();

			// Button 1 means to escape to top
			if (buttons & WPAD_BUTTON_1)
			{
				gEscapeToTop = true;
				break;
			}

			/* A button */
			if (buttons & WPAD_BUTTON_A)
				break;
		}
	}
	else
	{
		ndev = &ndevList[gConfig.nandDeviceIndex];
	}

	if (gEscapeToTop == false)
	{
		/* No NAND device */
		if (!ndev->mode)
			return;

		printf("[+] Enabling NAND emulator...");
		fflush(stdout);

		/* Mount NAND device */
		ret = Nand_Mount(ndev);
		if (ret < 0) {
			printf(" ERROR! (ret = %d)\n", ret);
			goto err;
		}

		/* Enable NAND emulator */
		ret = Nand_Enable(ndev);
		if (ret < 0) {
			printf(" ERROR! (ret = %d)\n", ret);
			goto err;
		} else
			printf(" OK!\n");
	}

	return;

err:
	WiiLightControl (WII_LIGHT_OFF);
	printf("\n");
	printf("    Press any button to continue...\n");

	WaitButtons();

	/* Prompt menu again */
	Menu_NandDevice();
}

void Menu_WadManage(fatFile *file)
{
	FILE *fp  = NULL;

	f32 filesize;
	u32 mode = 0;

	/* File size in megabytes */
	filesize = (file->filestat.st_size / MB_SIZE);

	for (;;) {
		/* Clear console */
		Con_Clear();

		printf("[+] WAD Filename : %s\n",          file->filename);
		printf("    WAD Filesize : %.2f MB\n\n\n", filesize);


		printf("[+] Select action: < %s WAD >\n\n", (!mode) ? "Install" : "Uninstall");

		printf("    Press LEFT/RIGHT to change selected action.\n\n");

		printf("    Press A to continue.\n");
		printf("    Press B to go back to the menu.\n\n");

		u32 buttons = WaitButtons();

		/* LEFT/RIGHT buttons */
		if (buttons & (WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT))
			mode ^= 1;

		/* A button */
		if (buttons & WPAD_BUTTON_A)
			break;

		/* B button */
		if (buttons & WPAD_BUTTON_B)
			return;
	}

	/* Clear console */
	Con_Clear();

	printf("[+] Opening \"%s\", please wait...", file->filename);
	fflush(stdout);

	/* Open WAD */
	fp = fopen(file->filename, "rb");
	if (!fp) {
		printf(" ERROR!\n");
		goto out;
	} else
		printf(" OK!\n\n");

	printf("[+] %s WAD, please wait...\n", (!mode) ? "Installing" : "Uninstalling");

	/* Do install/uninstall */
	WiiLightControl (WII_LIGHT_ON);
	if (!mode)
		Wad_Install(fp);
	else
		Wad_Uninstall(fp);
	WiiLightControl (WII_LIGHT_OFF);

out:
	/* Close file */
	if (fp)
		fclose(fp);

	printf("\n");
	printf("    Press any button to continue...\n");

	/* Wait for button */
	WaitButtons();
}

void Menu_WadList(void)
{
	fatFile *fileList = NULL;
	u32      fileCnt;

	s32 selected = 0, start = 0;
	s32 ret;

	printf("[+] Retrieving file list...");
	fflush(stdout);

	// Change to configured startupPath
	if (gConfig.startupPath[0] != 0)
		Fat_ChangeDir(gConfig.startupPath);


	/* Retrieve filelist */
	ret = __Menu_RetrieveList(&fileList, &fileCnt);
	if (ret < 0) {
		printf(" ERROR! (ret = %d)\n", ret);
		goto err;
	}

	/* No files */
	if (!fileCnt) {
		printf(" No files/directories found!\n");
		goto err;
	}

	for (;;) {
		u32 cnt;
		s32 index;

		/* Clear console */
		Con_Clear();

		/** Print entries **/
		printf("[++] Filebrowser:\n\n");

		/* Print entries */
		for (cnt = start; cnt < fileCnt; cnt++) {
			fatFile *file  = &fileList[cnt];
			f32      fsize = (file->filestat.st_size / MB_SIZE);

			/* Entries per page limit */
			if ((cnt - start) >= ENTRIES_PER_PAGE)
				break;

			/* Print filename */
			printf("\t%2s %-42s", (cnt == selected) ? ">>" : "  ", file->filename);

			/* Print stats */
			if (file->filestat.st_mode & S_IFDIR)
				printf(" (DIR)\n");
			else
				printf(" (%.2f MB)\n", fsize);
		}

		printf("\n");

		printf("    Press A button to open a directory or a WAD file.\n");
		printf("    Press B button to select the storage device.\n");

		/** Controls **/
		u32 buttons = WaitButtons();

		/* D-PAD buttons */
		if (buttons & (WPAD_BUTTON_UP | WPAD_BUTTON_LEFT)) {
			selected -= (buttons & WPAD_BUTTON_LEFT) ? ENTRIES_PER_PAGE : 1;

			if (selected <= -1)
				selected = (fileCnt - 1);
		}
		if (buttons & (WPAD_BUTTON_DOWN | WPAD_BUTTON_RIGHT)) {
			selected += (buttons & WPAD_BUTTON_RIGHT) ? ENTRIES_PER_PAGE : 1;

			if (selected >= fileCnt)
				selected = 0;
		}

		/* HOME button */
		if (buttons & WPAD_BUTTON_HOME)
			Restart();

		// If "1" is pressed, go to the top level menu
		if (buttons & WPAD_BUTTON_1)
		{
			// Set flag to escape to the top
			gEscapeToTop = true;

			// Make the user select everything all over again
			gConfig.cIOSVersion = CIOS_VERSION_INVALID;
			gConfig.nandDeviceIndex = NAND_DEVICE_INDEX_INVALID;
			gConfig.fatDeviceIndex = FAT_DEVICE_INDEX_INVALID;
			// free (tmpPath);
			return;
		}

		/* A button */
		if (buttons & WPAD_BUTTON_A) {
			fatFile *file = &fileList[selected];

			/* Manage entry */
			if (file->filestat.st_mode & S_IFDIR) {
				/* Change directory */
				ret = __Menu_ChangeDir(file, &fileList, &fileCnt);
				if (ret < 0) {
					printf("\n");
					printf("[+] ERROR: Could not change the directory! (ret = %d)\n", ret);

					break;
				}

				/* Reset cursor */
				selected = 0;
			} else
				Menu_WadManage(file);
		}

		/* B button */
		if (buttons & WPAD_BUTTON_B)
			return;

		/** Scrolling **/
		/* List scrolling */
		index = (selected - start);

		if (index >= ENTRIES_PER_PAGE)
			start += index - (ENTRIES_PER_PAGE - 1);
		if (index <= -1)
			start += index;
	}

err:
	printf("\n");
	printf("    Press any button to continue...\n");

	/* Wait for button */
	WaitButtons();
}


void Menu_Loop(void)
{
	//u8 iosVersion;
	u8 iosVersion = 0;

	while (1)
	{
		gEscapeToTop = false;

		/* Select IOS menu */
		Menu_SelectIOS();

		/* Retrieve IOS version */
		if (gEscapeToTop == false)
			iosVersion = IOS_GetVersion();

		/* NAND device menu */
		if (gEscapeToTop == false)
			if (iosVersion == CIOS_VERSION)
				Menu_NandDevice();

		//for (;;) {
		while (gEscapeToTop == false)
		{
			/* FAT device menu */
			Menu_FatDevice();

			/* WAD list menu */
			Menu_WadList();
		}
	}
}

void WaitPrompt (char *prompt)
{
	printf("\n%s", prompt);
	printf("    Press any button to continue...\n");

	/* Wait for button */
	WaitButtons();
}

u32 Pad_GetButtons(void)
{
	u32 buttons = 0, cnt;

	/* Scan pads */
	PAD_ScanPads();

	/* Get pressed buttons */
	//for (cnt = 0; cnt < MAX_WIIMOTES; cnt++)
	for (cnt = 0; cnt < 4; cnt++)
		buttons |= PAD_ButtonsDown(cnt);

	return buttons;
}


// Routine to wait for a button from either the Wiimote or a gamecube
// controller. The return value will mimic the WPAD buttons to minimize
// the amount of changes to the original code, that is expecting only
// Wiimote button presses. Note that the "HOME" button on the Wiimote
// is mapped to the "SELECT" button on the Gamecube Ctrl. (wiiNinja 5/15/2009)
u32 WaitButtons(void)
{
	u32 buttons = 0;
    u32 buttonsGC = 0;

	/* Wait for button pressing */
	while (!buttons && !buttonsGC)
    {
        // GC buttons
        buttonsGC = Pad_GetButtons ();

        // Wii buttons
		buttons = Wpad_GetButtons();

		VIDEO_WaitVSync();
	}

    if (buttonsGC)
    {
        if(buttonsGC & PAD_BUTTON_A)
        {
            //printf ("Button A on the GC controller\n");
            buttons |= WPAD_BUTTON_A;
        }
        else if(buttonsGC & PAD_BUTTON_B)
        {
            //printf ("Button B on the GC controller\n");
            buttons |= WPAD_BUTTON_B;
        }
        else if(buttonsGC & PAD_BUTTON_LEFT)
        {
            //printf ("Button LEFT on the GC controller\n");
            buttons |= WPAD_BUTTON_LEFT;
        }
        else if(buttonsGC & PAD_BUTTON_RIGHT)
        {
            //printf ("Button RIGHT on the GC controller\n");
            buttons |= WPAD_BUTTON_RIGHT;
        }
        else if(buttonsGC & PAD_BUTTON_DOWN)
        {
            //printf ("Button DOWN on the GC controller\n");
            buttons |= WPAD_BUTTON_DOWN;
        }
        else if(buttonsGC & PAD_BUTTON_UP)
        {
            //printf ("Button UP on the GC controller\n");
            buttons |= WPAD_BUTTON_UP;
        }
        else if(buttonsGC & PAD_BUTTON_START)
        {
            //printf ("Button START on the GC controller\n");
            buttons |= WPAD_BUTTON_HOME;
        }
		else if(buttonsGC & PAD_BUTTON_Y)
        {
            //printf ("Button Y on the GC controller\n");
            buttons |= WPAD_BUTTON_1;
        }
		else if(buttonsGC & PAD_BUTTON_X)
        {
            //printf ("Button X on the GC controller\n");
            buttons |= WPAD_BUTTON_2;
        }
    }

	return buttons;
} // WaitButtons


void WiiLightControl (int state)
{
	switch (state)
	{
		case WII_LIGHT_ON:
			/* Turn on Wii Light */
			WIILIGHT_SetLevel(255);
			WIILIGHT_TurnOn();
			break;

		case WII_LIGHT_OFF:
		default:
			/* Turn off Wii Light */
			WIILIGHT_SetLevel(0);
			WIILIGHT_TurnOn();
			WIILIGHT_Toggle();
			break;
	}
} // WiiLightControl

