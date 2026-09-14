// Standalone x86 checks of the production stripe decision and hook fingerprint.
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include "letterbox-extracted.h"
int main() {
    static_assert(sizeof(void*)==4,"test the game's pointer width");
    int failed=0,checks=0;
    for(int flags=0;flags<32;++flags) for(int native : {0,1,7,-1}) {
        const bool enabled=(flags&1)!=0,projection=(flags&2)!=0,session=(flags&4)!=0;
        const bool menu=(flags&8)!=0,matching=(flags&16)!=0;
        const int expected=enabled && projection && session && !menu && matching ? 0 : native;
        ++checks;
        if(LetterboxResult(native,enabled,projection,session,menu,matching)!=expected) ++failed;
    }
    ++checks; if(!LetterboxFingerprint(kLetterboxQueryOrig)) ++failed;
    for(unsigned i=0;i<sizeof(kLetterboxQueryOrig);++i) {
        uint8_t changed[7];memcpy(changed,kLetterboxQueryOrig,sizeof(changed));changed[i]^=1;
        ++checks;if(LetterboxFingerprint(changed)) ++failed;
    }
    printf("Letterbox x86: %d checks, %d failures (gate matrix, exact native return, byte mutations)\n",checks,failed);
    return failed ? 1 : 0;
}
