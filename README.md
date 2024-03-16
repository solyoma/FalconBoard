# ![image](https://user-images.githubusercontent.com/37068759/114211459-f61df280-9960-11eb-8e57-eddca19dcf7e.png) 
Last modified/utolsó változtatás: Mar. 16. 2024
(A magyar nyelvű leírást az angol után találod)

# FalconBoard
is a cross platform open source whiteboard / blackboard application with 
unique features that makes it perfectly suited for classroom or for online 
lectures. It has a semi-infinite canvas, unlimited drawings, unlimited undo 
and redo, colored pens of different width, screen capture, printout, PDF 
output and more (see below).

*The project also contains a separate viewer that only displays and prints
saved drawings, but cannot modify them or create PDFs.**

## Screenshots:
![FalconBoard - dark mode](https://github.com/solyoma/FalconBoard/assets/37068759/151e3386-1ac8-496b-8951-05c45c14ae08)
![FalconBoard - light mode](https://github.com/solyoma/FalconBoard/assets/37068759/9a4c7e8e-0f54-47ef-a6e1-d0562f34babb)
![FalconBoard - light mode with no grid shown](https://github.com/solyoma/FalconBoard/assets/37068759/00180553-6d94-454f-b714-362582734162)
## Features (Version 2.x) : 

   -  Works under Windows and Linux, and (I hope) on Macs too.
      (did not even try to compile it on Android or iOS)
   -  Two interface languages: US English and Hungarian, selectable  
      from inside the program.
   -  Can be used with the mouse and with pen input (a Wacom graphics tablet. 
   -  System, dark and black interface modes.
   -  Maximum 30 documents can be open at the same time
   -  Freehand drawing with seven different colored pens. Colors depend on
      interface modes (system/light/white/dark): black/white, red, green, blue/lighter blue, 
      yellow/magenta, brown, magenta and the last five be freely re-defined for each document.
   -  Widths of each pen and the Eraser are adjustable separately.
   -  Draw straight lines which may also be exactly horizontal or vertical.
   -  Draw empty or opaque rectangles, ellipses or circles
   -  Unlimited Undo/Redo during a single session.
   -  "Unlimited" scrolling from top of "paper" by the keyboard 
      (Left, Right, Up, Down, PgUp,PgDown, Home, End, 
       alone or together with Ctrl for faster movement), with the mouse or
       pen on Wacom graphics tablet.
   -  Keyboard shortcuts to most functions (See key F1 - Help).
   -  Export the whole document into a PDF file. 
   -  Mark a rectangular area with the right button on mouse or tablet. 
        If the Shift key is kept down during selection the selection will
        be a square.
        If the area contains any complete drawings or screenshots then 
        the selection will constrict around those.
	 -  Press the Alt key before letting the mouse button up to make the 
		    selection not to constrict to any elements inside it.
	 - If The Alt key is kept down during selection the selection will
		    expand from the original start position and will not constrict
		    when the button is released. This way you can draw concentric
		    circles or rectangles.
   -  While a selection is active
        - insert vertical space by pressing F5. Height of the space is the 
            height of the selection
        - draw a rectangle around it by pressing the R key. For a contracted
          selection there is a small padding inside this rectangle
        - draw an ellipse or a circle inside it by pressing C
		- mark the center of the selection by a dot or an x by pressing the 
		  corresponding key
        - if there were displayed items inside the area:
            - click inside the area and hold to move the marked items around,
              on the screen release the button to paste it to that position.
            - delete drawings completely inside selection area with the Del 
			        or BackSpace keys.
            - lines completely under the marked area can be re-colored by
			        clicking on the pen color buttons or pressing 1..5.
            - line widths of drawings inside the selection can be changed
              using the right (]) and left (\[ ) bracket keys
        - if the selected area is empty you can:
              delete empty space from the right or from below with the Del key. 
        - copy / cut marked area and paste it into any open drawings at at any 
          position, even into new documents or new instances
        - paste images from the clipboard (from v2.3.6)
        - press F7 to rotate the selected area right or left with any angle. Use
          Shift+F7 to repeat the last rotation.
        - Rotate 90 degrees left by pressing '9' or right by pressing '0',
          180 degrees by '8', mirror vertically or horizontally by 'V' and 'H'.
   -  Optional save and load of sessions, option to automatic save of data
      and background image on exit.
   -  Option to restrict many or one instance of the program to run
   -  Print the whole document or a range of it. (Qt 15 does not make it possible
      to print just the current page or a list of pages. Workaround: export as pdf
      and print that.)
   - Even documents created in a dark mode can be printed as light mode ones to
      save paint.
   -  You may set a static background image: all you draw are on a different layer above
        the background.
   -  Use screenshots: 
    - Take a screenshot with F4 from any rectangular area of the desktop and
      paste it into the middle of the screen to show or connotate. (After paste
      you may move it around like any drawings.)  
    - You may select a color to make portion of the screenshot transparent.
    - Ctrl+Click on a screenshot to mark it.
  -   Draw straight horizontal or vertical lines :press the left mouse button or 
        press the pen to the tablet THEN press and hold the Shift key and start
        drawing).
  -   Draw a straight line from the last drawn position to the current one:
        (press and hold the Shift key THEN press the pen to the tablet)
  -   Visible screen area can be saved as an image in many different image formats.

  Completely open source (see Copyright)
        I used some icons [from PikaJian's Whitepad project](https://github.com/PikaJian)
        but no code was used from that project.
		    The QuadTree code is taken from [PVigier](https://github.com/pvigier/Quadtree)
        Written in C++ with Qt (multi platform).
  Both Visual Studio project files and a QtDesigner project files are included.
  
*Compilation*:

  Compile it with Qt Version 5.15 or modify the .ui files. 
    (Wasn't tested with Qt version 6.x)
  - On Microsoft Windows you can  compile it with Visual Studio 2019/22
    using the extension [Qt VS Tools](https://marketplace.visualstudio.com/items?itemName=TheQtCompany.QtVisualStudioTools-19123)
    A project file *FalconBoard.sln* file is included for VS 2022 and 2019.
  - On Linux use the .pro files provided. 

*Installation*:
  No installation necessary. Copy the executable (and if you don't yet have them, the Qt 
	and Microsoft redistributables) into any folder. State of the programs are saved into *.ini files. On Windows these are inside the
    user's *AppData\Local\FalconBoard* folder, on linux under the hidden *.falconBoard* folder inside the user's home directory.

*Uninstalling*:    
    Delete the folder containing the program and optionally delete the configuration directories described above.

*Releases*
  For each release there are two compiled Windows-version ZIP files 
  in 'Releases'. One with all the Microsoft and Qt redistributables required
  and another one  with only the EXE files.
  Complete versions are named: 'FalconBoard_vX.Y.Z_Release.ZIP,
  executable only versions are named 'FalconBoard_vX.Y.Z_Executables_Only.Zip'
  You only need to download the ZIP file and unpack it into any empty folder
  and you are ready to go.
  
  If you have downloaded the ZIP and there were only sub-version changes 
  (like from 2.0.x to 2.3.y) you only need the new executables. 
  Unpack them into the same folder where the previous release resides.

## --------------------------------------

  *FalconBoard* egy platformfüggetlen, nyílt forráskódú "rajztábla" alkalmazás, amit a
  COVID alatti távoktatásrfa készítettem, olyan speciális tulajdonságokkal, amik
  osztályteremben kivetítésre, vagy távoktatásra nagyon hasznosak. Egy vizszintesen és 
  függőlegesen egyaránt végtelen "vászonra" írhatunk és rajzolhatunk különböző vastagságú
  és színű "tollakkal", képeket is vághatunk bele bármely más alkalmazásból, 
  a dokumentumot kinyomtathatjuk és PDF-be is elmenthetjük.

  Ugyanez a projekt tartalmaz egy nézegetőt (FalconBoardViewer), amivel az elmentett
  (nem PDF) fájlokat megnézhetjük, de nem módosíthatjuk.
  
## Képernyőfotók:
  ld. fent az angol részben

## Tulajdonságok (2.x verziók):

  - Működik Windows-on és linuxon és (remélem MAC-eken is)
 - Jelenleg kétnyelvű: magyar és angol, ami a programból választható
 - világos és sötét mód választhatóa programból
 - Egérrel és Wacom kompatibilis grafikus tabletek tollával is használható
 - Maximum 30 dokumentum szerkeszthető párhuzamosan
 - Szabadkézi rajzolás hét különböző, programban átdefiniálható, akár minden
   dokumentumban más színű tollal, amelyek színe a világos/sötét módokban
   különböző lehet. Az alapértelmezett színek (rendszer/világos/fehér/sötét/fekete -módok): 
   fekete/fehér, piros, zöld, kék/világoskék, bíbor/sárga, barna, bíborvörös.
 - a tollak és a Radir vastagsága választható
 - húzhatsz egyenes vonalakat, amelyek lehetnek pontosan vízszintesek vagy 
   fügőlegesek is.
 - rajzolhatsz üres, vagy kitöltött téglalapok/négyzetek és ellipszisek/köröket
 - "végtelen sok" visszavonás és újra alkalmazás lehetséges minden megnyitott
   dokumentumban
 - "végtelen" görgetés a "vászon/papír" bal felső sarkához képest mindkét
   irányban a billentyűzettel, illete az egérrel/tollal
 - a legtöbb funkció billentyűzetkombinációkkal könnyen elérhető (F1- súgó)
 - az egész dokumentum PDF fájlba exportálható
 - Kijelölhetsz egy téglalap alakú tartományt a jobb gombbal. 
     - Ha Shift-et is nyomva tartod négyzet alakú lesz a kijelölés.
     - Ha a kijelölt tartományban vannak teljes vonalak, illetve alakzatok és
       képek a kijelölés összehúzodik köréjük és ezzel kiválastja azokat.
       Ha ezt nem akarjuk, akkor nyomjuk be az Alt-ot mielőtt felengedjük az 
       egér/toll gombját.
     - A kijelölt tartományba téglalapot, illetve ellipszist rajzolhatsz az
       'R', ill. 'C' gombokkal
     - A kijelölt tartományba eső alakzatokat a DEL gombbal kitörölheted.
       Üres (vagy Alt-al kijelölt) terület esetén az F5 gomb egy üres 
       területet hoz létre, a kijelőlés tetejétől a kijelölés magsságával. 
       (Azaz minden, a kijelölés teteje alatti vonalat és képet eltol
       lefele.)
     - A kijelölt vonalak vastagságát a '[' és ']' gombokkal változtathatod
     - A kijelölés középpontját megjelölheted a '.', ill.'X' billentyűkkel
     - A kijelölt tartományba eső objektumokat mozgathatod az egérrel, ill. a tollal,
       vágólapra másolhatod, ami után bármelyik dokumentumba beillesztheted. 
       A beillesztés helyét a pillanatnyi egér/toll pozíció adja meg.
     - Nem csak a FalconBoardból kimásolt elemeket, de a vágólapon levő
       képeket is beillesztheted a dokumentumodba. (V2.3.6-tól)
     - Az F7 gombbal tetszőleges szögben elforgathatod a kijelölést. 
       A Shift+F7-el pedig az utolsó forgatás szögével. Az '1','0' gombok 90 
       fokkal, a '8' gomb 180 fokkal forgat a 'V' és 'H' gombok függőlegesen
       illetve vízszintesen tükrözik a kijelölt területet.
 - Képernyő részlet kivágás ('képernyőfotó') az F4 gombbal éa bal egérgombos
   kijelöléssel.
 - a látható terület képként is elmenthető
    
 Nyílt forráskódú (ld. Copyright)
        felhasználtam néhány ikont [PikaJian's Whitepad projectjéből](https://github.com/PikaJian)
        de semmilyen kódot nem
        A 'Quadtree' kód viszont [PVigier](https://github.com/pvigier/Quadtree)
        A program nyelve C++
    Visual Studio és Qt projekt fájlokat adok
* A program lefordítása
 
    Qt 5.15.2 -vel kell fordítani (Qt 6.x-el nem próbáltam.)
  - Microsoft Windows 10 és 11 alatt a Microsoft Visual Studio 2019/2022-tel fordítható, amihez
    a [Qt VS Tools](https://marketplace.visualstudio.com/items?itemName=TheQtCompany.QtVisualStudioTools-19123)
    kiterjesztést használtam
  - Linuxon a PRO fájlokat kell használni
  
* Telepítés
    
    A programot nem kell telepíteni. A parancsfájlokat (FalconBoar.exe, FalconViewer.exe) és az ikon fájlt
    (FalconBoard.ico), illetve, amennyiben ezek nincsenk fent a rendszerben a Qt (és Windows alatt a Microsoft
     redistributeable csomagot) be kell másolni egy tetszőleges mappába és egy parancsikont kell létrehozni
     az Asztalon. A program állapotát a felhasználó saját könyvtárában (Windows: a felhasználó
     *AppData\Local\FalconBoard* mappája, Linux a felhasználó könyvtárában levő rejtett *.falconBoard* mappába)
     elhelyezett .ini fájlban van elmentve.)
 
* Eltávolítás
 
    Töröld le a program mappát és ha későbbre nem kell, az itt megadott saját mappákat is.
  
* Programverziók a GitHub-on
    Mindegyikhez két ZIP fájl tartozik. Az egyikben ('FalconBoard_vX.Y.Z_Executables_Only.Zip')
    csak a futtatható programok és az ikon fájl, a másikban ('FalconBoard_vX.Y.Z_Release.ZIP')
    az összes fájl benne van. Az utóbbira csak új telepítés esetén, illetve akkor van szükség, 
    ha a program fő verziószáma változik. Egyszerűen felül ekell velük írni a régebbit.
