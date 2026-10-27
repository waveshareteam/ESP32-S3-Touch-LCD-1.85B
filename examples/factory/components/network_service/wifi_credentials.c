#include "wifi_credentials.h"
#include "cJSON.h"
#include <ctype.h>
#include <string.h>

bool wifi_credentials_valid(const wifi_credentials_t *c)
{
    if (!c || !memchr(c->ssid,0,sizeof(c->ssid)) || !memchr(c->password,0,sizeof(c->password))) return false;
    size_t ssid_len=strlen(c->ssid), pass_len=strlen(c->password);
    if (!ssid_len || ssid_len>32) return false;
    if (pass_len==0 || (pass_len>=8 && pass_len<=63)) return true;
    if (pass_len!=64) return false;
    for (size_t i=0;i<64;++i) if (!isxdigit((unsigned char)c->password[i])) return false;
    return true;
}

bool wifi_credentials_parse(const char *json, size_t length, wifi_credentials_t *out)
{
    if (!json || !out || length==0 || length>1024 || memchr(json,0,length)) return false;
    /* cJSON stores strings as C strings; reject escaped NUL rather than truncate credentials. */
    for (size_t i=0;i+6<=length;++i) if (!memcmp(json+i,"\\u0000",6)) return false;
    const char *end=NULL;
    cJSON *root=cJSON_ParseWithLengthOpts(json,length,&end,false);
    if (!root) return false;
    while (end<json+length && isspace((unsigned char)*end)) ++end;
    cJSON *ssid=cJSON_GetObjectItemCaseSensitive(root,"ssid");
    cJSON *password=cJSON_GetObjectItemCaseSensitive(root,"password");
    wifi_credentials_t candidate={0};
    bool valid=cJSON_IsObject(root) && end==json+length && cJSON_IsString(ssid) &&
               ssid->valuestring && strlen(ssid->valuestring)<=32 &&
               (!password || (cJSON_IsString(password) && password->valuestring && strlen(password->valuestring)<=64));
    if (valid) {
        strcpy(candidate.ssid,ssid->valuestring);
        if (password) strcpy(candidate.password,password->valuestring);
        valid=wifi_credentials_valid(&candidate);
    }
    cJSON_Delete(root);
    if (valid) *out=candidate;
    return valid;
}
