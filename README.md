# xflr5
 an update to xflr5 to enable 3d printable export



This is a fork of xflr5_v6 to test options for directly producing 3d printable wings

It would be very safe to assume the original Sourceforge version will be more up-to-date in every other aspect.
The original binaries and sourcecode can be accessed from https://sourceforge.net/projects/xflr5/


NB This is a work in progress - it is currently at the status of "barely functional"



You should be able to run the current windows version by copying xflr5_3dprinting.exe from the root of this repository into the original xflr5 installation directory.
You can then open your project and select "Export to STL" in the R click menu from the wing name in the Object Explorer
The stl output can be opened in your favorite printing or editing software (Prusa, Blender, etc)

[Export to STL](doc/STLExport.png)
[Dialog Box](doc/STLExport.png)
[STL Output](doc/STLoutput.png)



Alternatively you can compile it from the github source in the same way you can with the original xflr5 as follows:-


Install QT Creator - either the community/opensource or the professional version.
    Because this is opensource software you are fine to download the opensource version of QT (you will have to register either way - but the opensource one version is free) 
    https://www.qt.io/download

You MUST install the QT 5.12 kit during the installation process - xflr5 currently has dependencies which don't work for 6.x

Once QT is installed, open xflr5.pro using QT Creator
If all has gone well you should be able to run it directly with ctrl-R
[the "release" executable - selected in the bottom LH corner of QT Creator) is the version you will need to copy if you want to put it in the original xflr5 directory.]

If you're considering editing the code the only real files of significance are:-

..\xflr5\xflr5v6\xflobjects\objects3d
wing.h
wing.cpp   (this has the heart of the code updates - exportSTL3dPrintable is the primary entry point for this code)

..\xflr5\xflr5v6\misc
stlexportdialog.h
stlexportdialog.cpp     (this is where the modified dialog box is coded)

..\xflr5\xflr5v6\miarex
miarex.cpp
    void Miarex::onExporttoSTL()    // takes the output from the STL export dialog box and passes it to the Wing::exportSTL3dPrintable function