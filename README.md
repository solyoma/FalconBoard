# MyWhiteboard

Last modified: May 5. 2020.

MyWhiteboard is a cross platform open source whiteboard / blackboard application 
with unique features that makes it perfectly suited for classroom or for online 
lectures. It has a semi infinite canvas, unlimited drawings, unlimited undo and 
redo, colored pens of different width, screen capture and more (see below).

The project also contains a separate viewer that only displays saved 
drawings, but cannot modify them.

*Features* (Version 1.1.3): 

   0. Works under Windows and Linux, and (I hope) on Macs too.
   1. System, dark and black view modes
   2. Usable with the mouse and with a graphics tablet. (No Android or iOS 
        versions yet.)
   3. Freehand drawing with five different (black/white, red, green, 
        blue/lighterblue, yellow/agenta) colored pens.
   4. Pen and eraser pen width is adjustable separately between 1 and 300 pixels.
   5. Unlimited Undo/Redo.
   6. "Unlimited" scrolling of page by keyboard (PgUp,PgDown,Home,Left,Right
       alone or together with Ctrl for faster movement)
      mouse or graphics tablet (press and hold space while using them or used
      the wheel on the mouse).
   7. Mark an area with the right button on mouse or tablet, then 
        - click inside the area to move the marked items around on the screen,
          release the button to paste it to that position.
        - draw a rectangle around by pressing the R key
        - insert vertical space by pressing F5. Magnitude is the height of the
          selection
        - delete drawings completely inside selection
          this area with the Del or BackSpace keys.
        - if the selected area is empty delete empty space from the right 
          or from below. 
        - copy / cut marked area and paste it at any position, even into new
          documents during the same session (screenshots excluded yet).
        - drawings completely under marked area can be re-colored.
   8. Keyboard shortcuts to most functions (See Help).
   9. Optional saving and loading sessions, option to autosave data and 
      background image on exit.
  10. Loading a background image: all you draw are on a different layer above 
      the background to have illustrations to comment on.
  11. Horizontal and vertical line drawing (press and hold the Shift key).
  12. Visible area can be printed and saved in many different image formats.
  13. Take a screenshot with F4 from any rectangular area of the desktop and
      load it in the middle of the screen to show or connotate.
  14. Completely open source (GPL 3). 
      (I used some icons from PikaJian's Whitepad project 
       https://github.com/PikaJian, but no code was used from that project.)
  15. Written in C++ with Qt 14.1 (multi platform).
  16. Both Visual studio project files and QtDesigner project files are included.
  
*Compilation*:

  It should compile with older (>5.6) Qt versions too, but I did not test it.
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
  
  If you have downloaded the ZIP already you need the release with the 
  executables only. Undo/Redonpack it in the same folder where the previous release are.