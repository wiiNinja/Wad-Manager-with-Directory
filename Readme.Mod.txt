I built this for my own use, but thought I'd share.

This is based from Waninkoko's Wad Manager v1.4. I added support for the
wad manager to handle folders. Use it as you would with the original wad 
manager, only now you can have up to 9 levels of folder under the "wad"
folder.

Summary:

* The sd:\wad folder now can have folders in it
* Each folder can have up to 9 directories deep
* I've tested install/uninstall wads
* I've tested traversing directories
* I noticed that some directories may appear as all upper case. I don't know why; but
  it does not interfere with the function of the wad manager, so I just leave it alone.
* I have not tried directories with spaces in the names. I don't think anything
  detrimental will happen, since the wad file names can have spaces in them

If you use it and find anomalies, please let me know.

wiiNinja
5/9/2009
------------------------------------------------------

Sorg's Enhancements:

read ReadMe.txt and ReadMe.Mod.txt before

This enhanced mod based on WAD Manager 1.4 Mod v1 by wiiNinja.

Includes enhancements:
- Device selection combined with IOS selection for shorter startup.
- Bigger console window with 16 entries per page (instead 8)
- browse from root(/) folder (intead of /wad)
- "B" button returns to parent folder until root. On root - returns to device selection.
- remember/restore list position on enter/exit directory
- remember last used IOS when back to device selection (instead forced IOS249)
- fixes for some minor bugs

Sorg.

------------------------------------------------------
5/15/2009-wiiNinja

* Added support for the GameCube controller. This is a feature most everyone won't
  use normally, but will be needed in some SHTF situations.

7/29/2009: v1.5.Mod2

* Merged Waninkoko's v1.5
* Added password
* Added startupPath
* Added handling from wm_config.txt in sd:/wad

8/1/2009: v1.5.Mod3

* Added to wm_config.txt: cIOS select, Wad Source path, and Nand source path
* Added wiilight mod by mariomaniac33

------------------------------------------------------
8/16/2010: v1.7.Mod1

* Just took what Wanin had for v1.7 and added the stuff described above.
* Added "Music" and "Disclaimer" options in wm_config.txt

