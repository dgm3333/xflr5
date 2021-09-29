# xflr5
 an update to xflr5 to enable 3d printable export



This is a fork of xflr5_v6 to test options for directly producing 3d printable wings

It would be very safe to assume the original Sourceforge version will be more up-to-date in every other aspect.
The original binaries and sourcecode can be accessed from https://sourceforge.net/projects/xflr5/


NB This is a work in progress - it is currently at the status of "barely functional", so don't expect anything too amazing if you try it out.

Known issues:
[See also TODOs within wing.cpp]
It currently only processing a simple "single section" wing profile, and only the RHS - so you will have to mirror and print to get the Left wing.
Binary STL output doesn't work for printed parts (but does work for 3d viewable items) - don't know why, but as text output works fine it's a fairly low priority)
Some of the settings are hardcoded - so you can only access them if you compile your own code.
Spars:
   Will currently (probably) fail to form manifold surfaces if one is above another
   Spar settings are hardcoded as I haven't moved it into the dialog box yet. 
   If a spar outer surface is routed within 0.5radius of a skin surface (either outer or inner) then that part will probably be non-manifold
If an option seems robust and worthwhile keeping I'll move it into the dialog box and/or enable it to make it accessible.
None of the settings are saved - since I have no intention of touching the xflr5 savefiles this isn't likely to change.
Although it's basically just inverting the skin (so fairly basic extension), it isn't creating molds as yet - I wanted to get the "pure" 3d printable wing working to a decent level first.


TODO:
[See also TODOs within wing.cpp]
adjust the rib thickness depending on the force that each panel experiences



You should be able to run the current windows version by copying xflr5_3dprinting.exe from the root of this repository into the original xflr5 installation directory.
You can then open your project and select "Export to STL" in the R click menu from the wing name in the Object Explorer
The stl output can be opened in your favorite printing or editing software (Prusa, Blender, etc)


<p align="middle"><p>
  <img src="https://github.com/dgm3333/xflr5/blob/main/doc/STLExport.png" title="Export to STL" />
  <img src="https://github.com/dgm3333/xflr5/blob/main/doc/STLdlg.png" title="Dialog Box" />
  <img src="https://github.com/dgm3333/xflr5/blob/main/doc/STLoutput.png" title="STL Output" />
  <img src="https://github.com/dgm3333/xflr5/blob/main/doc/STLoutput2.png" title="STL Output" />
    <img src="https://github.com/dgm3333/xflr5/blob/main/doc/STLoutput3.png" title="STL Output" />
</p>


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