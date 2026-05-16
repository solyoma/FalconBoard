# ![image](https://user-images.githubusercontent.com/37068759/114211459-f61df280-9960-11eb-8e57-eddca19dcf7e.png) 
Last modified/utolsó változtatás: Oct. 23. 2025
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
![FalconBoard - dark mode](https://github.com/user-attachments/assets/5b44a7e7-83f7-46e8-9cf8-446611badeb7)
![FalconBoard - system mode](https://github.com/user-attachments/assets/d09ae2a4-0c50-4d30-ad93-31efc33051e9)
![FalconBoard - light mode with grid shown](https://github.com/user-attachments/assets/7700d2de-4b21-4e06-8368-9f9510e08caa)
## Features (Version 3.x) : 

  - Works under Windows and Linux, and (I hope) on Macs too.
    (did not even try to compile it on Android or iOS)
  - Two interface languages: US English and Hungarian, selectable  
    from inside the program.
  - Can be used with the mouse and with pen input (from a Wacom graphics tablet.)
  - System, ligt, white, dark and black interface modes.
  - Optional grid overlay on background with selectable spacing.
  - Maximum 30 documents can be open at the same time
  - Freehand drawing with seven different colored pens, from which
    the last 6 is redefineable. Colors depend on
    interface modes (light/dark modes), and the default colors are:
    * black/white, red/red, green/green, blue/lighter blue,
      magenta/yellow, brown/brown, gray1/gray2 and the last six be freely
      re-defined for each document.
    * Color transparency ("alpha") can be set for each elements
    * Widths of each pen and the Eraser are adjustable separately.
  - Draw straight lines which may also be exactly horizontal or vertical.
    * 5 different line style can be set and applied to any new or selected
      lines
  - Draw empty or opaque/transparent rectangles, ellipses or circles
  - Unlimited Undo/Redo during a single session.
  - "Unlimited" scrolling from top of "paper" by the keyboard
    (Left, Right, Up, Down, PgUp,PgDown, Home, End,
    alone or together with Ctrl for faster movement), with the mouse or
    pen on Wacom (compatible) graphics tablet.
  - Keyboard shortcuts to most functions (Press **F1** for Help).
  - Export the whole document into a PDF file.
  - Mark a rectangular area with the right button on mouse or tablet.
    * If the **Shift** key is kept down during selection the selection will
      be a square.
    * If both **Alt** and **Ctrl** keys are held during selection the selection will
      be centered on the starting point. This way you can draw concentric
      circles and ellipses, or rectangles and squares around the same
      center point.
    * If the area contains any complete drawings or screenshots then
      the selection will constrict around those, unless
    * The **Alt** key is pressed **before letting the mouse button up** to make the
      selection not to constrict to any elements inside it.
  - While a selection is active
    * Insert vertical space by pressing **F5**. Items below the top of the selection
      are pushed down by the height of the selection.
    * Draw a rectangle with the line style selected around it by 
      pressing the **R** key. 
      + For a contracted
        selection there is a small padding inside this rectangle.
    * Draw an ellipse or a circle inside it by pressing **C**
    * Mark the center of the selection by a dot or an x by pressing the
      corresponding key
    * If there were displayed items inside the area:
        + click inside the area and hold to move the marked items around,
          on the screen release the button to paste it to that position.
        + delete drawings completely inside selection area with the **Del**
          or **BackSpace** keys.
        + lines completely under the marked area can be re-colored
          (including transparency) by clicking on the pen color buttons
          or pressing 1..5.
        + line widths of drawings inside the selection can be changed
          using the right (**]**) and left (**[**) bracket keys
    * if the selected area is empty you can:
        delete empty space from the right or from below with the **Del** key.
    * copy / cut marked area and paste it into any open drawings at at any
      position, even into new documents or new instances
    * paste images from the clipboard (from v2.3.6)
    * press **F7** to rotate the selected area right or left with any angle. Use
      **Shift+F7** to repeat the last rotation.
    * Rotate 90 degrees left by pressing '**9**' or right by pressing '**0**',
        180 degrees by '**8**', mirror vertically or horizontally by '**V**' and '**H**'.
  - Optional save and load of sessions, option to automatic save of data
    and background image on exit.
  - If the document is modified and automatic save is not selected a temporary
    copy of that document will be available at the next program start.
  - Option to restrict to only one instance of the program when running
  - Print the whole document or a range of it. (Qt 15 does not make it possible
    to print just the current page or a list of pages. Workaround: export as pdf
    and print that.)
  - Even documents created in a dark mode can be printed as light mode ones to
    save paint.
  - You may set a static background image: all you draw are on a different layer above
    the background.
  - Use screenshots:
  - Take a screenshot with **F4** from any rectangular area of the desktop and
    paste it into the middle of the screen to show or connotate. (After paste
    you may move it around like any drawings.)
  - You may select a fuzzy color for transparency. Parts of the screenshot
    having that color(range) will be transparent.
  - **Ctrl+Click** on a screenshot to mark it.
  - You can draw straight exactly horizontal or vertical lines if you first
    press the **left mouse button** or the pen to the tablet THEN press and hold the
    Shift key and start drawing.
  - Draw a straight line from the last drawn position to the current one:
    (first press and hold the **Shift** key THEN press the pen to the tablet)
  - Visible screen area can be saved as an image in many different image formats.

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

## Tulajdonságok (3.x verziók):

 - Működik Windows-on és linuxon és (remélem MAC-eken is)
 - Jelenleg kétnyelvű: magyar és angol, ami a programból választható
 - Rendszer színek világos, fehér, sötét és fekete mód
 - Négyzetháló is megjeleníthető, a mérete változtatható.
 - Egérrel és Wacom kompatibilis grafikus tabletek tollával is használható
 - Maximum 30 dokumentum szerkeszthető párhuzamosan
 - Szabadkézi rajzolás hét különböző, programban átdefiniálható, akár minden
   dokumentumban más színű tollal, amelyek színe a világos/sötét módokban
   különböző lehet. Az alapértelmezett színek: 
   * fekete/fehér, piros/piros, zöld/zöld, kék/világoskék, 
     bíbor/sárga, barna-barna, szürke1/szürke2.
   * a színek áttetszősége ("alpha") minden elemre különböző lehet
   * a tollak és a Radir vastagsága választható
 - Húzhatsz egyenes vonalakat, amelyek lehetnek pontosan vízszintesek vagy 
   fügőlegesek is.
   * 5 különböző vonaltípus választható, amiket az új, illetve kiválasztott elemekre 
     is használhatsz
 - Rajzolhatsz üres, vagy kitöltött téglalapokat/négyzeteket és ellipszisek/köröket
 - "Végtelen sok" visszavonás és újra alkalmazás lehetséges minden megnyitott
   dokumentumban, a dokumentum bezárásáig
 - "végtelen" görgetés a "vászon/papír" bal felső sarkához képest mindkét
   irányban a billentyűzettel, illete az egérrel/tollal
 - a legtöbb funkció billentyűzetkombinációkkal könnyen elérhető (**F1**- súgó)
 - az egész dokumentum PDF fájlba exportálható
 - Kijelölhetsz egy téglalap alakú tartományt a jobb gombbal. 
   * Ha **Shift**-et is nyomva tartod négyzet alakú lesz a kijelölés.
   * Ha mind a **Shift** és **Alt** gombot nyomva tartod kijelőölés közben
     a kijelölés a kezdőpont körül nő. Így rajzolhatsz közös középpontú 
     négyzeteket/téglalapokat, illetve koncentrikus köröket.
   * Ha a kijelölt tartományban vannak teljes vonalak, illetve alakzatok és
     képek a kijelölés összehúzodik köréjük és ezzel kiválastja azokat.
   * Ha ezt nem akarjuk, akkor nyomjuk be az **Alt**-ot mielőtt felengedjük az 
     egér/toll gombját.
 - Amíg a kijelölés aktív
   * Az **F5** függőleges üres helyet illeszt be. A kijelölés teteje alatti 
     elemek a kijelölés magasságával lejjebb tolódnak.
   * A kijelölt tartomány köré téglalapot rajzolhatsz az **R** gombbal. A téglalap
     vonala az 5 stílus bármelyike lehet.
     + Ha a kijelölés összehúzódott elemekre, akkor egy kis margó kimarad az elemek
       és a kijelölés között
   * Ellipszist/kört rajzolhatsz a kijelölésbe.
     '**R**', ill. '**C**' gombokkal
   * A kijelölés középpontját megjelölheted a '**.**', ill.'**X**' billentyűkkel
   * Ha a kijelölt tartományba vonalak, alakzatok, vagy képernyőfotók vannak
     + a **DEL** gombbal kitörölheted azokat.
       - Ha a kijelölt terület üres, a **DEL** gombbal az üres területet kitörölheted. 
     + a  kijelölt vonalak vastagságát a '**[**' és '**]**' gombokkal változtathatod
     + a kijelölt tartományba eső objektumokat mozgathatod az egérrel, ill. a tollal,
       kimásolhatod a vágólapra, ami után bármelyik dokumentumba beillesztheted. 
     + A beillesztés helyét a pillanatnyi egér/toll pozíció adja meg.
   * Nem csak a FalconBoardból kimásolt elemeket, de a vágólapon levő
     képeket is beillesztheted a dokumentumodba. (V2.3.6-tól)
   * Az **F7** gombbal tetszőleges szögben elforgathatod a kijelölést. 
     A **Shift+F7**-el pedig az utolsó forgatás szögével. Az '**1**','**0**' gombok 90 
     fokkal, a '**8**' gomb 180 fokkal forgat a '**V**' és '**H**' gombok függőlegesen
     illetve vízszintesen tükrözik a kijelölt területet.
 - Képernyő részlet kivágás és beillesztés ('képernyőfotó') az **F4** gombbal éa *bal egérgombos*
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
    - Mindegyikhez két ZIP fájl tartozik. Az egyikben ('FalconBoard_vX.Y.Z_Executables_Only.Zip')
      csak a futtatható programok és az ikon fájl, a másikban ('FalconBoard_vX.Y.Z_Release.ZIP')
      az összes fájl benne van. Az utóbbira csak új telepítés esetén, illetve akkor van szükség, 
      ha a program fő verziószáma változik. Egyszerűen felül ekell velük írni a régebbit.
