# Build instructions for Visual Studio 2017

- [Install necessary software](#install-necessary-software)
- [Prepare folder](#prepare-folder)
- [Install third party software](#install-third-party-software)
- [Clone source code and prepare libraries](#clone-source-code-and-prepare-libraries)
- [Build the project](#build-the-project)
- [Qt Visual Studio Tools](#qt-visual-studio-tools)

## Install necessary software

* Install Visual Studio 2017
  * Currently Bettergram will not build with version 15.7, so you must install a previous version of Visual Studio. You can find previous versions here: https://docs.microsoft.com/en-us/visualstudio/productinfo/installing-an-earlier-release-of-vs2017 (this problem may be resolved in later builds as well)
  * Make sure that under Workloads "Desktop development with C++" is selected
  * Make sure that under Individual Components -> Compilers, build tools and runtimes "Windows XP Support for C++" is selected as well as the V140 toolset for desktop
  * If Visual Studio is already installed, go to Tools -> Get Tools and Features... and make sure that the above options are installed. If they aren't, select them and click Modify
* Install Git and make sure it is added to the PATH
* Ensure that you have Windows 10 SDK 10.0.16299 installed
  * If you installed version 15.6 of Visual Studio 2017, this is already installed
  * If you need to figure out if this is installed or not, you can create a new Windows Desktop C++ application, and then go into project properties and look at the target SDK dropdown. If 10.0.16299 appears in the list, then it is installed on your machine
  * If it isn't installed, download and install it: https://developer.microsoft.com/en-us/windows/downloads/sdk-archive

## Prepare folder

Choose an empty folder for the future build, for example **C:\\Bettergram**. It will be referred to as ***BuildPath*** in the rest of this document. Inside of ***BuildPath*** create a ***BuildPath*\\ThirdParty** folder. The ***BuildPath*** folder cannot have any spaces in the entire path or the build process will fail.

Create a ***BuildPath*\\TelegramPrivate** folder. Obtain a copy of **custom_api_id.h** and place it into the **TelegramPrivate** folder. This file will not be stored in the repository, but it is necessary to create a build of the project (see instructions below for how to build a non-commercial build of the project without having access to this file).

All commands will be launched from **x86 Native Tools Command Prompt for VS 2017.bat** (should be in **Start Menu > Visual Studio 2017** menu folder). Pay attention not to use any other Command Prompt.

## Install third party software

* **†** is used to demarcate software that can only be installed once on your system that you might not want to install under ***BuildPath*\\ThirdParty** as directed. You may have good reason to install this software to a more common location. This is just fine as long as the software is added to the PATH, and a junction to the proper path of the software is created in the ***BuildPath*\\ThirdParty** folder.
  * Help with creating Junctions can be found here: https://www.howtogeek.com/howto/16226/complete-guide-to-symbolic-links-symlinks-on-windows-or-linux/
* **†** Download **ActivePerl** installer from https://www.activestate.com/activeperl/downloads and install to ***BuildPath*\\ThirdParty\\Perl**  
* Download **NASM** installer from http://www.nasm.us and install to ***BuildPath*\\ThirdParty\\NASM**
* Download **Yasm** executable from http://yasm.tortall.net/Download.html, rename to *yasm.exe* and move it to ***BuildPath*\\ThirdParty\\yasm**
  * Try running yasm.exe. If it gives you the error that you are missing MSVCR100.dll, then you have to download and install the Visual C++ 2010 Redistributable Package from http://www.microsoft.com/en-us/download/details.aspx?id=13523
  * If it simply opens and closes a CMD window when you try running it, then it's fine
* Download **MSYS2** installer from http://www.msys2.org/ and install to ***BuildPath*\\ThirdParty\\msys64**
* Download **jom** archive from http://download.qt.io/official_releases/jom/jom.zip and unzip into ***BuildPath*\\ThirdParty\\jom**
* **†** Download **Python 2.7** installer from https://www.python.org/downloads/ and install to ***BuildPath*\\ThirdParty\\Python27**
* **†** Download **CMake** installer from https://cmake.org/download/ and install to ***BuildPath*\\ThirdParty\\cmake**
  * Presently the latest version (3.11.3) of CMake is failing to build the openal-soft project
  * It is working with CMake version 3.10.3, so install that version to avoid problems
* Download **Ninja** executable from https://github.com/ninja-build/ninja/releases/download/v1.7.2/ninja-win.zip and unzip into ***BuildPath*\\ThirdParty\\Ninja**

Open **x86 Native Tools Command Prompt for VS 2017.bat**, go to ***BuildPath*** and run

    cd ThirdParty
    git clone https://chromium.googlesource.com/external/gyp
    cd gyp
    git checkout a478c1ab51
then close the window

Add **GYP** and **Ninja** to your PATH:

* Open **Control Panel** -> **System** -> **Advanced system settings**
* Press **Environment Variables...**
* Select **Path**
* Press **Edit**
* Add ***BuildPath*\\ThirdParty\\gyp** value
* Add ***BuildPath*\\ThirdParty\\Ninja** value
* And please ensure that **perl** is in the **PATH** variable.
If you have installed it in the ***BuildPath*** directory, then add ***BuildPath*\\ThirdParty\\Perl** to the **PATH** variable.

## Clone source code and prepare libraries

Open **x86 Native Tools Command Prompt for VS 2017.bat**, go to ***BuildPath*** and run:

    git clone --recursive https://github.com/bettergram/bettergram.git
    copy bettergram\build_bettergram.bat .
    build_bettergram

## Build the project

If you want to pass a build define (like `TDESKTOP_DISABLE_AUTOUPDATE` or `TDESKTOP_DISABLE_NETWORK_PROXY`), call `set TDESKTOP_BUILD_DEFINES=TDESKTOP_DISABLE_AUTOUPDATE,TDESKTOP_DISABLE_NETWORK_PROXY,...` (comma seperated string)

After, call **gyp\refresh.bat** once again.

* Open ***BuildPath*\\bettergram\\Telegram\\Telegram.sln** in Visual Studio 2017
* Select Telegram project and press Build > Build Telegram (Debug and Release configurations)
* The result Telegram.exe will be located in ***BuildPath*\bettergram\out\Debug** (and **Release**)

If you do not have access to **custom_api_id.h**, you can build a test version of the project by editing the **Telegram.gyp** file and commenting out the define: **CUSTOM_API_ID** (be careful not to check this change in to the repository). Then you must execute the **refresh.bat** file before building. This build should only be used for testing.

### Qt Visual Studio Tools

For better debugging you may want to install Qt Visual Studio Tools:

* Open **Tools** -> **Extensions and Updates...**
* Go to **Online** tab
* Search for **Qt**
* Install **Qt Visual Studio Tools** extension
