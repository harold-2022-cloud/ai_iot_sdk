#pragma once

#include "cJSON.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int Entity_Http_Event_Report_Post_Json(const char *url, const char *json, size_t json_len);
int Entity_Http_Event_Report_Post(cJSON *root);

#ifdef __cplusplus
}
#endif
