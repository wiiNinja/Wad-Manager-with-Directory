#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>

#include "fat.h"
#include "nand.h"
#include "restart.h"
#include "title.h"
#include "usbstorage.h"
#include "utils.h"
#include "video.h"
#include "wad.h"
#include "wpad.h"
#include <ogc/pad.h>
#include "globals.h"



/* FAT device list  */
//static fatDevice fdevList[] = {
fatDevice fdevList[] = {
	{ "sd",		"Wii SD Slot",			&__io_wiisd },
	{ "usb",	"USB Mass Storage Device",	&__io_usbstorage },
	{ "usb2",	"USB 2.0 Mass Storage Device",	&__io_usb2storage },
	{ "gcsda",	"SD Gecko (Slot A)",		&__io_gcsda },
	{ "gcsdb",	"SD Gecko (Slot B)",		&__io_gcsdb },
};

/* NAND device list */
static nandDevice ndevList[] = {
	{ "Disable",				0,	0x00,	0x00 },
	{ "SD/SDHC Card",			1,	0xF0,	0xF1 },
	{ "USB 2.0 Mass Storage Device",	2,	0xF2,	0xF3 },
};


/* FAT device */
static fatDevice  *fdev = NULL;
static nandDevice *ndev = NULL;

// wiiNinja: Define a buffer holding the previous path names as user
// traverses the directory tree. Max of 10 levels is define at this point
static u8 gDirLevel = 0;
static char gDirList [MAX_DIR_LEVELS][MAX_FILE_PATH_LEN];
static s32  gSeleted[MAX_DIR_LEVELS];
static s32  gStart[MAX_DIR_LEVELS];

/* Macros */
#define NB_FAT_DEVICES		(sizeof(fdevList) / sizeof(fatDevice))
#define NB_NAND_DEVICES		(sizeof(ndevList) / sizeof(nandDevice))


// Local prototypes: wiiNinja
void WaitPrompt (char *prompt);
int PushCurrentDir(char *dirStr, s32 Selected, s32 Start);
char *PopCurrentDir(s32 *Selected, s32 *Start);
bool IsListFull (void);
char *PeekCurrentDir (void);
u32 WaitButtons(void);
u32 Pad_GetButtons(void);

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

	/* Compare entries */ // wiiNinja: Include directory
    if ((f1->filestat.st_mode & S_IFDIR) && !(f2->filestat.st_mode & S_IFDIR))
        return (-1);
    else if (!(f1->filestat.st_mode & S_IFDIR) && (f2->filestat.st_mode & S_IFDIR))
        return (1);
    else
        return strcmp(f1->filename, f2->filename);
}

