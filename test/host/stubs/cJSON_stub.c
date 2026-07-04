#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_range(const char *begin, const char *end)
{
    size_t len = (size_t)(end - begin);
    char *copy = (char *)malloc(len + 1u);

    if (copy == NULL)
    {
        return NULL;
    }
    memcpy(copy, begin, len);
    copy[len] = '\0';
    return copy;
}

static char *find_string_value(const char *json, const char *name)
{
    char needle[64];
    const char *pos;
    const char *begin;
    const char *end;

    if (snprintf(needle, sizeof(needle), "\"%s\":\"", name) <= 0)
    {
        return NULL;
    }
    pos = strstr(json, needle);
    if (pos == NULL)
    {
        return NULL;
    }
    begin = pos + strlen(needle);
    end = strchr(begin, '"');
    if (end == NULL)
    {
        return NULL;
    }
    return dup_range(begin, end);
}

cJSON *cJSON_Parse(const char *text)
{
    cJSON *root;

    if (text == NULL || text[0] != '{')
    {
        return NULL;
    }
    root = (cJSON *)calloc(1u, sizeof(*root));
    if (root == NULL)
    {
        return NULL;
    }
    root->json = dup_range(text, text + strlen(text));
    if (root->json == NULL)
    {
        free(root);
        return NULL;
    }
    return root;
}

cJSON *cJSON_GetObjectItem(cJSON *object, const char *name)
{
    cJSON *item;
    char *value;

    if (object == NULL || object->json == NULL || name == NULL)
    {
        return NULL;
    }
    value = find_string_value(object->json, name);
    if (value == NULL)
    {
        return NULL;
    }
    item = (cJSON *)calloc(1u, sizeof(*item));
    if (item == NULL)
    {
        free(value);
        return NULL;
    }
    item->type = cJSON_String;
    item->valuestring = value;
    return item;
}

int cJSON_IsString(const cJSON *item)
{
    return item != NULL && ((item->type & 0xFF) == cJSON_String) && item->valuestring != NULL;
}

int cJSON_IsBool(const cJSON *item)
{
    return item != NULL && (item->type == cJSON_True || item->type == cJSON_False);
}

int cJSON_IsTrue(const cJSON *item)
{
    return item != NULL && item->type == cJSON_True;
}

void cJSON_Delete(cJSON *item)
{
    if (item != NULL)
    {
        free(item->valuestring);
        free(item->json);
        free(item);
    }
}
