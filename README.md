# FalconBoard
![image](https://user-images.githubusercontent.com/37068759/114211459-f61df280-9960-11eb-8e57-eddca19dcf7e.png)

Last modified: April 8. 2021


FalconBoard (previous name MyWhiteboard) is a cross platform open source 
whiteboard / blackboard application with unique features that makes it 
perfectly suited for classroom or for online lectures. It has a semi 
infinite canvas, unlimited drawings, unlimited undo and redo, colored 
pens of different width, screen capture PDF output and more (see below).

The project also contains a separate viewer that only displays and prints
saved drawings, but cannot modify them.

## Screenshots:
![image](https://user-images.githubusercontent.com/37068759/108632072-54544c00-746d-11eb-9dfe-11a1120055f1.png)
## Features (Version 1.1) : 

   -  Works under Windows and Linux, and (I hope) on Macs too.
   -  Usable with the mouse and with a Wacom graphics tablet. 
        (No Android or iOS versions yet.)
   -  System, dark and black interface modes.
   -  Freehand drawing with five different colored pens. Colors depend on
      interface modes (light/dark): black/white, red, green, blue/lighter blue, 
      yellow/magenta.
   -  Draw straight lines which may be exactly horizontal or vertical.
   -  Draw rectangles, ellipses or circles
   -  Each Pen and the Eraser pen widths are adjustable separately.
   -  Unlimited Undo/Redo during a single session.
   -  "Unlimited" scrolling from top of "paper" by the keyboard 
      (Left, Right, Up, Down, PgUp,PgDown, Home, End, 
       alone or together with Ctrl for faster movement), with the mouse or
       pen on Wacom graphics tablet.
   -  Mark a rectangular area with the right button on mouse or tablet. 
        If the area contained any complete drawings or screenshots then 
        the selection will constrict around those.
   -  While a selection is active
        - insert vertical space by pressing F5. Height of the space is the 
            height of the selection
        - draw a rectangle around it by pressing the R key
        - if there were displayed items inside the area:
            - click inside the area to move the marked items around on the screen,
              release the button to paste it to that position.
            - delete drawings completely inside selection
              area with the Del or BackSpace keys.
            - lines completely under the marked area can be re-colored.
        - if the selected area is empty you can:
              delete empty space from the right  or from below with the Del key. 
        - copy / cut marked area and paste it at any position, even into new
          documents during the same session.
   -  Keyboard shortcuts to most functions (See key F1 - Help).
   -  Optional saving and loading sessions, option to automatic save of data
      and background image on exit.
   -  Print the whole document or a range of it. (Qt 15 dows not make
      just to print the current page or a list of pages possible.)
   -  Export the whole document into a PDF file.
   -  You may set a static background image: all you draw are on a different layer above
        the background.
   -  Take a screenshot with F4 from any rectangular area of the desktop and
      paste it into the middle of the screen to show or connotate. (You may move it
      around like any drawings by marking it first.)
  -   Draw straight horizontal or vertical lines :press the left mouse button or 
        press the pen to the tablet THEN press and hold the Shift key and start
        drawing).
  -   Draw a straight line from the last drawn position to the current one:
        (press and hold the Shift key THEN press the pen to the tablet)
  -   Visible screen area can be saved as an image in many different image formats.

  Completely open source (see Copyright)
        I used some icons [from PikaJian's Whitepad project](https://github.com/PikaJian)
        but no code was used from that project.
  Written in C++ with Qt (multi platform).
  Both Visual studio project files and QtDesigner project files are included.
  
*Compilation*:

  Compile it with Qt Version 5.15or modify the .ui files 
    (in older Qt versions some fields may be missing from the components 
    e.g. QComboBox.setPlaceHolderText() ).wasn't tested with Qt version 6.
  - On Microsoft Windows you can  compile it with Visual Studio 2017/2019
    using the extension [Qt VS Tools](https://marketplace.visualstudio.com/items?itemName=TheQtCompany.QtVisualStudioTools-19123)
    A project file *FalconBoard.sln* file is included for VS 2019.
  - On Linux use the .pro files provided. 

*Releases*
  For each release there are two compiled Windows-version ZIP files 
  in 'Releases'. One with all the Microsoft and Qt redistributables required
  and another one  with only the EXE files.
  Complete versions are named: 'FalconBoard_vX.Y.Z_Release.ZIP,
  executable only versions are named 'FalconBoard_vX.Y.Z_Executables_Only.Zip'
  You only need to download the ZIP file and unpack it into any empty folder
  and you are ready to go.
  
  If you have downloaded the ZIP and there were only sub-version changes 
  (like from 1.1.x to 1.1.y) you only need the executables. 
  Unpack it in the same folder where the previous release are.
