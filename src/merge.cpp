
#include <cstdlib>
#include <algorithm>
#include <iostream>

#include "args.hpp"
#include "bamfile.hpp"
#include "merge.hpp"
#include "util.hpp"

using std::sort;
using std::cerr;
using std::endl;
using std::vector;


#define MERGE_SIZE 128

#define MAX( a, b ) (((a) > (b)) ? (a) : (b))
#define MIN( a, b ) (((a) < (b)) ? (a) : (b))

enum cmp_t { LT, GT, EQ };


inline
cmp_t pos_cmp( const pos_t & x, const pos_t & y )
{
    // test the column, then the insertion, then equal
    if ( x.col > y.col )
        return GT;
    else if ( y.col > x.col )
        return LT;
    else if ( x.ins > y.ins )
        return GT;
    else if ( y.ins < x.ins )
        return LT;
    else
        return EQ;
}


bool ncontrib_cmp( const aligned_t & x, const aligned_t & y )
{
    return x.ncontrib > y.ncontrib;
}


void cerr_triple( const pos_t & x, bool end=true )
{
    cerr << x.col << " " << x.ins << " " << bits2nuc( x.nuc );
    if ( end )
        cerr << endl;
    else
        cerr << ", ";
}


#ifdef DEBUG
#define ABORT( msg ) { \
    cerr << msg << ": "; \
    cerr_triple( xs.data[ xidx ], false ); \
    cerr_triple( ys.data[ yidx ] ); \
    return FAILURE; \
}
#else
#define ABORT( msg ) { return FAILURE; }
#endif


res_t merge_two(
    const aligned_t & xs,
    const aligned_t & ys,
    const args_t & args,
    aligned_t & merged
    )
{
    int overlap = 0;
    int xidx = 0;
    int yidx = 0;
    int midx = 0;
    int mlen = 0;
    int cmp = 0;

    if ( !xs.len || !ys.len )
        ABORT( "insufficient length" )

    // if there is absolutely no hope to reach min_overlap, skip
    if ( xs.rpos < ys.lpos + args.min_overlap &&
            ys.rpos < xs.lpos + args.min_overlap )
        ABORT( "no opportunity for sufficient overlap" )

    cmp = pos_cmp( xs.data[ xidx ], ys.data[ yidx ] );

    // cerr_triple( xs.data[ xidx ], false );
    // cerr_triple( xs.data[ yidx ] );

    // disregard overhangs
    if ( cmp == LT ) {
        for ( ; cmp == LT && xidx + 1 < xs.len; ) {
            ++xidx;
            cmp = pos_cmp( xs.data[ xidx ], ys.data[ yidx ] );
        }
        // if its not a match, it's a gap, then backup one in xs
        if ( cmp == GT && !args.tol_gaps )
            ABORT( "no gaps in ys" )
        // mlen is now the # of xs already visited
        mlen += xidx;
    }
    else if ( cmp == GT ) {
        for ( ; cmp == GT && yidx + 1 < ys.len; ) {
            ++yidx;
            cmp = pos_cmp( xs.data[ xidx ], ys.data[ yidx ] );
        }
        // if its not a match, it's a gap, then backup one in ys
        if ( cmp == GT && !args.tol_gaps )
            ABORT( "no gaps in xs" )
        // mlen is now the # of ys already visited
        mlen += yidx;
    }

    // compute the amount of overlap
    for ( ; xidx < xs.len && yidx < ys.len; ++mlen ) {
        cmp = pos_cmp( xs.data[ xidx ], ys.data[ yidx ] );
        if ( cmp == LT ) {
            if ( !args.tol_gaps )
                ABORT( "no gaps in xs" )
            ++xidx;
        }
        else if ( cmp == GT ) {
            if ( !args.tol_gaps )
                ABORT( "no gaps in ys" )
            ++yidx;
        }
        // if the nucleotides match, move ahead
        else if ( xs.data[ xidx ].nuc == ys.data[ yidx ].nuc ||
                    ( args.tol_ambigs && xs.data[ xidx ].nuc & ys.data[ yidx ].nuc ) ) {
                ++overlap;
                ++xidx;
                ++yidx;
        }
        // nucleotides do not match, abort early
        else
            ABORT( "mismatch" )
    }

    if ( overlap < args.min_overlap )
        ABORT( "insufficient overlap" )

    // get the remainder of either xs or ys, whichever remains
    if ( xidx < xs.len )
        mlen += xs.len - xidx;
    else if ( yidx < ys.len )
        mlen += ys.len - yidx;

    merged.len = mlen;
    merged.data = reinterpret_cast< pos_t * >( malloc( merged.len * sizeof( pos_t ) ) );
    merged.lpos = MIN( xs.lpos, ys.lpos );
    merged.rpos = MAX( xs.rpos, ys.rpos );
    merged.ncontrib = xs.ncontrib + ys.ncontrib;

    if ( !merged.data )
        ABORT( "memory allocation error" )

    for ( xidx = 0, yidx = 0; xidx < xs.len && yidx < ys.len; ) {
        cmp = pos_cmp( xs.data[ xidx ], ys.data[ yidx ] );
        if ( cmp == LT )
            merged.data[ midx++ ] = xs.data[ xidx++ ];
        else if ( cmp == GT )
            merged.data[ midx++ ] = ys.data[ yidx++ ];
        else {
            merged.data[ midx ] = xs.data[ xidx ];
            merged.data[ midx ].cov += ys.data[ yidx ].cov;
            merged.data[ midx ].nuc = MIN( xs.data[ xidx ].nuc, ys.data[ yidx ].nuc );
            ++xidx;
            ++yidx;
            ++midx;
        }
    }

    if ( xidx < xs.len )
        for ( ; xidx < xs.len; ++xidx )
            merged.data[ midx++ ] = xs.data[ xidx ];
    else if ( yidx < ys.len )
        for ( ; yidx < ys.len; ++yidx )
            merged.data[ midx++ ] = ys.data[ yidx ];

#ifndef DEBUG
    if ( midx < mlen )
        cerr << "error: failed to fill 'merged' data" << endl;
    else if ( midx > mlen )
        cerr << "error: overfilled 'merged' data" << endl;
#endif

    return SUCCESS;
}


int merge_clusters(
    const int nread,
    const args_t & args,
    vector< aligned_t > & clusters
    )
{
    vector< aligned_t >::iterator a, b;
    int nclusters;
    bool repeat;

    do {
        repeat = false;

        sort( clusters.begin(), clusters.end(), ncontrib_cmp );

        for ( nclusters = 0, a = clusters.begin(); a != clusters.end(); ++a ) {
begin:
            for ( b = a + 1; b != clusters.end(); ++b ) {
                aligned_t merged = { .data = NULL };
                res_t res = merge_two( *a, *b, args, merged );
                if ( res == SUCCESS ) {
                    aligned_destroy( *a );
                    aligned_destroy( *b );
                    // replace i and remove j
                    *a = merged;
                    clusters.erase( b );
                    fprintf( stderr, "\rprocessed: %9i reads (%6lu clusters)", nread, clusters.size() );
                    repeat = true;
                    goto begin;
                }
                else if ( res == ERROR )
                    return -1;
            }
            if ( a->ncontrib >= args.min_reads )
                ++nclusters;
        }
    } while ( repeat );

    return nclusters;
}

void aligned_destroy( aligned_t & read )
{
    free( read.data );
    read.data = NULL;
    read.len = 0;
}
