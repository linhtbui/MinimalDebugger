# MinimalDebugger
## Authors: Eli Salm, Zoe Grubbs, Mujtaba Aslam

A minimal deubgger that catches segmentation faults and provides useful information to help the user debug them. The debugger also provides an option to set a breakpoint before the prgram is executed.  

Usage:
`./gdb ./[program]` where program is the program to be debugged. For example, run `make` in the src directory and then `./gdb ./seg`

# Library Dependencies
`libelfin` which can be found at https://github.com/aclements/libelfin

# Resources Used:
* http://system.joekain.com/debugger/
* http://t-a-w.blogspot.com/2007/03/how-to-code-debuggers.html
* https://blog.tartanllama.xyz/writing-a-linux-debugger-setup/
* https://eli.thegreenplace.net/2011/01/23/how-debuggers-work-part-1/
* https://eli.thegreenplace.net/2011/01/27/how-debuggers-work-part-2-breakpoints/
* https://eli.thegreenplace.net/2011/02/07/how-debuggers-work-part-3-debugging-information

Note: We always worked together on this project so all the commits reflect
work from all three of us
