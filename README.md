# CS4398-Digital-Forensics-Project
This project is sample code for a simple data recovery tool for ext3 file systems. It
currently only supports recovery of .iso volumes, though can be easily adapted for
many file types.

## Building the Project
The contained code can be compiled and run with the provided makefile. Compile and run with:
```$ make```
- make will compile all files and output final executable scan_drive.exe

## Run the code
```$ sudo ./scan_drive.exe /dev/sdxx```

Enter the drive letter/partition in place of 'xx' that you are trying to read from.
Invalid output will result in a Usage message being displayed. The program will default to the
first partition if no partition number is provided.

For example:<br>
```$ sudo ./scan_drive.exe /dev/sdb```<br>
Defaults to the first partition of the device /dev/sdb, but<br>

```$ sudo ./scan_drive.exe /dev/sdb2```<br>
Will scan the 2nd partition.

The program requires root permissions to access the drive, use sudo or login to root.

**** NOTE ****<br>
In trying to compile from an extracted zip file, I noticed this caused some issues will file
timestamps and effected the makefile from compiling properly.

If this issue occurs, run the commands
```
$ make refresh
$ make clean
```
then recompile with make. This should reset the timestamp on every file and allow the program
to compile successfully.
