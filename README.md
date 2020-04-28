# MyWhiteboard

Last modified: April 25. 2020.

MyWhiteboard is a cross platform open source Whiteboard application with unique
features usable in classrooms or for online lectures. It has a semi infinite
canvas unlimited drawing, unlimited undo and redo, colored pens of different 
width screen capture and more (see below).

*Features*: 

   0. Works under Windows and Linux, and (I hope) on Macs too.
   1. System, dark and black view modes
   2. Freehand drawing with four different (black/white, red, green, blue) 
      colored pens.
   3. Pen and eraser pen width is adjustable between 1 and 300 pixels.
   4. Unlimited Undo/Redo.
   5. "Unlimited" scrolling of page by keyboard (PgUp,PgDown,Home,Left,Right)
      mouse or graphics tablet (press and hold space while using them or used
      the wheel on the mouse).
   6. Mark area by right button on mouse or tablet, and 
            - draw a rectangle by pressing the R key
            - delete drawings completely inside
                this area with the Del or BackSpace keys.
            - copy / cut marked area and paste it at any position, even into new
                documents during the same session
            - drawings completely under marked are can be re-colored
   7. Usable with the mouse and with a graphic tablet.
   8. Keyboard shortcuts to most functions.
   8. Optional saving and loading sessions, autosaving data and background.
  10. Loading a background image: all drawings are on a different layer above 
      the background to have illustrations to comment on.
  11. Horizontal and vertical line drawing (with the Shift key)
  12. Visible area can be printed and saved in many different image formats.
  13. Take a screenshot from any rectangular area of the desktop and load it 
      as background image.
  14. Completely open source (GPL 3). 
      (I used icons from PikaJian's Whitepad project https://github.com/PikaJian,
       but no code was used from that project.)
  15. Written in C++ with Qt 14.1 (multi platform).
  16. Both Visual studio project files and QtDesigner project files are included.
  
*Compilation*:

  It should compile with older (>5.6) Qt versions too, but I did not test it.
  - On Microsoft Windows you can  compile it with Visual Studio 2019/2017
    A project file *MyWhiteboard.sln* file is included for VS2019
  - On Linux I used QtCreator.
