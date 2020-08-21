Required dependencies: 

Coaxlink drivers under C:\Program Files



Compiling and running the program in Visual Studio 2019:

Open Visual Studio.
 -> Choose Create a new project
 -> Search for "empry project" 
 -> Choose "Empty project" with "C++", "Windows" and "Console"
 -> Choose a projectnamename and directory, then press create
 -> In the Solution Explorer on the right, add "tools.h" from this folder to Header Files.
 -> In the Solution Explorer on the right, add "tools.cpp" and "main.cpp" from this folder to Source Files.
 -> Change x86 to x64 
 -> Under Project -> Properties -> Configuration Properties -> C/C++  In "Additional Include Directories" paste following:
    C:\Program Files\Euresys\Coaxlink\include;C:\Program Files\Euresys\Coaxlink\cti\x86_64
 -> In the subfolder "projectname", add "config.js" from this folder

To compile, press Ctrl + B
To run with the debugger, press F5
To run without the debugger, press Ctrl + F5





