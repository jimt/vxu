**vxu** allows reading from and **vxuw** allows writing to a Yaesu / Vertex Standard
VX-5R (and similar) handheld radios and the FT-817/FT-857 transceivers in "clone" mode.
These command line utilities allow easy backup of the rig memory.

## Name

vxur,vxuw - VerteX Standard (Yaesu) Read and Write for Unix  

## SYNOPSIS
vxur [options...] file
vxuw [options...] file  

## DESCRIPTION

vxur is a small program that receives data from a Yaesu radio in clone mode and saves it to a file. vxuw reads the specified file and sends it to the radio.  

## OPTIONS
<dl>
<dt>-d, --device</dt>
<dd>Specify the serial device to use. (default /dev/ttyS0)</dd>
<dt>-h, --hash</dt>
<dd>Echo a '.' for each block received (sent) and a '#' for each 200 bytes of data.</dd>
<dt>-v, --verbose</dt>
<dd>Display program information.</dd>
<dt>-?, --help</dt>
<dd>Display the command line options and exit.</dd>
</dl> 

### Upload FROM Radio

- turn on the radio in clone mode (for example, with the VX-5R, hold down FW button while turning on)
- start **vxur**
- tell the radio to start sending by pressing the VFO button  

### Download TO Radio

- turn on the radio in clone mode
- tell the radio to start waiting for data by pressing the MR button
- start download with **vxuw** (e.g. `vxuw -d /dev/ttyS1 myfile.eve`, or for OS X `vxuw -d /dev/cu.USBAxx -h myfile.eve`)  

## AUTHOR
Jim Tittsler <7J1AJH@OnJapan.net>

Original Windows yard/yawr utilities by Jose R. Camara <camaraya at quatacorp.com>

