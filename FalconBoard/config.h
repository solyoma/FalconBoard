#pragma once
#ifndef _CONFIG_H
#define _CONFIG_H

// version number 0xMMIISS;     M - major, I-minor, s- sub
constexpr int DRAWABLE_ZORDER_BASE = 10000000;  // zOrder for all images is below this number

const qint32 MAGIC_ID = 0x53414d57; // "SAMW" - little endian !! MODIFY this and Save() for big endian processors!
const qint32 MAGIC_VERSION = 0x56020308; // V 02.03.08	2025.10.20.
const QString sVersion = "2.3.8";

// names in configuration

constexpr const char
		// general
	*GEOMETRY = "geometry",
	*WINDOWSTATE = "windowState",
	*VERSION= "version",
	*LANG	= "lang",		// actual language

	*SINGLE = "single",		// allow multiple program instances

	*AUTOSAVEDATA = "saved",// automatic save of changed documents on close
	*AUTOSBCKIMG = "saveb",	// automatic save of page background on close
	*AUTOSAVEPRINT="saveonprint",	// auto save before print/ PDF

	*RECENTLIST = "Recent",	// group for recent file list of max 9 recent files
	*CNTRECENT = "cnt",		// count of recent files
	*BCKGIMG = "bckgrnd",	// background image

	*MODE	= "mode",		// system , dark, black
	*GRID	= "grid",		// grid visible?
	*PAGEGUIDES	= "pageG",	// show page guides?
	*LIMITED= "limited",	// page width limited to visible screen / width
	*LASTDIR = "lastDir",	// last used folder path for documents
	*LASTPDFDIR = "lastPDFDir",	// last used folder path for PDF output
		// PEN COLORS			   defaults: (light,dark)
	*PEN_BLACK = "penBlackOrWhite",	// 000000,FFFFFF
	*PEN_RED   = "penT2",		// FF0000,FF0000
	*PEN_GREEN = "penT3",	// 007D1A,00FF00
	*PEN_BLUE  = "penT4",		// 0000FF,82DBFC
	*PEN_YELLOW= "penT5",	// B704BE,FF00FF
		// PEN COLOR NAMES		    defaults: (light,dark)	- user defined names do not get translated
	*PEN_BLACK_NAME	= "penNBlack",	// 
	*PEN_RED_NAME	= "penNRed",	// 
	*PEN_GREEN_NAME	= "penNGreen",	// 
	*PEN_BLUE_NAME	= "penNBlue",	// 
	*PEN_YELLOW_NAME= "penNYellow",	// 
		// TABS
	*TABS = "Tabs",			// group for tabs
	*TABSIZE = "tabSize",	// how many TABS
	*LASTTAB = "lastTab",	// index of last used TAB
		// printing
			// PDF
	*PSETUP = "Page Setup", // group for page setup
	*PFLAGS = "pflags",		// bit #0: print background image, bit #1: white background
							// bit #2: 0-> no page numbers, 1-> page numbers 
	*PPOSIT = "pposit",		// page number position: 0-6:topLeft,topCenter,topRight,bottomLeft,bottomCenter,bottomRight
	*PDFMLR = "pdfmlr",		// PDF margins: left/right
	*PDFMTB = "pdfmtb",		//				top/bottom
	*PDFGUT = "pdfgut",		//				gutter
	*PDFDPI	= "pdfdpi",		// pdf resolution
	*PDFPGS = "pdfpgs",		// 
			// ALL
	*USERI	= "useri",		// 1: use RESI, 0: use HPXS
	*RESI	= "resi",		// screen resolution index
	*HPXS	= "hpxs",		// horizontal pixels for a page
	*SDIAG	= "sdiag",		// screen diagonal
	*UNITINDEX= "uf",		// index to determines the multipl. factor for number in edScreenDiag number to inch
	*PDFUI	= "pdfui",		// pdf unit
		// drawing
	*GRIDSPACING = "gridspacing",
	*TRANSP = "transp",		// transparency used for screenshots
	*TRANSC = "transc",		// transparent color + fuzzyness, format #FFRRGGBB, default #FFFFFF, FF fuzzyness in %
	*PENSIZES = "pensizes",		// e.g. "30, 3,3,3,3,3,3" - pen width in pixels for eraser, black, red, green, blue, yellow
		// pen color group
	*PENGROUP = "Pens",
	*DEFPENGROUP = "Default Pens",
	*PENGROUPCOUNT = "count",

	*end_of_config = nullptr;
;

constexpr const unsigned
	printBackgroundImageFlag	= 0x01,	// bit #0
	printWhiteBackgroundFlag	= 0x02,	// bit #1
	printGrayScaleFlag			= 0x04,	// bit #2
	printGridFlag				= 0x08,	// bit #3
	printNoImagesFlag			= 0x10,	// bit #4
	openPdfViewerFlag			= 0x20,	// bit #5
	usePageNumbersFlag = 0x40,	// bit #6
	//pageNumberFlagTopBottom		= 0x80,	// bit #7		 0: top, 1: bottom
	//pageNumberFlagLeftCenterRight=0x300,// bit #8-9
	landscapeModeFlag			= 0x400 // bit #10		 0: portrait, 1: landscape		
	  // used for pageNumberFlagLeftCenterRigh
	//pageNumberBitsForLeft		= 0x100,			
	//pageNumberBitsForCenter		= 0x000,
	//pageNumberBitsForRight		= 0x200
;

#endif
