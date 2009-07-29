#ifndef _GLOBALS_H_
#define _GLOBALS_H_

// Constants
#define CIOS_VERSION		249
#define ENTRIES_PER_PAGE	16
#define MAX_FILE_PATH_LEN	1024
#define MAX_DIR_LEVELS		10
#define WAD_DIRECTORY		"/"
#define WAD_ROOT_DIRECTORY  "/wad"

#define WM_CONFIG_FILE_PATH "sd:/wad/wm_config.txt"

typedef struct 
{
	char password[11];
	char startupPath [256];
} CONFIG;


extern CONFIG gConfig;

#endif
