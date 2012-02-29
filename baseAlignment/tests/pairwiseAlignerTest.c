/*
 * Copyright (C) 2009-2011 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include "CuTest.h"
#include "sonLib.h"
#include "pairwiseAligner.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

char getRandomChar() {
    char *positions = "AaCcGgTtAaCcGgTtAaCcGgTtAaCcGgTtAaCcGgTtAaCcGgTtAaCcGgTtAaCcGgTtAaCcGgTtAaCcGgTtAaCcGgTtN";
    return positions[st_randomInt(0, strlen(positions))];
}

/*
 * Creates a random DNA sequence of the given length.
 */
char *getRandomSequence(int32_t length) {
    char *seq = st_malloc((length + 1) * sizeof(char));
    for (int32_t i = 0; i < length; i++) {
        seq[i] = getRandomChar();
    }
    seq[length] = '\0';
    return seq;
}

/*
 * Transfroms the given sequence into a different sequence.
 */
char *evolveSequence(const char *startSequence) {
    //Copy sequence
    char *seq = stString_copy(startSequence);

    //Do substitutions
    for (int32_t i = 0; i < strlen(seq); i++) {
        if (st_random() > 0.8) {
            seq[i] = getRandomChar();
        }
    }

    //Do indels
    while (st_random() > 0.2) {
        char *toReplace = getRandomSequence(st_randomInt(2, 4));
        char *replacement = getRandomSequence(st_randomInt(0, 10));
        char *seq2 = stString_replace(seq, toReplace, replacement);
        free(seq);
        free(toReplace);
        free(replacement);
        seq = seq2;
    }

    return seq;
}

static void test_pairwiseAlignerRandom(CuTest *testCase) {
    for (int32_t test = 0; test < 100; test++) {
        //Make a pair of sequences
        char *seqX = getRandomSequence(st_randomInt(0, 100));
        char *seqY = evolveSequence(seqX); //stString_copy(seqX);
        int32_t seqXLength = strlen(seqX);
        int32_t seqYLength = strlen(seqY);
        st_logInfo("Sequence X to align: %s END\n", seqX);
        st_logInfo("Sequence Y to align: %s END\n", seqY);

        //Now do alignment
        PairwiseAlignmentParameters *p = pairwiseAlignmentBandingParameters_construct();
        p->alignAmbiguityCharacters = st_random() > 0.5; //Do this stochastically.
        stList *alignedPairs = getAlignedPairs(seqX, seqY, p);
        //Check the aligned pairs.
        stListIterator *iterator = stList_getIterator(alignedPairs);
        stIntTuple *alignedPair;
        while ((alignedPair = stList_getNext(iterator)) != NULL) {
            CuAssertTrue(testCase, stIntTuple_length(alignedPair) == 3);
            int32_t score = stIntTuple_getPosition(alignedPair, 0);
            int32_t x = stIntTuple_getPosition(alignedPair, 1);
            int32_t y = stIntTuple_getPosition(alignedPair, 2);
            //st_logInfo("Got aligned pair, score: %i x pos: %i y pos: %i\n", score, x, y);
            CuAssertTrue(testCase, score > 0);
            CuAssertTrue(testCase, score <= PAIR_ALIGNMENT_PROB_1);
            CuAssertTrue(testCase, x >= 0);
            CuAssertTrue(testCase, x < seqXLength);
            CuAssertTrue(testCase, y >= 0);
            CuAssertTrue(testCase, y < seqYLength);
        }
        stList_destructIterator(iterator);

        //Cleanup
        free(seqX);
        free(seqY);
        stList_destruct(alignedPairs);
    }
}

static int test_pairwiseAligner_FastRandom_cmpFn(stIntTuple *i, stIntTuple *j) {
    assert(stIntTuple_length(i) == stIntTuple_length(j));
    int32_t k = stIntTuple_getPosition(i, 1) - stIntTuple_getPosition(j, 1);
    int32_t l = stIntTuple_getPosition(i, 2) - stIntTuple_getPosition(j, 2);
    return k == 0 ? l : k;
}

static double weight(stSortedSet *set) {
    stIntTuple *i;
    stSortedSetIterator *it = stSortedSet_getIterator(set);
    double d = 0.0;
    while((i = stSortedSet_getNext(it)) != NULL) {
        d += stIntTuple_getPosition(i, 0);
    }
    stSortedSet_destructIterator(it);
    return d;
}

