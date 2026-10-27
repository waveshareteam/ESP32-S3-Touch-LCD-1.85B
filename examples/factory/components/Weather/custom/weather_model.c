#include "weather_model.h"
#include "cJSON.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool numeric_value(const cJSON *value, double *number)
{
    if (cJSON_IsNumber(value)) *number=value->valuedouble;
    else if (cJSON_IsString(value) && value->valuestring && value->valuestring[0]) {
        char *end;
        *number=strtod(value->valuestring,&end);
        if (end==value->valuestring) return false;
        while (isspace((unsigned char)*end)) ++end;
        if (*end) return false;
    } else return false;
    return isfinite(*number);
}

static bool format_value(const cJSON *object, const char *key, char *out, size_t cap,
                          double minimum, double maximum, int decimals)
{
    double value;
    if (!numeric_value(cJSON_GetObjectItemCaseSensitive(object,key),&value) || value<minimum || value>maximum) {
        snprintf(out,cap,"--"); return false;
    }
    snprintf(out,cap,decimals?"%.1f":"%.0f",value);
    return true;
}

static const char *translate_weather(const char *text, weather_condition_t *condition)
{
    static const struct { const char *cn,*en; weather_condition_t icon; } known[]={
        {"晴","Sunny",WEATHER_SUNNY},{"多云","Cloudy",WEATHER_CLOUDY},{"阴","Overcast",WEATHER_OVERCAST},
        {"小雨","Light Rain",WEATHER_RAIN},{"中雨","Moderate Rain",WEATHER_RAIN},{"大雨","Heavy Rain",WEATHER_RAIN},
        {"暴雨","Rainstorm",WEATHER_RAIN},{"阵雨","Shower",WEATHER_RAIN},{"雷阵雨","Thunder Shower",WEATHER_THUNDER},
        {"雾","Fog",WEATHER_FOG},{"霾","Haze",WEATHER_FOG},{"小雪","Light Snow",WEATHER_SNOW},
        {"中雪","Moderate Snow",WEATHER_SNOW},{"大雪","Heavy Snow",WEATHER_SNOW}
    };
    for (size_t i=0;i<sizeof(known)/sizeof(known[0]);++i)
        if (!strcmp(text,known[i].cn)) { *condition=known[i].icon; return known[i].en; }
    if (strstr(text,"雷")) { *condition=WEATHER_THUNDER; return "Thunderstorms"; }
    if (strstr(text,"雪")) { *condition=WEATHER_SNOW; return "Snow"; }
    if (strstr(text,"雨")) { *condition=WEATHER_RAIN; return "Rain"; }
    if (strstr(text,"雾") || strstr(text,"霾")) { *condition=WEATHER_FOG; return "Fog / Haze"; }
    if (strstr(text,"云")) { *condition=WEATHER_CLOUDY; return "Cloudy"; }
    if (strstr(text,"阴")) { *condition=WEATHER_OVERCAST; return "Overcast"; }
    if (strstr(text,"晴")) { *condition=WEATHER_SUNNY; return "Sunny"; }
    *condition=WEATHER_UNKNOWN; return "Unknown";
}

static const char *translate_wind(const char *text)
{
    static const struct { const char *cn,*en; } known[]={
        {"东风","E"},{"南风","S"},{"西风","W"},{"北风","N"},{"东南风","SE"},
        {"东北风","NE"},{"西南风","SW"},{"西北风","NW"},{"无持续风向","VAR"}
    };
    for (size_t i=0;i<sizeof(known)/sizeof(known[0]);++i) if (!strcmp(text,known[i].cn)) return known[i].en;
    return "--";
}

weather_parse_result_t weather_parse_json(const char *json, size_t length, const char *city, weather_data_t *out)
{
    if (!json || !out || !city || !length || length>16384 || memchr(json,0,length)) return WEATHER_PARSE_INVALID;
    const char *end=NULL;
    cJSON *root=cJSON_ParseWithLengthOpts(json,length,&end,false);
    if (!root) return WEATHER_PARSE_INVALID;
    weather_parse_result_t result=WEATHER_PARSE_INVALID;
    while (end<json+length && isspace((unsigned char)*end)) ++end;
    if (end!=json+length || !cJSON_IsObject(root)) goto done;
    cJSON *code=cJSON_GetObjectItemCaseSensitive(root,"code");
    double api_code;
    if (code && (!numeric_value(code,&api_code) || api_code!=200)) { result=WEATHER_PARSE_API_ERROR; goto done; }
    cJSON *weather=cJSON_GetObjectItemCaseSensitive(root,"weather1");
    cJSON *now=cJSON_GetObjectItemCaseSensitive(root,"nowinfo");
    cJSON *date=cJSON_GetObjectItemCaseSensitive(root,"uptime");
    if (!cJSON_IsString(weather) || !weather->valuestring || !weather->valuestring[0] || !cJSON_IsObject(now)) goto done;
    weather_data_t candidate={0};
    snprintf(candidate.city,sizeof(candidate.city),"%s",city);
    snprintf(candidate.date,sizeof(candidate.date),"%.10s",cJSON_IsString(date)?date->valuestring:"--");
    snprintf(candidate.weather,sizeof(candidate.weather),"%s",translate_weather(weather->valuestring,&candidate.condition));
    bool valid_temp=format_value(now,"temperature",candidate.temp,sizeof(candidate.temp),-100,100,1);
    bool valid_humi=format_value(now,"humidity",candidate.humi,sizeof(candidate.humi),0,100,0);
    bool valid_speed=format_value(now,"windSpeed",candidate.wind_speed,sizeof(candidate.wind_speed),0,1000,1);
    cJSON *wind=cJSON_GetObjectItemCaseSensitive(now,"windDirection");
    snprintf(candidate.wind_dir,sizeof(candidate.wind_dir),"%s",cJSON_IsString(wind)?translate_wind(wind->valuestring):"--");
    if (!valid_temp && !valid_humi && !valid_speed) goto done;
    *out=candidate;
    result=WEATHER_PARSE_OK;
done:
    cJSON_Delete(root);
    return result;
}

static bool append(char **cursor, size_t *remaining, const char *text, bool encode)
{
    static const char hex[]="0123456789ABCDEF";
    for (const unsigned char *p=(const unsigned char *)text;*p;++p) {
        bool plain=(*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||strchr("-._~",*p);
        size_t count=encode&&!plain?3:1;
        if (*remaining<=count) return false;
        if (count==3) { *(*cursor)++='%'; *(*cursor)++=hex[*p>>4]; *(*cursor)++=hex[*p&15]; }
        else *(*cursor)++=(char)*p;
        *remaining-=count;
    }
    **cursor=0;
    return true;
}

bool weather_build_url(char *out, size_t capacity, const char *id, const char *key, const char *province, const char *city)
{
    if (!out || !capacity || !id || !*id || !key || !*key || !province || !city) return false;
    char *cursor=out; size_t remaining=capacity; out[0]=0;
    bool ok=append(&cursor,&remaining,"http://cn.apihz.cn/api/tianqi/tqyb.php?id=",false) &&
            append(&cursor,&remaining,id,true) && append(&cursor,&remaining,"&key=",false) &&
            append(&cursor,&remaining,key,true) && append(&cursor,&remaining,"&sheng=",false) &&
            append(&cursor,&remaining,province,true) && append(&cursor,&remaining,"&place=",false) &&
            append(&cursor,&remaining,city,true);
    if (!ok) out[0]=0;
    return ok;
}
