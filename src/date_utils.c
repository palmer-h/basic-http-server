#include <time.h>

#include "http.h"
#include "date_utils.h"

void current_date_time(char *s) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(s, 30, HTTP_HEADER_DATE_FORMAT, tm);
}
