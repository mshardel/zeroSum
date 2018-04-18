#include "RegressionDataScheme.h"

#define MIN_SUCCESS_RATE 0.90

void RegressionDataScheme::coordinateDescent(int seed) {
#ifdef DEBUG
    double timet, costStart, costEnd;
    struct timespec ts0, ts1;
    clock_gettime(CLOCK_REALTIME, &ts0);
#endif

    costFunction();

#ifdef DEBUG
    costStart = cost;
    PRINT("Loglikelihood: %e lasso: %e ridge: %e cost: %e sum=%e sum=%e\n",
          loglikelihood, lasso, ridge, cost, sum_a(beta, P),
          sum_a_times_b(beta, u, P));
#endif

    activeSet.clear();
    checkWholeActiveSet();

    int warmStart = false;
    int activeSetChange = 0;

    if (activeSet.size() > 0)
        warmStart = true;  // this is a warmstart

    double e1, e2, rn;
    int change, success, attempts, P2, k;
    int gettingWorse = FALSE;

#ifdef AVX_VERSION
    double* last_beta =
        (double*)aligned_alloc(ALIGNMENT, memory_P * K * sizeof(double));
    double* last_offset = (double*)aligned_alloc(ALIGNMENT, K * sizeof(double));
#else
    double* last_beta = (double*)malloc(memory_P * K * sizeof(double));
    double* last_offset = (double*)malloc(K * sizeof(double));
#endif

    std::mt19937_64 mt(seed);
    std::uniform_real_distribution<double> rng(0.0, 1.0);

    for (int steps = 0; steps < 100 && !gettingWorse; steps++) {
#ifdef DEBUG
        PRINT("Step: %d\nFind active set\n", steps);
#endif
        activeSetChange = 0;

        e1 = cost;
        memcpy(last_beta, beta, memory_P * K * sizeof(double));
        memcpy(last_offset, offset, K * sizeof(double));

        for (int l = 0; l < K; l++) {
            approxFailed = FALSE;
            if (type >= BINOMIAL)
                refreshApproximation(l);
            if (useOffset)
                offsetMove(l);

            for (int s = 0; s < P; s++) {
                P2 = 1;

                if (isZeroSum) {
                    if (warmStart) {
                        if (std::find(activeSet.begin(), activeSet.end(), s) !=
                            activeSet.end())
                            continue;

                        P2 = activeSet.size();
                    } else
                        P2 = s;
                }

                for (int h = 0; h < P2; h++) {
                    k = h;

                    if (isZeroSum) {
                        if (warmStart)
                            k = activeSet[h];

                        if (s == k || rng(mt) > downScaler)
                            continue;

                        change = cdMoveZS(s, k, l);

                        if (change) {
                            activeSetChange += checkActiveSet(k);
                            activeSetChange += checkActiveSet(s);
                        }
                    } else {
                        change = cdMove(s, l);
                        if (change)
                            activeSetChange += checkActiveSet(s);
                    }
                }
            }

            if (type == MULTINOMIAL || type == MULTINOMIAL_ZS) {
                optimizeParameterAmbiguity(200);
            }
        }
        costFunction();
        // polish to get small coefs where the cd is undefined to zero
        if (polish) {
            int useOffsetBak = useOffset;
            useOffset = false;
            int useApproxBak = useApprox;
            if (approxFailed)
                useApprox = FALSE;

            for (int l = 0; l < K; l++) {
                double* betaPtr = &beta[INDEX(0, l, memory_P)];
                if (useApprox)
                    refreshApproximation(l, TRUE);

                for (const int& s : activeSet) {
                    if (fabs(betaPtr[s]) < DBL_EPSILON * 100)
                        continue;
                    for (const int& k : activeSet) {
                        if (s == k || fabs(betaPtr[k]) < DBL_EPSILON * 100)
                            continue;

                        lsSaMove(k, s, l, -betaPtr[k]);
                    }
                }
            }

            useOffset = useOffsetBak;
            useApprox = useApproxBak;
        }
        costFunction();
        e2 = cost;
        if (std::isnan(e2) || e1 < e2) {
            memcpy(beta, last_beta, memory_P * K * sizeof(double));
            memcpy(offset, last_offset, K * sizeof(double));
            costFunction();
            break;
        }

        if (activeSetChange == 0 || activeSet.empty()) {
            break;
        }
        warmStart = true;

#ifdef DEBUG
        PRINT("converge\n");
#endif
        int P2 = (isZeroSum) ? activeSet.size() : 1;

        while (TRUE) {
            costFunction();
            e1 = cost;
            memcpy(last_beta, beta, memory_P * K * sizeof(double));
            memcpy(last_offset, offset, K * sizeof(double));

            success = 0;

            for (int l = 0; l < K; l++) {
                approxFailed = FALSE;
                if (type >= BINOMIAL)
                    refreshApproximation(l);
                if (useOffset)
                    offsetMove(l);

                for (const int& s : activeSet) {
                    for (int n = 0; n < P2; n++) {
                        if (isZeroSum) {
                            int k = activeSet[n];
                            if (s == k)
                                continue;
                            change = cdMoveZS(s, k, l);
                        } else {
                            change = cdMove(s, l);
                        }

                        if (change)
                            success++;
                    }
                }

                if (isZeroSum) {
                    attempts = activeSet.size() * (activeSet.size() - 1);

                    if (diagonalMoves && (double)success / (double)attempts <=
                                             MIN_SUCCESS_RATE) {
                        int h;
                        for (const int& s : activeSet) {
                            for (const int& k : activeSet) {
                                if (s == k)
                                    continue;

                                h = floor(rng(mt) * activeSet.size());
                                h = activeSet[h];

                                if (h == s || h == k)
                                    continue;

                                rn = rng(mt);
                                cdMoveZSRotated(s, k, h, l, rn * M_PI);
                            }
                        }
                    }
                }
            }

            if (type == MULTINOMIAL || type == MULTINOMIAL_ZS) {
                optimizeParameterAmbiguity(200);
            }

            costFunction();
            e2 = cost;
#ifdef DEBUG
            PRINT(
                "Loglikelihood: %e lasso: %e ridge: %e cost: %e sum=%e "
                "sum=%e\tChange: e1=%e e2=%e %e %e (success:%d)\n",
                loglikelihood, lasso, ridge, cost, sum_a(beta, P),
                sum_a_times_b(beta, u, P), e1, e2, fabs(e2 - e1), precision,
                success);
#endif
            if (std::isnan(e2) || e1 < e2) {
                gettingWorse = TRUE;
                memcpy(beta, last_beta, memory_P * K * sizeof(double));
                memcpy(offset, last_offset, K * sizeof(double));
                costFunction();
                break;
            }

            if (success == 0 || fabs(e2 - e1) < precision)
                break;
        }
    }

    if (type == MULTINOMIAL || type == MULTINOMIAL_ZS) {
        optimizeParameterAmbiguity(200);
    }

    for (int j = 0; j < K * memory_P; j++)
        if (fabs(beta[j]) < 100 * DBL_EPSILON)
            beta[j] = 0.0;

    free(last_beta);
    free(last_offset);
#ifdef DEBUG
    costFunction();
    costEnd = cost;

    PRINT("Cost start: %e cost end: %e diff %e\n", costStart, costEnd,
          costEnd - costStart);

    clock_gettime(CLOCK_REALTIME, &ts1);
    timet = (ts1.tv_sec - ts0.tv_sec) + (ts1.tv_nsec - ts0.tv_nsec) * 1e-9;
    PRINT("time taken = %e s\n", timet);
#endif
}
