#include <iostream>
#include <cmath>
#include <mpi.h>

#define EPSILON (10e-5)
#define INF (10e6)

void copyMem(double *ptrTo, double *ptrFrom, int k) {
    for (int i = 0; i < k; ++i) {
        ptrTo[i] = ptrFrom[i];
    }
}

void MPI1_fillData(double *A, double *x, double *b, int n, int m, int idx) {
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            A[j + i*n] = ((idx + i) == j) ? 2.0 : 1.0;
        }
    }
    for (int i = 0; i < n; ++i) {
        x[i] = 0;
        b[i] = n+1;
    }
}

double* MPI1_calculateYn(double *A, double *xn, double *b, int n, int m, int idx) {
    auto *yn = new double[n]();
    for (int i = 0; i < m; ++i) {
        yn[idx + i] = -b[idx + i];
        for (int j = 0; j < n; ++j) {
            yn[idx + i] += A[j + i*n] * xn[idx + i];
        }
    }
    return yn;
}

bool MPI1_isSolutionFound(double *yn, double *b, int n) {
    double *length = new double[2]();
    for (int i = 0; i < n; ++i) {
        length[0] += yn[i] * yn[i];
        length[1] += b[i] * b[i];
    }
    bool isFound = sqrt(std::abs(length[0] / length[1])) < EPSILON;
    delete[] length;
    return isFound;
}

double* MPI1_calculateTn(double *A, double *yn, int n, int m, int idx) {
    auto *tn = new double[2]();
    double AynTmp;
    for (int i = 0; i < m; ++i) {
        AynTmp = 0.0;
        for (int j = 0; j < n; ++j) {
            AynTmp += A[j + i*n] * yn[idx + i];
        }
        tn[0] += yn[idx + i] * AynTmp;
        tn[1] += AynTmp * AynTmp;
    }
    return tn;
}

void MPI1_calculateNextX(double *x, double *yn, double tn, int n) {
    for (int i = 0; i < n; ++i) {
        x[i] -= yn[i] * tn;
    }
}

double* mpi1(int n, int m, int idx) {

    auto *A = new double[n * m];
    auto *x = new double[n];
    auto *b = new double[n];

    MPI1_fillData(A, x, b, n, m, idx);

    for (int k = 0; k < INF; ++k) {
        // y(n) = Ax(n) - b
        auto *yn = MPI1_calculateYn(A, x, b, n, m, idx);
        auto *ynFinal = new double[n];
        MPI_Allreduce(yn, ynFinal, n, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        delete[] yn;

        // Check |y(n)| / |b| < Epsilon
        if (MPI1_isSolutionFound(ynFinal, b, n)) {
            delete[] ynFinal;
            break;
        }

        // t(n) = (y(n), Ay(n)) / (Ay(n), Ay(n))
        auto *tn = MPI1_calculateTn(A, ynFinal, n, m, idx);
        auto *tnFinal = new double[2];
        MPI_Allreduce(tn, tnFinal, 2, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        double tnResult = tnFinal[0] / tnFinal[1];
        delete[] tn;
        delete[] tnFinal;

        // x(n+1) = x(n) - t(n)*y(n)
        MPI1_calculateNextX(x, ynFinal, tnResult, n);
        delete[] ynFinal;
    }

    delete[] A;
    delete[] b;

    return x;
}

void MPI2_fillData(double *A, double *x, double *b, int n, int m, int idx) {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            A[j + i*m] = (i == (idx + j)) ? 2.0 : 1.0;
        }
    }
    for (int i = 0; i < m; ++i) {
        x[i] = 0;
        b[i] = n+1;
    }
}

double* MPI2_calculateYn(double *A, double *xn, double *b, int n, int m, int idx) {
    auto *yn = new double[n]();
    for (int i = 0; i < n; ++i) {
        if (idx <= i  && i < idx + m) {
            yn[i] = -b[i - idx];
        }
        for (int j = 0; j < m; ++j) {
            yn[i] += A[j + i*m] * xn[j];
        }
    }
    return yn;
}

double* MPI2_getVectorsLength(double *yn, double *b, int m) {
    double *length = new double[2]();
    for (int i = 0; i < m; ++i) {
        length[0] += yn[i] * yn[i];
        length[1] += b[i] * b[i];
    }
    return length;
}

