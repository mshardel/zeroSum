#include <R.h>
#include <Rdefines.h>

#include "RegressionCV.h"
#include "RegressionData.h"
#include "mathHelpers.h"

SEXP getListElement(SEXP list, const char* str) {
    SEXP elmt = R_NilValue, names = getAttrib(list, R_NamesSymbol);
    for (int i = 0; i < length(list); i++) {
        if (strcmp(CHAR(STRING_ELT(names, i)), str) == 0) {
            elmt = VECTOR_ELT(list, i);
            break;
        }
    }
    return elmt;
}

RegressionData rListToRegressionData(SEXP _dataObjects) {
    SEXP _x = getListElement(_dataObjects, "x");
    SEXP _y = getListElement(_dataObjects, "y");
    SEXP _beta = getListElement(_dataObjects, "beta");
    SEXP _w = getListElement(_dataObjects, "w");
    SEXP _v = getListElement(_dataObjects, "v");
    SEXP _u = getListElement(_dataObjects, "u");
    SEXP _cSum = getListElement(_dataObjects, "cSum");
    SEXP _alpha = getListElement(_dataObjects, "alpha");
    SEXP _lambdaSeq = getListElement(_dataObjects, "lambda");
    SEXP _gammaSeq = getListElement(_dataObjects, "gamma");
    SEXP _nc = getListElement(_dataObjects, "nc");
    SEXP _fusion = getListElement(_dataObjects, "fusionC");
    SEXP _precision = getListElement(_dataObjects, "precision");
    SEXP _useOffset = getListElement(_dataObjects, "useOffset");
    SEXP _useApprox = getListElement(_dataObjects, "useApprox");
    SEXP _downScaler = getListElement(_dataObjects, "downScaler");
    SEXP _type = getListElement(_dataObjects, "type");
    SEXP _algorithm = getListElement(_dataObjects, "algorithm");
    SEXP _diagonalMoves = getListElement(_dataObjects, "diagonalMoves");
    SEXP _polish = getListElement(_dataObjects, "polish");
    SEXP _foldid = getListElement(_dataObjects, "foldid");
    SEXP _nFold = getListElement(_dataObjects, "nFold");
    SEXP _cores = getListElement(_dataObjects, "cores");
    SEXP _verbose = getListElement(_dataObjects, "verbose");
    SEXP _cvStop = getListElement(_dataObjects, "cvStop");

    int type = INTEGER(_type)[0];
    int N = INTEGER(GET_DIM(_x))[0];
    int P = INTEGER(GET_DIM(_x))[1];
    int K = INTEGER(GET_DIM(_y))[1];
    int nc = INTEGER(_nc)[0];

    RegressionData data(N, P, K, nc, type);

    data.cores = INTEGER(_cores)[0];
    data.verbose = INTEGER(_verbose)[0];
    data.alpha = REAL(_alpha)[0];

    data.lambdaSeq = REAL(_lambdaSeq);
    data.lengthLambda = length(_lambdaSeq);
    data.lambda = data.lambdaSeq[0];

    data.gammaSeq = REAL(_gammaSeq);
    data.lengthGamma = length(_gammaSeq);
    data.gamma = data.gammaSeq[0];

    data.cSum = REAL(_cSum)[0];
    data.useOffset = INTEGER(_useOffset)[0];
    data.useApprox = INTEGER(_useApprox)[0];
    data.downScaler = REAL(_downScaler)[0];
    data.algorithm = INTEGER(_algorithm)[0];
    data.diagonalMoves = INTEGER(_diagonalMoves)[0];
    data.polish = INTEGER(_polish)[0];
    data.precision = REAL(_precision)[0];
    data.foldid = INTEGER(_foldid);
    data.nFold = INTEGER(_nFold)[0];
    data.cvStop = INTEGER(_cvStop)[0];

    double* xR = REAL(_x);
    for (int j = 0; j < data.P; ++j)
        memcpy(&(data.x[INDEX(0, j, data.memory_N)]), &xR[INDEX(0, j, data.N)],
               data.N * sizeof(double));

    double* yR = REAL(_y);
    for (int j = 0; j < data.K; ++j)
        memcpy(&(data.y[INDEX(0, j, data.memory_N)]), &yR[INDEX(0, j, data.N)],
               data.N * sizeof(double));

    double* betaR = REAL(_beta);
    for (int l = 0; l < data.K; ++l) {
        data.offset[l] = betaR[INDEX(0, l, data.P + 1)];
        memcpy(&(data.beta[INDEX(0, l, data.memory_P)]),
               &betaR[INDEX(1, l, data.P + 1)], data.P * sizeof(double));
    }

    if (data.isFusion) {
        double* fusionListR = REAL(_fusion);
        int rows = INTEGER(GET_DIM(_fusion))[0];

        int i, j;
        double x;

        for (int row = 0; row < rows; row++) {
            i = (int)fusionListR[INDEX(row, 0, rows)];
            j = (int)fusionListR[INDEX(row, 1, rows)];
            x = fusionListR[INDEX(row, 2, rows)];

            data.fusionKernel[j] = appendElement(data.fusionKernel[j], i, x);
        }
    }

    memcpy(data.w, REAL(_w), data.N * sizeof(double));
    memcpy(data.wOrg, data.w, data.memory_N * sizeof(double));
    memcpy(data.v, REAL(_v), data.P * sizeof(double));
    memcpy(data.u, REAL(_u), data.P * sizeof(double));

    if (type > 4)
        memcpy(data.yOrg, data.y, data.memory_N * data.K * sizeof(double));

    if (type >= COX) {
        SEXP _status = getListElement(_dataObjects, "status");
        int* status = INTEGER(_status);
        memcpy(data.status, status, data.N * sizeof(int));
        data.calcCoxRegressionD();
    }

    return data;
}

