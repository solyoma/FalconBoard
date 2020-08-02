# MyWhiteboard

Last modified: May 5. 2020.

MyWhiteboard is a cross platform open source whiteboard / blackboard application 
with unique features that makes it perfectly suited for classroom or for online 
lectures. It has a semi infinite canvas, unlimited drawings, unlimited undo and 
redo, colored pens of different width, screen capture and more (see below).

The project also contains a separate viewer that only displays saved 
drawings, but cannot modify them.

*Features* (Version 1.2.0): 

   0. Works under Windows and Linux, and (I hope) on Macs too.
   1. System, dark and black view modes
   2. Usable with the mouse and with a graphics tablet. (No Android or iOS 
        versions yet.)
   3. Freehand drawing with five different colored (black/white, red, green, 
        blue/lighterblue, yellow/agenta) pens or draw exactly horizontal and
        vertical lines and rectangles. (Not ellipses yet.)
   4. Print the hole document or a range of it. (Because of a bug in Qt it is
      not possible to set the current page or a list of pages.)
   5. Pen and eraser pen widths are adjustable separately between 1 and 300 pixels.
      Unlimited Undo/Redo during a single session, undo visible drawings
   6. even in a new session.
      "Unlimited" scrolling of page by keyboard (PgUp,PgDown,Home,Left,Right
       alone or together with Ctrl for faster movement)
      mouse or graphics tablet (press and hold space while using them or used
   7. the wheel on the mouse).
      Mark an area with the right button on mouse or tablet, then 
        - insert vertical space by pressing F5. Magnitude is the height of the
          selection
        - draw a rectangle around it by pressing the R key
        - if there were displayed items inside the area:
            - click inside the area to move the marked items around on the screen,
              release the button to paste it to that position.
            - delete drawings completely inside selection
              this area with the Del or BackSpace keys.
            - drawings completely under marked area can be re-colored.
        - if the selected area is empty:
              delete empty space from the right 
              or from below. 
        - copy / cut marked area and paste it at any position, even into new
   8.     documents during the same session (screenshots excluded yet).
   9. Keyboard shortcuts to most functions (See Help).
      Optional saving and loading sessions, option to autosave data and 
  10. background image on exit.
      Loading a background image: all you draw are on a different layer above 
  11. the background to have illustrations to comment on.
  12. Horizontal and vertical line drawing (press and hold the Shift key).
  13. Visible area can be saved in many different image formats.
      Take a screenshot with F4 from any rectangular area of the desktop and
  14. load it in the middle of the screen to show or connotate.
      Completely open source (GPL 3). 
      (I used some icons from PikaJian's Whitepad project 
  15.  https://github.com/PikaJian, but no code was used from that project.)
  16. Written in C++ with Qt 14.1 (multi platform).
  17. Both Visual studio project files and QtDesigner project files are included.
  
*Compilation*:

  Compile it with Qt Version 5.15.0 or higher or modify the .ui files 
  as with older Qt versions some fields may be missing from the components 
  (e.g. QComboBox.setPlaceHolderText() ).
  - On Microsoft Windows you can  compile it with Visual Studio 2019/2017
    A project file *MyWhiteboard.sln* file is included for VS2019
  - On Linux use the .pro files provided. 

*Releases*
  You find two compiled Windows 10 version ZIP files in the 'releases'. One with 
  all the Microsoft and Qt redistributables and one with only the EXE files.
  Complete version: 'MyWhiteboard_vX.Y.Z_Release.ZIP,
  executable only version
  You only need to download the ZIP file and unpack it into any empty folder
  and you are ready to go.
  
  If you have downloaded the ZIP and there were no 2nd digit version changes 
  (like from 1.1.x to 1.2.x you already you need the release with the 
  executables only. Unpack it in the same folder where the previous release are.