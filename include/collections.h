#pragma once

#include <Arduino.h>
#include "config.h"

struct Collection {
    char id[48];
    char title[64];
};

struct Word {
    char id[40];
    char spanish[32];
    char english[32];

    char imageFile[64];
    char imageHash[65];

    bool hasImage;
    bool imageAvailable;
};

extern Collection collections[MAX_COLLECTIONS];
extern int totalCollections;
extern int selectedCollectionIndex;

extern Word words[MAX_WORDS];
extern int totalWords;

extern char activeCollection[MAX_COLLECTION_ID];
extern char collectionTitle[64];

extern bool collectionLoaded;

bool loadCollectionCatalog();
bool saveCollectionCatalog(const String& json);

bool loadSelectedCollection();
bool saveSelectedCollection(const char* collectionId);

bool downloadCollectionCatalog();
bool downloadCollectionContent(const char* collectionId);

bool loadActiveCollectionFromLocal();
bool selectCollection(int index);

bool nextCollection();
bool previousCollection();

int findCollectionIndex(const char* collectionId);
bool isCollectionAvailableOffline(const char* collectionId);