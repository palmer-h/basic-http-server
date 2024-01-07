#ifndef MIME_H_
#define MIME_H_

#define MIME_TYPES_PATH "./mime-types.tsv"
#define DEFAULT_MIME "application/octet-stream"

int mime_type_from_path(char *mime, char *path);

#endif