char gFileName[MAX_FILE_PATH_LEN];
s32 __Menu_RetrieveList(char *inPath, fatFile **outbuf, u32 *outlen)
{
	fatFile  *buffer = NULL;
	DIR_ITER *dir    = NULL;

	struct stat filestat;

	//char dirpath[256], filename[768];
	u32  cnt;

	/* Generate dirpath */
	//sprintf(dirpath, "%s:" WAD_DIRECTORY, fdev->mount);

	/* Open directory */
	dir = diropen(inPath);
	if (!dir)
		return -1;

	/* Count entries */
	for (cnt = 0; !dirnext(dir, gFileName, &filestat);) {
		// if (!(filestat.st_mode & S_IFDIR)) // wiiNinja
			cnt++;
	}

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
		for (cnt = 0; !dirnext(dir, gFileName, &filestat);)
		{
			bool addFlag = false;
			
			if (filestat.st_mode & S_IFDIR)  // wiiNinja
            {
                // Add only the item ".." which is the previous directory
                // AND if we're not at the root directory
                if ((strcmp (gFileName, "..") == 0) && (gDirLevel > 1))
                    addFlag = true;
                else if (strcmp (gFileName, ".") != 0)
                    addFlag = true;
            }
            else
			{
				if(strlen(gFileName)>4)
				{
					if(!stricmp(gFileName+strlen(gFileName)-4, ".wad"))
						addFlag = true;
				}
			}

            if (addFlag == true)
            {
				fatFile *file = &buffer[cnt++];

				/* File name */
				strcpy(file->filename, gFileName);

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


void Menu_SelectIOS(void)
{
	u8 *iosVersion = NULL;
	u32 iosCnt;

	u32 cnt;
	s32 ret, selected = 0;

	/* Get IOS versions */
	ret = Title_GetIOSVersions(&iosVersion, &iosCnt);
	if (ret < 0)
		return;

	/* Sort list */
	qsort(iosVersion, iosCnt, sizeof(u8), __Menu_IsGreater);

	/* Set default version */
	for (cnt = 0; cnt < iosCnt; cnt++) {
		u8 version = iosVersion[cnt];

		/* Custom IOS available */
		if (version == CIOS_VERSION) {
			selected = cnt;
			break;
		}

		/* Current IOS */
		if (version == IOS_GetVersion())
			selected = cnt;
	}

	/* Ask user for IOS version */
	for (;;) {
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

		/* A button */
		if (buttons & WPAD_BUTTON_A)
			break;
	}


	u8 version = iosVersion[selected];

	if (IOS_GetVersion() != version) {
		/* Shutdown subsystems */
		Wpad_Disconnect();

		/* Load IOS */
		ret = IOS_ReloadIOS(version);

		/* Initialize subsystems */
		Wpad_Init();
	}
}

void Menu_FatDevice(void)
{
	s32 ret, selected = 0;

	/* Unmount FAT device */
	if (fdev)
		Fat_Unmount(fdev);

	/* Select source device */
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

		/* A button */
		if (buttons & WPAD_BUTTON_A)
			break;
	}

	printf("[+] Mounting device, please wait...");
	fflush(stdout);

	/* Mount FAT device */
	ret = Fat_Mount(fdev);
	if (ret < 0) {
		printf(" ERROR! (ret = %d)\n", ret);
		goto err;
	} else
		printf(" OK!\n");

	return;

err:
	printf("\n");
	printf("    Press any button to continue...\n");

	WaitButtons();

	/* Prompt menu again */
	Menu_FatDevice();
}

void Menu_NandDevice(void)
{
	s32 ret, selected = 0;

	/* Disable NAND emulator */
	if (ndev) {
		Nand_Unmount(ndev);
		Nand_Disable();
	}

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

		/* A button */
		if (buttons & WPAD_BUTTON_A)
			break;
	}

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

	return;

err:
	printf("\n");
	printf("    Press any button to continue...\n");

	WaitButtons();

	/* Prompt menu again */
	Menu_NandDevice();
}

char gTmpFilePath[MAX_FILE_PATH_LEN];
void Menu_WadManage(fatFile *file, char *inFilePath)
{
	FILE *fp  = NULL;

	//char filepath[128];
	f32  filesize;

	u32  mode = 0;

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

	/* Generate filepath */
	// sprintf(filepath, "%s:" WAD_DIRECTORY "/%s", fdev->mount, file->filename);
	sprintf(gTmpFilePath, "%s/%s", inFilePath, file->filename); // wiiNinja

	/* Open WAD */
	fp = fopen(gTmpFilePath, "rb");
	if (!fp) {
		printf(" ERROR!\n");
		goto out;
	} else
		printf(" OK!\n\n");

	printf("[+] %s WAD, please wait...\n", (!mode) ? "Installing" : "Uninstalling");

	/* Do install/uninstall */
	if (!mode)
		Wad_Install(fp);
	else
		Wad_Uninstall(fp);

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
	char str [100];
	fatFile *fileList = NULL;
	u32      fileCnt;
	s32 ret, selected = 0, start = 0;
    char *tmpPath = malloc (MAX_FILE_PATH_LEN);

    // wiiNinja: check for malloc error
    if (tmpPath == NULL)
    {
        ret = -999; // What am I gonna use here?
		printf(" ERROR! Out of memory (ret = %d)\n", ret);
        return;
    }

	printf("[+] Retrieving file list...");
	fflush(stdout);
	
	gDirLevel = 0;

    // wiiNinja: The root is always the primary folder
    // But if the user has a /wad directory, just go there. This makes
    // both sides of the argument win
	sprintf(tmpPath, "%s:" WAD_DIRECTORY, fdev->mount);
    PushCurrentDir(tmpPath,0,0);
	//if (strcmp (WAD_DIRECTORY, WAD_ROOT_DIRECTORY) != 0)
	if (strcmp (WAD_DIRECTORY, gConfig.startupPath) != 0)
	{
        // If the directory can be successfully opened, it must exists
        //DIR_ITER *tmpDirPtr = NULL;
        //tmpDirPtr = diropen(WAD_ROOT_DIRECTORY);
        //if (tmpDirPtr)
        //{
		//	dirclose (tmpDirPtr);

            // Now push the /wad directory as the current operating folder
            //sprintf(tmpPath, "%s:" WAD_ROOT_DIRECTORY, fdev->mount);
			sprintf(tmpPath, "%s:%s", fdev->mount, gConfig.startupPath);
			//printf ("\nThe final startupPath is: %s\n", tmpPath);
			//WaitButtons ();
            PushCurrentDir(tmpPath,0,0); // wiiNinja
        //}
	}

	/* Retrieve filelist */
getList:
    if (fileList)
    {
        free (fileList);
        fileList = NULL;
    }

	ret = __Menu_RetrieveList(tmpPath, &fileList, &fileCnt);
	if (ret < 0) {
		printf(" ERROR! (ret = %d)\n", ret);
		goto err;
	}

	/* No files */
	if (!fileCnt) {
		printf(" No files found!\n");
		goto err;
	}

	for (;;) 
	{
		u32 cnt;
		s32 index;

		/* Clear console */
		Con_Clear();

		/** Print entries **/
		cnt = strlen(tmpPath);
		if(cnt>30)
			index = cnt-30;
		else
			index = 0;

		printf("[+] WAD files on [%s]:\n\n", tmpPath+index);

		/* Print entries */
		for (cnt = start; cnt < fileCnt; cnt++) 
		{
			fatFile *file     = &fileList[cnt];
			f32      filesize = file->filestat.st_size / MB_SIZE;

			/* Entries per page limit */
			if ((cnt - start) >= ENTRIES_PER_PAGE)
				break;

			strncpy(str, file->filename, 48);
			str[48]=0;

			/* Print filename */
			//printf("\t%2s %s (%.2f MB)\n", (cnt == selected) ? ">>" : "  ", file->filename, filesize);
            if (file->filestat.st_mode & S_IFDIR) // wiiNinja
				printf("\t%2s [%s]\n", (cnt == selected) ? ">>" : "  ", str);
            else
                printf("\t%2s %s (%.2f MB)\n", (cnt == selected) ? ">>" : "  ", str, filesize);

		}

		printf("\n");

		printf("[+] Press A button to (un)install a WAD file.\n");
		if(gDirLevel>1)
			printf("    Press B button to go up-level DIR");
		else
			printf("    Press B button to select a storage device");

		/** Controls **/
		u32 buttons = WaitButtons();

		/* DPAD buttons */
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

		/* A button */
		if (buttons & WPAD_BUTTON_A)
		{
			fatFile *tmpFile = &fileList[selected];
            char *tmpCurPath;
            if (tmpFile->filestat.st_mode & S_IFDIR) // wiiNinja
            {
                if (strcmp (tmpFile->filename, "..") == 0)
                {
					selected = 0;
					start = 0;

					// Previous dir
                    tmpCurPath = PopCurrentDir(&selected, &start);
                    if (tmpCurPath != NULL)
                        sprintf(tmpPath, "%s", tmpCurPath);
						
                    goto getList;
                }
                else if (IsListFull () == true)
                {
                    WaitPrompt ("Maximum number of directory levels is reached.\n");
                }
                else
                {
                    tmpCurPath = PeekCurrentDir ();
                    if (tmpCurPath != NULL)
                    {
						if(gDirLevel>1)
							sprintf(tmpPath, "%s/%s", tmpCurPath, tmpFile->filename);
						else
							sprintf(tmpPath, "%s%s", tmpCurPath, tmpFile->filename);
                    }
                    // wiiNinja: Need to PopCurrentDir
                    PushCurrentDir (tmpPath, selected, start);
					selected = 0;
					start = 0;
                    goto getList;
                }
            }
            else
            {
                tmpCurPath = PeekCurrentDir ();
                if (tmpCurPath != NULL)
                    Menu_WadManage(tmpFile, tmpCurPath);
            }
		}

		/* B button */
		if (buttons & WPAD_BUTTON_B)
		{
			if(gDirLevel<=1)
			{
				return;
			}

			char *tmpCurPath;
			selected = 0;
			start = 0;
			// Previous dir
			tmpCurPath = PopCurrentDir(&selected, &start);
			if (tmpCurPath != NULL)
				sprintf(tmpPath, "%s", tmpCurPath);
			goto getList;
			//return;
		}

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

	free (tmpPath);
	
	/* Wait for button */
	WaitButtons();
}


void Menu_Loop(void)
{
	u8 iosVersion;

	/* Select IOS menu */
	Menu_SelectIOS();

	/* Retrieve IOS version */
	iosVersion = IOS_GetVersion();

	/* NAND device menu */
	if (iosVersion == CIOS_VERSION)
		Menu_NandDevice();

	for (;;) {
		/* FAT device menu */
		Menu_FatDevice();

		/* WAD list menu */
		Menu_WadList();
	}
}

// Start of wiiNinja's added routines

int PushCurrentDir (char *dirStr, s32 Selected, s32 Start)
{
    int retval = 0;

    // Store dirStr into the list and increment the gDirLevel
    // WARNING: Make sure dirStr is no larger than MAX_FILE_PATH_LEN
    if (gDirLevel < MAX_DIR_LEVELS)
    {
        strcpy (gDirList [gDirLevel], dirStr);
		gSeleted[gDirLevel]=Selected;
		gStart[gDirLevel]=Start;
        gDirLevel++;
        //if (gDirLevel >= MAX_DIR_LEVELS)
        //    gDirLevel = 0;
    }
    else
        retval = -1;

    return (retval);
}

char *PopCurrentDir(s32 *Selected, s32 *Start)
{
	if (gDirLevel > 1)
        gDirLevel--;
    else
        gDirLevel = 0;

	*Selected = gSeleted[gDirLevel];
	*Start = gStart[gDirLevel];
	return PeekCurrentDir();
}

bool IsListFull (void)
{
    if (gDirLevel < MAX_DIR_LEVELS)
        return (false);
    else
        return (true);
}

char *PeekCurrentDir (void)
{
    // Return the current path
    if (gDirLevel > 0)
        return (gDirList [gDirLevel-1]);
    else
        return (NULL);
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
    }

	return buttons;
} // WaitButtons

