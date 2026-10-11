#include "m_fixed.h"
#include "doomtype.h"
#include "i_system.h"

fixed_t FixedMul(fixed_t a,fixed_t b) { return ((long long)a*b)>>FRACBITS; }
fixed_t FixedDiv2(fixed_t a,fixed_t b) {
    unsigned aa=a<0?0u-(unsigned)a:(unsigned)a;
    unsigned bb=b<0?0u-(unsigned)b:(unsigned)b;
    unsigned whole,remainder,fraction=0,result;
    int bit,negative=(a<0)!=(b<0);
    if(!b) I_Error("FixedDiv2: zero denominator");
    whole=aa/bb;remainder=aa%bb;
    if(whole>32768u)I_Error("FixedDiv2 overflow");
    /* bb <= 2^31: a doubled remainder fits unsigned 32 bits. Generate
     * only the 16 fractional bits instead of general 64-bit division. */
    for(bit=0;bit<16;bit++){
        remainder<<=1;fraction<<=1;
        if(remainder>=bb){remainder-=bb;fraction|=1;}
    }
    result=(whole<<16)|fraction;
    if(result>(negative?0x80000000u:0x7fffffffu))I_Error("FixedDiv2 overflow");
    return negative?(fixed_t)(0u-result):(fixed_t)result;
}
fixed_t FixedDiv(fixed_t a,fixed_t b) {
    unsigned aa=a<0?0u-(unsigned)a:(unsigned)a;
    unsigned bb=b<0?0u-(unsigned)b:(unsigned)b;
    if((aa>>14)>=bb) return (a^b)<0?MININT:MAXINT;
    return FixedDiv2(a,b);
}
