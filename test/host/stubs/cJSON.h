#ifndef TEST_STUB_CJSON_H
#define TEST_STUB_CJSON_H

#define cJSON_String 16
#define cJSON_True 2
#define cJSON_False 1

typedef struct cJSON
{
    int type;
    char *valuestring;
    char *json;
} cJSON;

cJSON *cJSON_Parse(const char *text);
cJSON *cJSON_GetObjectItem(cJSON *object, const char *name);
int cJSON_IsString(const cJSON *item);
int cJSON_IsBool(const cJSON *item);
int cJSON_IsTrue(const cJSON *item);
void cJSON_Delete(cJSON *item);

#endif /* TEST_STUB_CJSON_H */
