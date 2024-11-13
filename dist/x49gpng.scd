X49GPNG(1)

# NAME

x49gpng — Emulator for HP 49g+ / 50g calculators

# SYNOPSIS

*x49gpng* <options>

# DESCRIPTION

*x49gpng* is an emulator capable of running any software written for the HP 49g+  and  HP  50g calculators,  including  ARM  programs  or even custom firmware. It achieves this by using a modified version of QEMU to emulate the ARM hardware based around the Samsung S3C2410 chip.

## Input

For  mouse  input, the keys shown in the window can be interacted with; left-clicking on one holds it down for as long as the mouse button is down, right-clicking on one holds  it  down until any key is left-clicked.

Another method of interacting with the calculator is through a physical keyboard.  _A-Z_, _0-9_, _F1-F6_,  _cursor keys_, _'_, _Backspace_, _^_, _+_, _-_, _\*_, _/_, _._, _Space_, _Enter_ correspond to their sensible calculator counterparts. In addition, _Delete_ acts as extra Backspace key, _,_ acts as  extra  decimal separator key, _Shift_ presses the Leftshift key (green on the 49g+, white on the 50g), _Control_ presses the Rightshift key (red on the 49g+, orange on the 50g),  _Tab+  presses the  Alpha key (yellow on the 49g+ and 50g), and _Escape_ presses the On key. Note that in order to allow multiple-key combinations involving e.g.   _Shift+  without  surprises,  such  as pressing  _Shift-8_  on  a  US  QWERTY  keyboard  producing the key sequence “Leftshift-hold multiply”, *x49gpng* ignores all keyboard modifier states except  NumLock.  Because  that  may make certain virtual keys inaccessible to a laptop keyboard without a numpad, there are certain additional key definitions to support at least some common keyboard layouts:

US, UK, International QWERTY: _\\_ acts as \* key, _=_ acts as + key.

German QWERTZ: _#_ acts as \* key, _ß_ acts as / key.

(Further  suggestions  are  welcome.) These additional key definitions do not need their respective layouts to work, so e.g.  _\\_ will act as \* on any keyboard that has  it  as  an  unshifted key.

Pressing  physical  keys  and clicking on virtual ones can be freely combined. Right-clicked keys are released when no keys are held down by the left mouse button or physical keys  anymore. To avoid stuck keys, *x49gpng* forcibly releases all keys when losing focus.

There  are  more remaining miscellaneous key definitions:
- _F7_ or _F10_ closes *x49gpng* and saves the configuration, CPU registers, hardware registers, and memory contents to their respective files;
- _F12_ resets the calculator, like inserting a paperclip into the tiny hole at the back of the real device or removing all of its power sources (batteries, USB) would;
- _Menu_ opens a popup menu granting access to several emulator control items. This popup menu can also be opened by  right-clicking  on  the  screen area.

## The popup menu

The  first group of items in the popup menu deals with the calculator's SD card. The SD card can either be backed by a directory on the host system (item *Mount SD  folder...*;  *x49gpng* will  simulate  a  FAT-formatted file system on the fly), or by an image file (item *Mount SD image...*).  The former is more convenient for general use, but when debugging a  hypothetical  replacement  firmware's  SD driver, the latter variant's accuracy (it allows corrupting the filesystem, for instance) might be more desirable. The virtual SD card can be absent entirely, too (item *Unmount SD*).

The second group consists of only one item, and it is only visible when one of the debug options has been used while starting *x49gpng*.  Named *Start debugger*, it  (re-)starts  the  GDB remote  debugging  interface. This is hidden by default because accidental clicks will seemingly freeze the emulator while it waits for an incoming connection.

The third and final group contains two items: *Reset*, which resets the calculator  just  like the  _F12_  key,  and  *Quit*,  which closes *x49gpng* and saves the configuration, CPU registers, hardware registers, and memory contents to their respective files. This saving step is  also performed  when *x49gpng* is closed in another way, e.g. pressing _Control-C_ in the controlling terminal.

## First launch

During the first launch, *x49gpng* generates a new configuration with default values and empty memory.  *x49gpng* does not ship with a default firmware, only with the  bootloaders  for  the supported  calculators.  Hence,  the  user is required to select a firmware to load into the flash.  *x49gpng* will show a file selection window for that task. For the stock  firmware  HP has  always  used the file extension “.bin”, any other files (including the update.scp files that typically come with them!) are generally not actual firmware files.

