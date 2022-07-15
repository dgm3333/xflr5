# xflr5
 an update to xflr5 to enable 3d printable export



This is a fork of xflr5_v6 to test options for directly producing 3d printable wings.
It is a work in progress and current status is "generally works", so don't expect anything too amazing if you try it out.

It would be very safe to assume the original Sourceforge version will be more up-to-date in every other aspect.
(these binaries and sourcecode can be accessed from https://sourceforge.net/projects/xflr5/)


The windows executable (xflr5_3dprinting.exe) in the base directory of this repo works to create a set of basic "vases" (with a rib as the base and the surfaces).
You should be able to run it if you copy xflr5_3dprinting.exe from the root of this repository into the original xflr5 installation directory (nothing else is required) and no files need to be overwritten.
After you run this version you can open your project and select "Export to STL" in the R click menu from the wing name in the Object Explorer (see images below)
The stl output can be opened in your favorite printing or editing software (Prusa, Blender, etc)

Currently hardcoded only (ie you can only use if you open the source with Qt Creator and edit the code yourself):-
   Spar cutouts or printing (at approx line 4350 in xflr5\xflr5v6\xflobjects\objects3d\wings.cpp)
   Drainage holes for resin printers (just below spars in the code)


Known issues:
[See also TODOs within wing.cpp]
If you only want a single section you will currently have to delete unwanted parts in a 3d modelling program.
It currently only processes a simple "single section" wing profile, and only the RHS - so you will have to mirror and print to get the Left wing.
Binary STL output doesn't work for printed parts (but does work for 3d viewable items) - don't know why, but as text output works fine it's a fairly low priority to fix.
Some of the settings are hardcoded - so you can only access them if you compile your own code.
Spars:
   Will currently fail if one spar is vertically above another
   If a spar enters the wing skin then that part will be non-manifold (ie potentially unprintable)
   It may also fail if the spar is close to the edge of the wall if the triangle tries to stitch to a point "over the horizon" of the spar
      (you can generally fix this by increasing the number of chord panels)

If an option seems robust and worthwhile keeping I'll move it into the dialog box and/or enable it to make it accessible.
None of the settings are saved - since I have no intention of touching the xflr5 savefiles this isn't likely to change.
Although it's basically just inverting the skin (so fairly basic extension), it isn't creating molds as yet - I wanted to get the "pure" 3d printable wing working to a decent level first.

Slicer tests:
I've tested with smaller wings (25-50cm span). Using PLA prints OK with 2mm rib thickness and 0.5mm skin thickness. I seem to get best slicing and printing results using vase mode and no cooling fan, 0.4mm nozzle and 0.2mm layer height (although I haven't experimented a lot with different printer setup). 
Prusaslicer: Sometimes (but not always) distorts the outer surface of the rib (but not the skin), or the leading edge for the first 10mm or so - I assume the rib distortion is from some setting related to not overextruding close to the buildsurface, but not sure why it persists further up the model. It also doesn't slice well if multiple parts are attempted to be printed together (which is irritiating as this was my prime reason for the project)
Cura seems to slice OK, but for me it doesn't print properly (I haven't actually looked at why)


TODO:
[See also TODOs within wing.cpp]
adjust the rib/skin thickness depending on the force that each panel experiences



<p align="middle"><p>
  <img src="https://github.com/dgm3333/xflr5/blob/main/doc/STLExport.png" title="Export to STL" />
  <img src="https://github.com/dgm3333/xflr5/blob/main/doc/STLdlg.png" title="Dialog Box" />
  <img src="https://github.com/dgm3333/xflr5/blob/main/doc/STLoutput.png" title="STL Output" />
  <img src="https://github.com/dgm3333/xflr5/blob/main/doc/STLoutput2.png" title="STL Output" />
    <img src="https://github.com/dgm3333/xflr5/blob/main/doc/STLoutput3.png" title="STL Output" />
</p>


You can compile this from the github source in the same way you can with the original xflr5 as follows:-


Install QT Creator - either the community/opensource or the professional version.
    Because this is opensource software you are fine to download the opensource version of QT (you will have to register either way - but the opensource one version is free) 
    https://www.qt.io/download

You MUST install the QT 5.15.2 kit during the installation process - xflr5 currently has dependencies which don't work for 6.x

Once QT is installed, open xflr5.pro using QT Creator
If all has gone well you should be able to run it directly with ctrl-R
[the "release" executable - selected in the bottom LH corner of QT Creator) is the version you will need to copy if you want to put it in the original xflr5 directory.]

If you're considering editing the code the only real files of significance are:-

..\xflr5\xflr5v6\xflobjects\objects3d
wing.h
wing.cpp   // the heart of the code updates - exportSTL3dPrintable is the primary entry point for this test code

..\xflr5\xflr5v6\misc
stlexportdialog.h
stlexportdialog.cpp     // where the modified dialog box is coded

..\xflr5\xflr5v6\miarex
miarex.cpp
    void Miarex::onExporttoSTL()    // takes the output from the STL export dialog box and passes it to the Wing::exportSTL3dPrintable function