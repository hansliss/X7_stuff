#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "m_fixed.h"

void I_Error(char *fmt,...){(void)fmt;abort();}
static unsigned state=0x7c21439;
static unsigned random_word(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(void){
    unsigned i;
    assert(FixedDiv(INT_MIN,65536)==INT_MIN);
    assert(FixedDiv(INT_MAX,65536)==INT_MAX);
    assert(FixedDiv(1,0)==INT_MAX);
    assert(FixedDiv(-1,0)==INT_MIN);
    assert(FixedDiv2(-1,3)==-21845);
    assert(FixedDiv2(INT_MIN,INT_MIN)==65536);
    for(i=0;i<200000;i++){
        int a=(int)random_word(),b=(int)random_word(),expected;
        unsigned aa=a<0?0u-(unsigned)a:(unsigned)a;
        unsigned bb=b<0?0u-(unsigned)b:(unsigned)b;
        long long exact=b?(long long)a*65536/b:0;
        if((aa>>14)>=bb)expected=(a^b)<0?INT_MIN:INT_MAX;
        else expected=(int)exact;
        assert(FixedDiv(a,b)==expected);
        if(b&&exact>=INT_MIN&&exact<=INT_MAX)assert(FixedDiv2(a,b)==exact);
        assert(FixedMul(a,b)==(int)(((long long)a*b)>>16));
    }
    puts("Fixed-point tests: 200000 reference comparisons passed");
}
