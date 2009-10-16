
    Dokan Ruby Binding

 Copyright(c) Hiroki Asakawa http://decas-dev.net



What is Dokan Ruby Binding
==========================

This program is a Ruby binding of Dokan that is a user mode file
system for Windows. Using this program, you can make a file system
in Ruby language.


Licensing
=========

Dokan Ruby Binding is distributed under a version of the "MIT License",
which is a BSD-like license. See the 'license.txt' file for details.


Environment
===========

'dokan_lib.so' is built using Visual C++ 6.0.
Dokan Ruby Binding is checked on mswin32 Ruby and One-Click Ruby.


How to use
==========

Place 'dokan_lib.so' in $LOAD_PATH directory and run a ruby script.
You can check $LOAD_PATH using "ruby -e 'puts $LOAD_PATH'" command.

  > ruby sample\test.rb

Administrator privileges are required to run file system applications.

You can unmount following command.

  > dokanctl.exe /u DriveLetter

'dokanctl.exe' is included in Dokan library.

You must see 'readme.txt' included in Dokan library.


How to make a file system
=========================

To make a file system, you need to implement methods described in 'API.txt'
file in 'class Hello' and  call 'Dokan.mount("d", Hello.new)'.
Each method should return 'true' when the operations scuceeded, otherwise
it should return 'false'.

    open(path, fileinfo)

The first argument is a path of opening file, the second is context information.
FileInfo#directory?, FileInfo#process_id, FileInfo#context and FileInfo#context=
methods are defined.

FILE_ATTRIBUTE_* used with WindowsAPI are defined (Dokan::NORMAL,
DOKAN::Directory ...). These constans are used in 'stat' method.
You can get debug information by setting 'Dokan.debug = true'.

There are sample scripts under 'sample' folder.
'lib\dokanfs.rb' is compatibility interface of FuseFS (Ruby binding of FUSE).
See 'sample\hello.rb' for details.