static void test_pairwiseAligner_FastRandom(CuTest *testCase) {
    for (int32_t test = 0; test < 10; test++) {
        //Make a pair of sequences
        char *seqX = getRandomSequence(st_randomInt(0, 1000));
        char *seqY = evolveSequence(seqX); //stString_copy(seqX);
        int32_t seqXLength = strlen(seqX);
        int32_t seqYLength = strlen(seqY);
        st_logInfo("Sequence X to align: %s END, seq length %i\n", seqX, seqXLength);
        st_logInfo("Sequence Y to align: %s END, seq length %i\n", seqY, seqYLength);

        //Now do alignment
        PairwiseAlignmentParameters *p = pairwiseAlignmentBandingParameters_construct();
        p->alignAmbiguityCharacters = st_random() > 0.5; //Do this stochastically.
        stList *alignedPairs = getAlignedPairs(seqX, seqY, p);
        stList *alignedPairs2 = getAlignedPairs_Fast(seqX, seqY, p);

        stSortedSet *alignedPairsSet = stList_getSortedSet(alignedPairs, (int (*)(const void *, const void *))test_pairwiseAligner_FastRandom_cmpFn);
        stSortedSet *alignedPairsSet2 = stList_getSortedSet(alignedPairs2, (int (*)(const void *, const void *))test_pairwiseAligner_FastRandom_cmpFn);
        stSortedSet *intersectionOfAlignedPairs = stSortedSet_getIntersection(alignedPairsSet, alignedPairsSet2);
        stSortedSet *unionOfAlignedPairs = stSortedSet_getUnion(alignedPairsSet, alignedPairsSet2);

        st_logInfo("Slow size %i, fast size %i, intersection %i, union %i\n", stSortedSet_size(alignedPairsSet), stSortedSet_size(alignedPairsSet2), stSortedSet_size(intersectionOfAlignedPairs), stSortedSet_size(unionOfAlignedPairs));

        st_logInfo("Slow weight %f, fast weight %f, intersection weight %f, union weight %f\n", weight(alignedPairsSet), weight(alignedPairsSet2), weight(intersectionOfAlignedPairs), weight(unionOfAlignedPairs));
    }
}

/*
 * Test the blast heuristic to get the different pairs.
 */

stList *getBlastPairs(const char *sX, const char *sY, int32_t lX, int32_t lY, int32_t trim);


static void test_getBlastPairs(CuTest *testCase) {
    for (int32_t test = 0; test < 10; test++) {
        //Make a pair of sequences
        char *seqX = getRandomSequence(st_randomInt(0, 10000));
        char *seqY = evolveSequence(seqX); //stString_copy(seqX);
        int32_t seqXLength = strlen(seqX);
        int32_t seqYLength = strlen(seqY);
        st_logInfo("Sequence X to align: %s END, seq length %i\n", seqX, seqXLength);
        st_logInfo("Sequence Y to align: %s END, seq length %i\n", seqY, seqYLength);

        int32_t trim = st_randomInt(0, 5);
        st_logInfo("Using random trim %i\n", trim);

        stList *blastPairs = getBlastPairs(seqX, seqY, seqXLength, seqYLength, trim);

        st_logInfo("I got %i blast pairs\n", stList_length(blastPairs));
        int32_t pX = -1;
        int32_t pY = -1;
        for(int32_t i=0; i<stList_length(blastPairs); i++) {
            stIntTuple  *j = stList_get(blastPairs, i);
            CuAssertTrue(testCase, stIntTuple_length(j) == 2);
            int32_t x = stIntTuple_getPosition(j, 0);
            int32_t y = stIntTuple_getPosition(j, 1);
            CuAssertTrue(testCase, x >= 0);
            CuAssertTrue(testCase, y >= 0);
            CuAssertTrue(testCase, x < seqXLength);
            CuAssertTrue(testCase, y < seqYLength);
            CuAssertTrue(testCase, x > pX);
            CuAssertTrue(testCase, y > pY);
            pX = x;
            pY = y;
        }

        stList_destruct(blastPairs);
    }
}

stList *getAnchorPoints(stList *alignedPairs, int32_t minRectangleSize, int32_t lX, int32_t lY);