When the file selection dialog is cancelled or the file could not be read, *x49gpng* creates a flash without firmware; the bootloader will complain and offer ways to correct the situation just like it would on a real device, which include receiving a firmware via  USB  (currently not  available  in  *x49gpng*,  see _Caveats_ below) or loading one from the SD card. Apart from these options, it's also possible to recover from this by exiting *x49gpng* and either  delet‐ ing  the  flash  file  or using one of the options *-f* and *-F*, causing *x49gpng* to rebuild the flash and ask for a firmware again.

Please consult HP's manual (for their official firmware) or the documentation of the  custom firmware (if using one) for an explanation of using the calculator.

# OPTIONS

*-h* *--help*                    print this message and exit
*--verbose*                 print out more information
*--state*[=_filename_]      alternate config file
*--50g*                     emulate an HP 50g (default)
*--49gp*                    emulate an HP 49g+
*--newrpl-keyboard*         label keyboard for newRPL
*-n* *--name*[=_name_]           set alternate UI name
*-t* *--font*[=_fontname_]       set alternate UI font
*-s* *--font-size*[=_X_]         scale text by X (default: 3)
*-S* *--display-scale*[=_X_]     scale LCD by X (default: 2)
*-D* *--enable-debug*[=_port_]   enable the debugger interface (default port: 1234)
*-d* *--debug*                   use along -D to also start the debugger immediately
*-f* *--reflash*[=_firmware_]      rebuild the flash using the supplied firmware (default: select one interactively) (implies -r for safety reasons)
*-F* *--reflash-full*            use along -f to drop the flash contents in the area beyond the firmware
*-r* *--reboot*                  reboot on startup instead of continuing from the saved state in the state file

# FILES

All files are located under ~/.config/x49gpng/

- *config.lua*

The general configuration file of *x49gpng*. Content reflecting default values (as printed by running *x49gpng --print-config*):

```
--------------------------------------------------------------------------------
-- Configuration file for x49gpng
-- This is a comment
name = "HP 50g" -- this customize the title of the window
model = "50g" -- possible values: "49gp", "50g". Changes the colors and the bootloader looked for when (re-)flashing
newrpl_keyboard = false -- when true this makes the keyboard labels more suited to newRPL use
font = "urw gothic l"
font_size = 12 -- integer only
display_scale = 2 -- integer only
verbose = false
--- End of saturnng configuration ----------------------------------------------
```

- *state*

The  default  location  for  the configuration file. The file format is the INI-like “key-file” format defined by GLib. The configuration file stores, for the most part, the state of the calculator's hardware, excluding the memory. The peripherals' offi‐ cial names according the the S3C2410 datasheet are used as section  names  with  the prefix “s3c2410-”, the respective register names are the keys used in a section, and the  values are the numbers stored in these registers. For the ARM processor itself, the section name is s3c2410-arm, and the keys correspond to the names of  attributes in the QEMU CPU state structures.

In   addition,  there  are  keys  called  _filename_  in  the  sections  _flash_,  _sram_, _s3c2410-sram_, _s3c2410-sdi_ which allow changing the filename of the files  where  the state  of the respective memory is held. These can be absolute paths, or paths rela‐ tive to the location of the configuration file (in the simplest case, only  a  file‐ name).  Any files that do not exist will be created with appropriate size and place‐ holder content. For _s3c2410-sdi_, the empty string is accepted as a special value for no inserted SD card.

- *flash*

The default name of the file backing the flash memory; required size: 2 MiB. If this file does not exist or is too small, the calculator  type's  appropriate  bootloader will  be  copied  into  the first 16 KiB. Do NOT simply put a firmware file here, it will not work. Just use the builtin firmware installation mechanism instead.

- *sram*

The default name of the file backing the SRAM memory; required size: 512 KiB.

- *s3c2410-sram*

The default name of the file backing the S3C2410's internal  SRAM  memory;  required size: 4 KiB.

# CAVEATS

The emulation of the calculators' hardware is not fully accurate. Most notably, emulation speed is not authentic (it depends on the host's capabilites), and communication ports (serial, infrared, USB) are not available. For most purposes this is good enough, though.