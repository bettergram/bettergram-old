SET PATH=%cd%\ThirdParty\NASM;%cd%\ThirdParty\jom;%cd%\ThirdParty\yasm;%PATH%

mkdir Libraries
cd Libraries

git clone https://github.com/Microsoft/Range-V3-VS2015 range-v3

git clone https://github.com/telegramdesktop/lzma.git
cd lzma\C\Util\LzmaLib
msbuild LzmaLib.sln /property:Configuration=Debug
msbuild LzmaLib.sln /property:Configuration=Release
cd ..\..\..\..

git clone https://github.com/openssl/openssl.git
cd openssl
git checkout OpenSSL_1_0_1-stable
perl Configure no-shared --prefix=%cd%\Release --openssldir=%cd%\Release VC-WIN32
call ms\do_ms
nmake -f ms\nt.mak
nmake -f ms\nt.mak install
xcopy tmp32\lib.pdb Release\lib\
nmake -f ms\nt.mak clean
perl Configure no-shared --prefix=%cd%\Debug --openssldir=%cd%\Debug debug-VC-WIN32
call ms\do_ms
nmake -f ms\nt.mak
nmake -f ms\nt.mak install
xcopy tmp32.dbg\lib.pdb Debug\lib\
cd ..

git clone https://github.com/telegramdesktop/zlib.git
cd zlib
git checkout tdesktop
cd contrib\vstudio\vc14
msbuild zlibstat.vcxproj /property:Configuration=Debug
msbuild zlibstat.vcxproj /property:Configuration=ReleaseWithoutAsm
cd ..\..\..\..

git clone git://repo.or.cz/openal-soft.git
cd openal-soft
git checkout 18bb46163af
cd build
cmake -G "Visual Studio 15 2017" -D LIBTYPE:STRING=STATIC -D FORCE_STATIC_VCRT:STRING=ON ..
msbuild OpenAL32.vcxproj /property:Configuration=Debug
msbuild OpenAL32.vcxproj /property:Configuration=Release
cd ..\..

git clone https://github.com/google/breakpad
cd breakpad
git checkout a1dbcdcb43
git apply ../../bettergram/Telegram/Patches/breakpad.diff
cd src
git clone https://github.com/google/googletest testing
cd client\windows
set GYP_MSVS_VERSION=2017
call gyp --no-circular-check breakpad_client.gyp --format=ninja
cd ..\..
ninja -C out/Debug common crash_generation_client exception_handler
ninja -C out/Release common crash_generation_client exception_handler
cd ..\..

git clone https://github.com/telegramdesktop/opus.git
cd opus
git checkout tdesktop
cd win32\VS2015
msbuild opus.sln /property:Configuration=Debug /property:Platform="Win32"
msbuild opus.sln /property:Configuration=Release /property:Platform="Win32"

cd ..\..\..\..
SET PATH_BACKUP_=%PATH%
SET PATH=%cd%\ThirdParty\msys64\usr\bin;%PATH%
cd Libraries

git clone https://github.com/FFmpeg/FFmpeg.git ffmpeg
cd ffmpeg
git checkout release/3.4

set CHERE_INVOKING=enabled_from_arguments
set MSYS2_PATH_TYPE=inherit
bash --login ../../bettergram/Telegram/Patches/build_ffmpeg_win.sh

SET PATH=%PATH_BACKUP_%
cd ..

git clone git://code.qt.io/qt/qt5.git qt5_6_2
cd qt5_6_2
perl init-repository --module-subset=qtbase,qtimageformats
git checkout v5.6.2
cd qtimageformats
git checkout v5.6.2
cd ..\qtbase
git checkout v5.6.2
git apply ../../../bettergram/Telegram/Patches/qtbase_5_6_2.diff
cd ..

call configure -debug-and-release -force-debug-info -opensource -confirm-license -static -I "%cd%\..\openssl\Release\include" -no-opengl -openssl-linked OPENSSL_LIBS_DEBUG="%cd%\..\openssl\Debug\lib\ssleay32.lib %cd%\..\openssl\Debug\lib\libeay32.lib" OPENSSL_LIBS_RELEASE="%cd%\..\openssl\Release\lib\ssleay32.lib %cd%\..\openssl\Release\lib\libeay32.lib" -mp -nomake examples -nomake tests -platform win32-msvc2015

jom -j4
jom -j4 install
cd ..

cd ../bettergram/Telegram
call gyp\refresh.bat