double* MPI2_calculateAyn(double *A, double *yn, int n, int m) {
    auto *Ayn = new double[n]();
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            Ayn[i] += A[j + i*m] * yn[j];
        }
    }
    return Ayn;
}

double* MPI2_calculateTn(double *yn, double *Ayn, int m) {
    double *tn = new double[2]();
    for (int i = 0; i < m; ++i) {
        tn[0] += yn[i] * Ayn[i];
        tn[1] += Ayn[i] * Ayn[i];
    }
    return tn;
}

double* MPI2_calculateNextX(double *xn, double *yn, double tn, int n, int m, int idx) {
    auto *xNext = new double[n]();
    for (int i = 0; i < m; ++i) {
        xNext[idx + i] = xn[i] - tn * yn[i];
    }
    return xNext;
}

double* mpi2(int n, int m, int idx) {

    auto *A = new double[m * n];
    auto *x = new double[m];
    auto *b = new double[m];

    MPI2_fillData(A, x, b, n, m, idx);

    auto *result = new double [n];

    for (int k = 0; k < INF; ++k) {
        // y(n) = Ax(n) - b
        auto *yn = MPI2_calculateYn(A, x, b, n, m, idx);
        auto *ynFinal = new double[n];
        MPI_Allreduce(yn, ynFinal, n, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        delete[] yn;

        // y(n) cut
        auto *ynCut = new double [m];
        copyMem(ynCut, &ynFinal[idx], m);

        // Calculating |y(n)| / |b| < Epsilon
        auto *length = MPI2_getVectorsLength(ynCut, b, m);
        auto *lengthFinal = new double[2];
        MPI_Allreduce(length, lengthFinal, 2, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        double lengthAttitude = sqrt(std::abs(lengthFinal[0] / lengthFinal[1]));
        delete[] length;
        delete[] lengthFinal;

        // Check if solution is found
        if (lengthAttitude < EPSILON) {
            delete[] ynCut;
            break;
        }

        // Ay(n) calculation
        auto *Ayn = MPI2_calculateAyn(A, ynCut, n, m);
        auto *AynFinal = new double [n];
        MPI_Allreduce(Ayn, AynFinal, n, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        delete[] Ayn;

        // Ay(n) cut
        auto *AynCut = new double [m];
        copyMem(AynCut, &AynFinal[idx], m);
        delete[] AynFinal;

        // t(n) = (y(n), Ay(n)) / (Ay(n), Ay(n))
        auto *tn = MPI2_calculateTn(ynCut, AynCut, m);
        auto *tnFinal = new double[2];
        MPI_Allreduce(tn, tnFinal, 2, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        double tnResult = tnFinal[0] / tnFinal[1];
        delete[] AynCut;
        delete[] tn;
        delete[] tnFinal;

        // x(n+1) = x(n) - t(n)*y(n)
        auto *nextX = MPI2_calculateNextX(x, ynCut, tnResult, n, m, idx);
        MPI_Allreduce(nextX, result, n, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        delete[] ynCut;
        delete[] nextX;

        // x(n) cut
        copyMem(x, &result[idx], m);
    }

    delete[] A;
    delete[] x;
    delete[] b;

    return result;
}

void printAnswer(double *x, int n) {
    for (int i = 0; i < n; ++i) {
        printf("x%3d: %.1f\n", i, x[i]);
    }
}

int calculateIdx(int n, int procTotal, int procRank) {
    int idx = 0;
    for (int i = 0; i < procRank; ++i) {
        idx += (n - idx) / (procTotal - i);
    }
    return idx;
}

int main(int argc, char* argv[]) {

    int procTotal, procRank;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &procTotal);
    MPI_Comm_rank(MPI_COMM_WORLD, &procRank);

    if (argc != 3) {
        if (!procRank) {
            printf("Wrong arguments number\n");
        }
        MPI_Finalize();
        return 0;
    }

    int variant = atoi(argv[1]);
    int n = atoi(argv[2]);

    int idx = calculateIdx(n, procTotal, procRank);
    int m = (n - idx) / (procTotal - procRank);

    double startTime = MPI_Wtime();
    double *result = variant == 1 ? mpi1(n, m, idx) : mpi2(n, m, idx);
    double endTime = MPI_Wtime();

    if (!procRank) {
        printAnswer(result, n);
        printf("Work time: %.2f seconds\n", endTime - startTime);
    }
    delete[] result;

    MPI_Finalize();
    return 0;
}