static void test_filterPairsToGetAnchorPoints(CuTest *testCase) {
    for (int32_t test = 0; test < 10; test++) {
        //Make a pair of sequences
        char *seqX = getRandomSequence(st_randomInt(0, 10000));
        char *seqY = evolveSequence(seqX); //stString_copy(seqX);
        int32_t seqXLength = strlen(seqX);
        int32_t seqYLength = strlen(seqY);

        stList *blastPairs = getBlastPairs(seqX, seqY, seqXLength, seqYLength, 0);
        int32_t minRectangleSize = st_randomInt(0, 20);
        stList *filteredPairs = getAnchorPoints(blastPairs, minRectangleSize, seqXLength, seqYLength);
        int32_t pX = -1;
        int32_t pY = -1;
        for(int32_t i=0; i<stList_length(filteredPairs); i++) {
            stIntTuple *pair = stList_get(filteredPairs, i);
            assert(stList_contains(blastPairs, pair));
            int32_t x = stIntTuple_getPosition(pair, 0);
            int32_t y = stIntTuple_getPosition(pair, 1);
            CuAssertTrue(testCase, x > pX);
            CuAssertTrue(testCase, y > pY);
            CuAssertTrue(testCase, (x - pX) * (y - pY) >= minRectangleSize);
            pX = x;
            pY = y;
        }
        st_logInfo("I got %i filtered pairs from %i pairs\n", stList_length(filteredPairs), stList_length(blastPairs));
    }
}


/*
 * Test the math functions.
 */

double logAdd(double x, double y);

static void test_logAdd(CuTest *testCase) {
    for (int32_t test = 0; test < 100000; test++) {
        double i = st_random();
        double j = st_random();
        double k = i + j;
        double l = exp(logAdd(log(i), log(j)));
        //st_logInfo("I got %f %f\n", k, l);
        CuAssertTrue(testCase, l < k + 0.001);
        CuAssertTrue(testCase, l > k - 0.001);
    }
}

/*
 * Test the sequence functions.
 */

char *convertSequence(const char *s, int32_t sL);

static void test_convertSequence(CuTest *testCase) {
    char cA[10] = { 0, 1, 2, 3, 4, 3, 4, 1, 2, '\0' };
    CuAssertStrEquals(testCase, cA, convertSequence("AcGTntNCG", 9));
    CuAssertStrEquals(testCase, cA, convertSequence("aCGTntNcg", 9));
}

/*
 * Test the forward matrix calculation with trivial example.
 */

double *forwardMatrix(int32_t lX, int32_t lY, const char *sX, const char *sY);

double totalForwardProb(double *fM, int32_t lX, int32_t lY);

static void test_forwardMatrixCalculation(CuTest *testCase) {
    const char *seqX = "";
    const char *seqY = "";
    double *fM = forwardMatrix(1, 1, seqX, seqY);
    CuAssertDblEquals(testCase, fM[0], log(0.9703833696510062), 0.0001);
    CuAssertDblEquals(testCase, fM[1], log(0.0129868352330243), 0.0001);
    CuAssertDblEquals(testCase, fM[2], log(0.0129868352330243), 0.0001);
    CuAssertDblEquals(testCase, fM[3], log((1.0 - 0.9703833696510062 - 2*0.0129868352330243)/2), 0.0001);
    CuAssertDblEquals(testCase, fM[4], log((1.0 - 0.9703833696510062 - 2*0.0129868352330243)/2), 0.0001);
    CuAssertDblEquals(testCase, totalForwardProb(fM, 1, 1), log(1.0/5), 0.0001);
}

/*
 * Test the forward matrix calculation with trivial example.
 */

double *backwardMatrix(int32_t lX, int32_t lY, const char *sX, const char *sY);

double totalBackwardProb(double *fM, int32_t lX, int32_t lY);

static void test_backwardMatrixCalculation(CuTest *testCase) {
    const char *seqX = "";
    const char *seqY = "";
    double *bM = backwardMatrix(1, 1, seqX, seqY);
    CuAssertDblEquals(testCase, bM[0], log(1.0/5), 0.0001);
    CuAssertDblEquals(testCase, bM[1], log(1.0/5), 0.0001);
    CuAssertDblEquals(testCase, bM[2], log(1.0/5), 0.0001);
    CuAssertDblEquals(testCase, bM[3], log(1.0/5), 0.0001);
    CuAssertDblEquals(testCase, bM[4], log(1.0/5), 0.0001);
    CuAssertDblEquals(testCase, totalBackwardProb(bM, 1, 1), log(1.0/5), 0.0001);
}

CuSuite* pairwiseAlignmentTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, test_logAdd);
    SUITE_ADD_TEST(suite, test_pairwiseAlignerRandom);
    SUITE_ADD_TEST(suite, test_pairwiseAligner_FastRandom);
    SUITE_ADD_TEST(suite, test_convertSequence);
    SUITE_ADD_TEST(suite, test_forwardMatrixCalculation);
    SUITE_ADD_TEST(suite, test_backwardMatrixCalculation);
    SUITE_ADD_TEST(suite, test_getBlastPairs);
    SUITE_ADD_TEST(suite, test_filterPairsToGetAnchorPoints);
    return suite;
}
