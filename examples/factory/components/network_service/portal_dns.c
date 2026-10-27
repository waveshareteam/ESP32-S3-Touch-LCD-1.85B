#include "portal_dns.h"
#include <string.h>

size_t portal_dns_reply(const uint8_t *q, size_t n, uint8_t *r, size_t cap, const uint8_t ip[4])
{
    if (!q || !r || !ip || n<17 || n>512 || (q[2]&0xf8) || q[4]!=0 || q[5]!=1) return 0;
    size_t p=12, name_length=0;
    while (p<n && q[p]) {
        size_t count=q[p++];
        if (count>63 || count>n-p || name_length+count+1>255) return 0;
        p+=count;
        name_length+=count+1;
    }
    if (p>=n || n-(p+1)<4) return 0;
    ++p;
    unsigned type=(q[p]<<8)|q[p+1], cls=(q[p+2]<<8)|q[p+3];
    size_t question_end=p+4;
    const int answer=(type==1 && cls==1);
    size_t total=question_end+(answer?16:0);
    if (total>cap) return 0;
    memcpy(r,q,question_end);
    r[2]=0x84|(q[2]&1); r[3]=0;
    r[6]=0; r[7]=(uint8_t)answer;
    memset(r+8,0,4);
    if (answer) {
        const uint8_t rr[12]={0xc0,0x0c,0,1,0,1,0,0,0,30,0,4};
        memcpy(r+question_end,rr,12);
        memcpy(r+question_end+12,ip,4);
    }
    return total;
}
