This project aims to build tools for Interlisp source code analysis (with cross-referencing and semantic navigation as far as possible), to support the development of a new VM compatible with old Interlisp-10 and Interlisp-D versions, and to bootstrap systems from source code.

I'm particularly interested in the Interlisp-D versions which were available when Smalltalk-80 was published. Fortunately, the Computer History Museum has published a [Xerox PARC file system archive](https://xeroxparcarchive.computerhistory.org/), which - among other remarkable finds - includes the images and sources of many Interlisp-D versions.

The [original Smalltalk-80 sources](http://www.wolczko.com/st80/image.tar.gz) are dated April 1983, and if we run the included image with [a compatible virtual machine](https://github.com/rochus-keller/Smalltalk), the version message in the terminal says May 31. 1983. 

The closest Interlisp version is "Chorus", which was released in Feb. 1983; the Xerox PARC archive includes [a Chorus image (called Lisp.sysout)](https://xeroxparcarchive.computerhistory.org/eris/lisp/chorus/basics/.index.html), but unfortunately no source code. The first version which is [available with source code is Fugue.2](https://xeroxparcarchive.computerhistory.org/eris/lisp/fugue.2/sources/.index.html); it was released around Aug. 1983; though it is unclear whether this source code version was indeed the released one, since there are syntax errors in [ADISPLAY!1](https://xeroxparcarchive.computerhistory.org/eris/lisp/fugue.2/sources/.ADISPLAY!1.html). But for now I will stick with this release.

I have implemented an Interlisp Navigator (see screenshot), which I will extend as I study the sources.

![Navigator Screenshot 1](http://software.rochus-keller.ch/interlisp-navigator-screenshot-0.2.0-1.png)


The next [Interlisp version with source code is Fugue.5](https://xeroxparcarchive.computerhistory.org/eris/lisp/fugue.5/.index.html), released around March 1984; the source code was completely refactored compared to Fugue.2, and there are no syntax errors. I found some [release notes](https://xeroxparcarchive.computerhistory.org/erinyes/lisp/fugue.6/doc/.CAROLRELEASE.TED!1.html) which state that if the release "survives the next few weeks of your use, it will be released outside Xerox as Carol". Carol was then indeed released around June 1984, and we have [the source code and a lot of other files](https://xeroxparcarchive.computerhistory.org/eris/lisp/carol/.index.html). The present version of the Navigator is compatible with both Fugue and Carol (I also briefly tried Koto which seems to work as well).

NOTE that this project is work in progress.

### Precompiled versions

The following precompiled versions are available at this time:

- [Linux x86](http://software.rochus-keller.ch/InterlispTools_linux_i386.tar.gz)
- [Linux x64](http://software.rochus-keller.ch/InterlispTools_linux_x64.tar.gz)
- [Windows x86](http://software.rochus-keller.ch/InterlispTools_win32.zip)

Just download and unpack the compressed file to a directory. Start the application by double clicking on the InterlispNavigator executable. 

Here is a copy of the [Fugue.2 sources](http://software.rochus-keller.ch/interlisp-fugue-2-sources.zip) for convenience.

### Build Steps

This project can be built using qmake and Qt5. Use the .pro files to run the build as described in the Qt documentation. 

Alternatively the navigator can be built using LeanQt and the BUSY build system (with no other dependencies than a C++98 compiler); follow these steps:

1. Create a new directory; we call it the root directory here
1. Download https://github.com/rochus-keller/Interlisp/archive/refs/heads/master.zip and unpack it to the root directory; rename the resulting directory to "Interlisp".
1. Download https://github.com/rochus-keller/GuiTools/archive/refs/heads/master.zip and unpack it to the root directory; rename the resulting directory to "GuiTools".
1. Download https://github.com/rochus-keller/LeanQt/archive/refs/heads/master.zip and unpack it to the root directory; rename the resulting directory to "LeanQt".
1. Download https://github.com/rochus-keller/BUSY/archive/refs/heads/master.zip and unpack it to the root directory; rename the resulting directory to "build".
1. Open a command line in the build directory and type `cc *.c -O2 -lm -o lua` or `cl /O2 /MD /Fe:lua.exe *.c` depending on whether you are on a Unix or Windows machine; wait a few seconds until the Lua executable is built.
1. Now type `./lua build.lua ../Interlisp` (or `lua build.lua ../Interlisp` on Windows); wait until the InterlispNavigator executable is built; you find it in the output subdirectory.

## Support

If you need support or would like to post issues or feature requests please use the Github issue list at https://github.com/rochus-keller/Interlisp/issues or send an email to the author.
