# FalconBoard

Last modified: Aug 8. 2020

FalconBoard (previous name MyWhiteboard) is a cross platform open source 
whiteboard / blackboard application with unique features that makes it 
perfectly suited for classroom or for online lectures. It has a semi 
infinite canvas, unlimited drawings, unlimited undo and redo, colored 
pens of different width, screen capture PDF output and more (see below).

The project also contains a separate viewer that only displays and prints
saved drawings, but cannot modify them.

*Features* (Version 1.0.0) : 

   -  Works under Windows and Linux, and (I hope) on Macs too.
   -  Usable with the mouse and with a Wacom graphics tablet. 
        (No Android or iOS versions yet.)
   -  Freehand drawing with five different color (black/white, red, green, 
        blue/lighterblue, yellow/agenta) pens or draw exactly horizontal and
        vertical lines and rectangles.
   -  Pen and eraser pen widths are adjustable separately between - and - - pixels.
   -  Unlimited Undo/Redo during a single session, undo visible drawings
      even in a new session.
   -  System, dark and black view modes
   -  "Unlimited" scrolling of page by keyboard (PgUp,PgDown,Home,Left,Right
       alone or together with Ctrl for faster movement), the mouse or a
       Wacin graphics tablet (press and hold space while using them or used
       the wheel on the mouse).
   -  Mark a rectangular area with the right button on mouse or tablet, then 
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
          documents during the same session (screenshots excluded yet).
   -  Keyboard shortcuts to most functions (See Help).
   -  Optional saving and loading sessions, option to autosave data and 
      background image on exit.
   -  Print the hole document or a range of it. (Because of a bug in Qt it is
      not possible to set the current page or a list of pages.)
   -  Export the whole document into a PDF file.
   -  You may set a background image: all you draw are on a different layer above
        the background.
   -  Take a screenshot with F- from any rectangular area of the desktop and
      load it in the middle of the screen to show or connotate. (You may move it
      around like any drawings by marking it first.)
  -   Draw straight horizontal or vertical lines (press and hold the Shift key 
        before you start drawing).
  -   Visible area can be saved in many different image formats.

  Completely open source (GPL - . 
        (I used some icons from PikaJian's Whitepad project 
            https://github.com/PikaJian, but no code was used from that project.)
  Written in C++ with Qt - .- (multi platform).
  Both Visual studio project files and QtDesigner project files are included.
  
*Compilation*:

  Compile it with Qt Version 15.5.0 or higher or modify the .ui files 
  ( in older Qt versions some fields may be missing from the components 
    e.g. QComboBox.setPlaceHolderText() ).
  - On Microsoft Windows you can  compile it with Visual Studio 2017/2019
    using the extension Qt VS Tools
    A project file *FalconBoard.sln* file is included for VS 2019.
  - On Linux use the .pro files provided. 

*Releases*
  You find two compiled Windows -  version ZIP files in the 'releases'. One with 
  all the Microsoft and Qt redistributables and one with only the EXE files.
  Complete version: 'FalconBoard_vX.Y.Z_Release.ZIP,
  executable only version
  You only need to download the ZIP file and unpack it into any empty folder
  and you are ready to go.
  
  If you have downloaded the ZIP and there were no 2nd digit version changes 
  (like from 1.1.0 x to 1.2.0) you only need the executables only. 
  Unpack it in the same folder where the previous release are.