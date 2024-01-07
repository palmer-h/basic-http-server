#include <stdio.h>

#include "mime.h"

/**
 * Attempts to get a MIME type from a filepath
 * 
 * Uses DEFAULT_MIME if cannot find one
*/
int mime_type_from_path(char *mime, char *path) {
    char line[128];
    char *token;
    char *ext = strrchr(path, '.');
    int lineCount = 1;

    if ((ext = strrchr(path, '.')) == NULL) {
        strcpy(mime, DEFAULT_MIME);
        return;
    }

    // Ignore '.' at start of extension
    ++ext;

	FILE *mime_types = fopen(MIME_TYPES_PATH, "r");

    if (mime_types == NULL) {
        return -1;
    }

    while(fgets(line, sizeof(line), mime_types) != NULL) {
        if (lineCount > 1) {
            if ((token = strtok(line, "\t")) != NULL) {
                if (strcmp(token, ext) == 0) {
                    token = strtok(NULL, "\t");
                    strcpy(mime, token);
                    break;
                }
            }
        }

        ++lineCount;
    }

    if (!mime == NULL) {
        strcpy(mime, DEFAULT_MIME);
    }

    fclose(mime_types);

    return 0;
}
