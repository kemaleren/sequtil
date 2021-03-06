
#include "util.hpp"


namespace util
{
    char nuc2bits( const char nuc )
    {
        switch ( nuc ) {
        case 'A': return 1;
        case 'C': return 2;
        case 'G': return 4;
        case 'T': return 8;
        case 'M': return 1 | 2;
        case 'R': return 1 | 4;
        case 'W': return 1 | 8;
        case 'S': return 2 | 4;
        case 'Y': return 2 | 8;
        case 'K': return 4 | 8;
        case 'V': return 1 | 2 | 4;
        case 'H': return 1 | 2 | 8;
        case 'D': return 1 | 4 | 8;
        case 'B': return 2 | 4 | 8;
        default:  return 1 | 2 | 4 | 8;
        }
    }


    char bits2nuc( const char bits )
    {
        switch ( bits ) {
        case 1:           return 'A';
        case 2:           return 'C';
        case 4:           return 'G';
        case 8:           return 'T';
        case (1 | 2):     return 'M';
        case (1 | 4):     return 'R';
        case (1 | 8):     return 'W';
        case (2 | 4):     return 'S';
        case (2 | 8):     return 'Y';
        case (4 | 8):     return 'K';
        case (1 | 2 | 4): return 'V';
        case (1 | 2 | 8): return 'H';
        case (1 | 4 | 8): return 'D';
        case (2 | 4 | 8): return 'B';
        default:          return 'N';
        }
    }
}