SEXP CV(SEXP _dataObjects) {
    PROTECT(_dataObjects = AS_LIST(_dataObjects));

    RegressionData data(rListToRegressionData(_dataObjects));

    // #ifdef AVX_VERSION_256
    //     PRINT("Compiled with AVX\n");
    // #endif
    //
    // #ifdef FMA
    //     PRINT("Compiled with FMA\n");
    // #endif
    // #ifdef AVX_VERSION_512
    //     PRINT("Compiled with AVX512\n");
    // #endif

#ifdef _OPENMP
    if (data.cores != -1)
        omp_set_num_threads(data.cores);
    else
        omp_set_num_threads(omp_get_max_threads());
#else
    if (data.cores != 0) {
        PRINT("Warning: compiled without openMP support. ");
        PRINT("Argument cores thus has no effect!\n");
        PRINT("Use a compiler which supports openMP for installation ");
        PRINT("of zeroSum if you need parallel execution.\n");
    }
#endif

    if (data.verbose) {
        int core_get = 1;

#ifdef _OPENMP
#pragma omp parallel
        {
#pragma omp single
            core_get = omp_get_num_threads();
        }
#endif
        PRINT("verbose=%d cores wanted=%d cores get: %d\n", data.verbose,
              data.cores, core_get);
    }

    GetRNGstate();
    int seed = (int)(unif_rand() * 1e4);
    PutRNGstate();
    RegressionCV cvRegression(data);
    std::vector<double> cv_stats = cvRegression.doCVRegression(seed);
    SEXP measures;
    PROTECT(measures = allocVector(REALSXP, cv_stats.size()));
    double* m = REAL(measures);

    for (size_t i = 0; i < cv_stats.size(); i++)
        m[i] = cv_stats[i];

    UNPROTECT(2);
    return measures;
}

SEXP checkMoves(SEXP _dataObjects,
                SEXP _number,
                SEXP _k,
                SEXP _s,
                SEXP _t,
                SEXP _l) {
    PROTECT(_dataObjects = AS_LIST(_dataObjects));
    PROTECT(_number = AS_INTEGER(_number));
    int num = INTEGER(_number)[0];

    PROTECT(_k = AS_INTEGER(_k));
    int k = INTEGER(_k)[0];

    PROTECT(_s = AS_INTEGER(_s));
    int s = INTEGER(_s)[0];

    PROTECT(_t = AS_INTEGER(_t));
    int t = INTEGER(_t)[0];

    PROTECT(_l = AS_INTEGER(_l));
    int l = INTEGER(_l)[0];

    RegressionData data = rListToRegressionData(_dataObjects);
    data.costFunction();
    data.approxFailed = FALSE;
    if (data.type > 4) {
        memcpy(data.w, data.wOrg, data.memory_N * sizeof(double));
        memcpy(data.y, data.yOrg, data.memory_N * data.K * sizeof(double));

        for (int i = 0; i < data.N; i++) {
            data.xTimesBeta[i] = data.y[i] - data.offset[0];

            for (int j = 0; j < data.P; j++)
                data.xTimesBeta[i] -=
                    data.beta[j] * data.x[INDEX(i, j, data.memory_N)];
        }
    }

    SEXP result = R_NilValue;

    if (num == 0) {
        data.cdMove(k, l);

        PROTECT(result = allocVector(REALSXP, 1));
        REAL(result)[0] = data.beta[INDEX(k, l, data.memory_P)];
    } else if (num == 1) {
        data.offsetMove(l);

        PROTECT(result = allocVector(REALSXP, 1));
        REAL(result)[0] = data.offset[l];
    } else if (num == 2) {
        data.cdMoveZS(k, s, l);

        PROTECT(result = allocVector(REALSXP, 2));
        REAL(result)[0] = data.beta[INDEX(k, l, data.memory_P)];
        REAL(result)[1] = data.beta[INDEX(s, l, data.memory_P)];
    } else if (num == 3) {
        data.cdMoveZSRotated(k, s, t, l, 37.32);

        PROTECT(result = allocVector(REALSXP, 3));
        REAL(result)[0] = data.beta[INDEX(k, l, data.memory_P)];
        REAL(result)[1] = data.beta[INDEX(s, l, data.memory_P)];
        REAL(result)[2] = data.beta[INDEX(t, l, data.memory_P)];
    }

    UNPROTECT(7);
    return result;
}

