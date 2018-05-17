echo off
echo compiling test driver...
del pelconf.exe 2>__autok2

if arg%1 == argCC goto :getcc2
if arg%1 == argcc goto :getcc2
if arg%1 == arg--CC goto :getcc2
if arg%1 == arg--cc goto :getcc2

if arg%2 == argCC goto :getcc3
if arg%2 == argcc goto :getcc3
if arg%2 == arg--CC goto :getcc3
if arg%2 == arg--cc goto :getcc3

if arg%3 == argCC goto :getcc4
if arg%3 == argcc goto :getcc4
if arg%3 == arg--CC goto :getcc4
if arg%3 == arg--cc goto :getcc4

if arg%4 == argCC goto :getcc5
if arg%4 == argcc goto :getcc5
if arg%4 == arg--CC goto :getcc5
if arg%4 == arg--cc goto :getcc5

if arg%5 == argCC goto :getcc6
if arg%5 == argcc goto :getcc6
if arg%5 == arg--CC goto :getcc6
if arg%5 == arg--cc goto :getcc6

if arg%6 == argCC goto :getcc7
if arg%6 == argcc goto :getcc7
if arg%6 == arg--CC goto :getcc7
if arg%6 == arg--cc goto :getcc7

if arg%7 == argCC goto :getcc8
if arg%7 == argcc goto :getcc8
if arg%7 == arg--CC goto :getcc8
if arg%7 == arg--cc goto :getcc8

if arg%8 == argCC goto :getcc9
if arg%8 == argcc goto :getcc9
if arg%8 == arg--CC goto :getcc9
if arg%8 == arg--cc goto :getcc9


goto :autodetect

:getcc2
set cc=%2
goto :checkcc

:getcc3
set cc=%3
goto :checkcc

:getcc4
set cc=%4
goto :checkcc

:getcc5
set cc=%5
goto :checkcc

:getcc6
set cc=%6
goto :checkcc

:getcc7
set cc=%7
goto :checkcc

:getcc8
set cc=%8
goto :checkcc

:getcc9
set cc=%9
goto :checkcc


if %cc%prog == prog goto :autodetect

:checkcc
echo compiling driver with %cc%

%cc% pelconf.c >__autok1 2>__autok2
if exist pelconf.exe goto dotests

%cc% -o pelconf.exe pelconf.c >__autok1 2>__autok2
if exist pelconf.exe goto dotests

%cc% -eautoconf.exe pelconf.c >__autok1 2>__autok2
if exist pelconf.exe goto dotests


:failure
echo Can't compile the file pelconf.c
echo Please compile pelconf.c and run it as if it were configure.
goto cleanup




:autodetect
echo pelconf.exe: pelconf.c>__autocf.mk
echo #>>__autocf.mk

if %make%prog == prog goto nomake

echo compiling driver with %make%
%make% -f__autocf.mk >__autok1 2>__autok2
goto checkexe

:nomake
echo compiling driver with make
make -f__autocf.mk >__autok1 2>__autok2

:checkexe
if not exist pelconf.exe goto failure


:dotests
.\pelconf.exe %*

:cleanup
del __autocf.mk 2>__autok2
del __autok1
del __autok2
del __autotst*
del __dummy*


:failure
echo Could not find your compiler, please use the --cc=<compiler> option
