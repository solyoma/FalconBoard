#pragma once
#ifndef _CONFIG_H
#define _CONFIG_H

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
	*PENSIZES = "size",		// e.g. "3,3,3,3,3,30" - pen size for black, red, green, blue, yellow, eraser
	*TRANSP = "transp",		// transparency used for screenshots
	*TRANSC = "transc",		// transparent color + fuzzyness, format #FFRRGGBB, default #FFFFFF, FF fuzzyness in %

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
