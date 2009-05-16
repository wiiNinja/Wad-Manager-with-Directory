#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include <malloc.h>

#include "fat.h"

static const char FAT_SIG[3] = {'F', 'A', 'T'}; 
#define read_le32_unaligned(x) ((x)[0]|((x)[1]<<8)|((x)[2]<<16)|((x)[3]<<24))

fatDevice *mounted = NULL;

s32 Fat_Mount(fatDevice *dev)
{
	if(mounted) Fat_Unmount();

	s32 result = -1;
	
	if(dev && dev->interface->startup())
	{
		u8 *sectorBuffer = (u8*)memalign(32, 2048);
		u8 *bootBuffer = (u8*)memalign(32, 2048);
		if(dev->interface->readSectors(0, 1, sectorBuffer))
		{
			u32 lba = 0;
			if((sectorBuffer[0x1FE] == 0x55) || (sectorBuffer[0x1FF] == 0xAA)) 
			{
				int i;
				u8* ptr = sectorBuffer+0x1be;
				for(i=0;i<4;i++,ptr+=16)
				{
					lba = read_le32_unaligned(ptr+0x8);
					if(dev->interface->readSectors(lba,1,bootBuffer))
					{
						if((bootBuffer[0x1FE] == 0x55) || (bootBuffer[0x1FF] == 0xAA)) 
						{
							if(!memcmp(bootBuffer + 0x36, FAT_SIG, sizeof(FAT_SIG))) 
							{
								break;
							} 
							else if (!memcmp(bootBuffer + 0x52, FAT_SIG, sizeof(FAT_SIG))) 
							{
								break;
							}
						}
					}
					lba = 0;
				}
			}

			if(lba)
			{
				result = fatMount(dev->mount, dev->interface, lba, 8);
				if(result>=0) mounted = dev;
			}
		}

		free(bootBuffer);
		free(sectorBuffer);
		
		if(result<0) dev->interface->shutdown();
	}

	return result;
}

void Fat_Unmount()
{
	if(mounted)
	{
		/* Unmount device */
		fatUnmount(mounted->mount);

		/* Shutdown interface */
		mounted->interface->shutdown();
		mounted = NULL;
	}
}

char *Fat_ToFilename(const char *filename)
{
	static char buffer[128];

	u32 cnt, idx, len;

	/* Clear buffer */
	memset(buffer, 0, sizeof(buffer));

	/* Get filename length */
	len = strlen(filename);

	for (cnt = idx = 0; idx < len; idx++) {
		char c = filename[idx];

		/* Valid characters */
		if ( (c >= '#' && c <= ')') || (c >= '-' && c <= '.') ||
		     (c >= '0' && c <= '9') || (c >= 'A' && c <= 'z') ||
		     (c >= 'a' && c <= 'z') || (c == '!') )
			buffer[cnt++] = c;
	}

	return buffer;
}
