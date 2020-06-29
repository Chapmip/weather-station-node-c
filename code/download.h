// Header file for wrapper routines for SHDesigns Web-Based Downloader

// Copyright (c) 2006, Ian Chapman (Chapmip Consultancy)

// All rights reserved, except for those rights implicitly granted to
// GitHub Inc by publishing on GitHub and those rights granted by
// commercial agreement with the author.


#ifndef	DOWNLOAD_H
#define	DOWNLOAD_H


// Constant definitions

#define DL_DEF_PATH		"/update.html"


// Function prototypes

int check_download(char *web_host, char *path, int send_full_url);
int get_download(void);


// Compilation switches for download.c and WEB_DL.c

#define COPY2FLASH
#define WEB_DEBUG
#define ST_VERSION_168

// Uncomment the next line to download to serial flash rather than main flash

#define EXTERNAL_STORAGE

// Path to appropriate copy loader

#ifdef EXTERNAL_STORAGE
#define COPY_LOADER "..\flashcopy\sflashcopy37k-d.bin"
#else
#define COPY_LOADER "..\flashcopy\flashcopy-d.bin"
#endif


#endif	// DOWNLOAD_H