SEXP costFunctionWrapper(SEXP _dataObjects) {
    PROTECT(_dataObjects = AS_LIST(_dataObjects));
    RegressionData data = rListToRegressionData(_dataObjects);

    // printf("N: %d K: %d P: %d\n", data.N, data.K, data.P);
    // printf("x: \n");
    // printMatrix(data.x, data.N, data.P);
    // printf("y: \n");
    // printMatrix(data.y, data.N, data.K);

    data.costFunction();
    SEXP returnList, names;
    PROTECT(returnList = allocVector(VECSXP, 5));
    PROTECT(names = allocVector(STRSXP, 5));

    SET_STRING_ELT(names, 0, mkChar("loglikelihood"));
    SET_STRING_ELT(names, 1, mkChar("lasso"));
    SET_STRING_ELT(names, 2, mkChar("ridge"));
    SET_STRING_ELT(names, 3, mkChar("fusion"));
    SET_STRING_ELT(names, 4, mkChar("cost"));

    setAttrib(returnList, R_NamesSymbol, names);

    SEXP loglikelihood, lasso, ridge, fusion, cost;
    PROTECT(loglikelihood = allocVector(REALSXP, 1));
    PROTECT(lasso = allocVector(REALSXP, 1));
    PROTECT(ridge = allocVector(REALSXP, 1));
    PROTECT(fusion = allocVector(REALSXP, 1));
    PROTECT(cost = allocVector(REALSXP, 1));
    REAL(loglikelihood)[0] = data.loglikelihood;
    REAL(lasso)[0] = data.lasso;
    REAL(ridge)[0] = data.ridge;
    REAL(fusion)[0] = data.fusion;
    REAL(cost)[0] = data.cost;

    SET_VECTOR_ELT(returnList, 0, loglikelihood);
    SET_VECTOR_ELT(returnList, 1, lasso);
    SET_VECTOR_ELT(returnList, 2, ridge);
    SET_VECTOR_ELT(returnList, 3, fusion);
    SET_VECTOR_ELT(returnList, 4, cost);

    UNPROTECT(8);
    return returnList;
}

SEXP lambdaMax(SEXP _X, SEXP _res, SEXP _u, SEXP _v, SEXP _alpha) {
    PROTECT(_X = AS_NUMERIC(_X));
    double* x = REAL(_X);

    int* dimX = INTEGER(GET_DIM(_X));
    int N = dimX[0];
    int P = dimX[1];

    PROTECT(_res = AS_NUMERIC(_res));

    int* dimRes = INTEGER(GET_DIM(_res));
    int K = dimRes[1];
    int memory_N = N;

#ifdef AVX_VERSION

    if (memory_N % ALIGNED_DOUBLES != 0)
        memory_N += ALIGNED_DOUBLES - memory_N % ALIGNED_DOUBLES;

    double* res =
        (double*)aligned_alloc(ALIGNMENT, memory_N * K * sizeof(double));

    double* resR = REAL(_res);
    for (int l = 0; l < K; ++l) {
        memcpy(&res[INDEX(0, l, memory_N)], &resR[INDEX(0, l, N)],
               N * sizeof(double));
    }
#else
    double* res = REAL(_res);
#endif

    PROTECT(_u = AS_NUMERIC(_u));
    double* u = REAL(_u);

    PROTECT(_v = AS_NUMERIC(_v));
    double* v = REAL(_v);

    PROTECT(_alpha = AS_NUMERIC(_alpha));
    double alpha = REAL(_alpha)[0];

    double tmp1, tmp2;

    double lambdaMax = DBL_MIN;
    double lambda;
    for (int l = 0; l < K; ++l) {
        for (int k = 2; k < P; ++k) {
            if (u[k] == 0)
                continue;
            for (int s = 1; s < k; ++s) {
                if (u[s] == 0)
                    continue;
                tmp1 = u[k] / u[s];
                tmp2 = alpha * (v[k] + v[s] * tmp1);
                if (fabs(tmp2) < 1000 * DBL_EPSILON)
                    continue;

                lambda = sum_a_sub_b_mul_d_times_c(
                    &x[INDEX(0, s, N)], &x[INDEX(0, k, N)],
                    &res[INDEX(0, l, memory_N)], tmp1, N);
                lambda = fabs(lambda);
                lambda /= tmp2;

                if (lambda > lambdaMax)
                    lambdaMax = lambda;
            }
        }
    }

#ifdef AVX_VERSION
    free(res);
#endif
    UNPROTECT(5);
    return ScalarReal(lambdaMax);
}

extern "C" {

static const R_CallMethodDef callMethods[] = {
    {"CV", (DL_FUNC)&CV, 1},
    {"costFunctionWrapper", (DL_FUNC)&costFunctionWrapper, 1},
    {"checkMoves", (DL_FUNC)&checkMoves, 6},
    {"lambdaMax", (DL_FUNC)&lambdaMax, 5},
    {NULL, NULL, 0}};

void R_init_zeroSum(DllInfo* info) {
    R_registerRoutines(info, NULL, callMethods, NULL, NULL);
}
}
