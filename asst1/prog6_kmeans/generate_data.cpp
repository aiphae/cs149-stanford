#include <algorithm>
#include <iostream>
#include <math.h>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <string>

using namespace std;

// Functions for generating data
double randDouble() {
  return static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
}

void initData(double *data, int M, int N) {
  int K = 10;
  double *centers = new double[K * N];

  // Gaussian noise
  double mean = 0.0;
  double stddev = 0.5;
  std::default_random_engine generator;
  std::normal_distribution<double> normal_dist(mean, stddev);

  // Randomly create points to center data around
  for (int k = 0; k < K; k++) {
    for (int n = 0; n < N; n++) {
      centers[k * N + n] = randDouble();
    }
  }

  // Even clustering
  for (int m = 0; m < M; m++) {
    int startingPoint = rand() % K; // Which center to start from
    for (int n = 0; n < N; n++) {
      double noise = normal_dist(generator);
      data[m * N + n] = centers[startingPoint * N + n] + noise;
    }
  }

  delete[] centers;
}

void initCentroids(double *clusterCentroids, int K, int N) {
  // Initialize centroids (close together - makes it a bit more interesting)
  for (int n = 0; n < N; n++) {
    clusterCentroids[n] = randDouble();
  }
  for (int k = 1; k < K; k++) {
    for (int n = 0; n < N; n++) {
      clusterCentroids[k * N + n] =
          clusterCentroids[n] + (randDouble() - 0.5) * 0.1;
    }
  }
}

double dist(double *x, double *y, int nDim) {
  double distance = 0.0;
  for (int i = 0; i < nDim; i++) {
    double diff = x[i] - y[i];
    distance += diff * diff;
  }
  return sqrt(distance);
}

void writeData(string filename, double *data, double *clusterCentroids,
               int *clusterAssignments, int *M_p, int *N_p, int *K_p,
               double *epsilon_p) {
  FILE *file = fopen(filename.c_str(), "wb");
  if (!file) {
    cerr << "Cannot open file: " << filename << endl;
    return;
  }

  // Write dimensions and parameters
  fwrite(M_p, sizeof(int), 1, file);
  fwrite(N_p, sizeof(int), 1, file);
  fwrite(K_p, sizeof(int), 1, file);
  fwrite(epsilon_p, sizeof(double), 1, file);

  // Write data arrays
  fwrite(data, sizeof(double), (*M_p) * (*N_p), file);
  fwrite(clusterCentroids, sizeof(double), (*K_p) * (*N_p), file);
  fwrite(clusterAssignments, sizeof(int), *M_p, file);

  fclose(file);
  cout << "Data written to " << filename << endl;
}

int main() {
  srand(7); // SEED = 7

  // Use smaller values for testing, change to original values as needed
  int M = 1e6;      // Original: 1e6
  int N = 100;      // Original: 100
  int K = 3;
  double epsilon = 0.1;

  double *data = new double[M * N];
  double *clusterCentroids = new double[K * N];
  int *clusterAssignments = new int[M];

  // Initialize data
  initData(data, M, N);
  initCentroids(clusterCentroids, K, N);

  // Initialize cluster assignments
  for (int m = 0; m < M; m++) {
    double minDist = 1e30;
    int bestAssignment = -1;
    for (int k = 0; k < K; k++) {
      double d = dist(&data[m * N], &clusterCentroids[k * N], N);
      if (d < minDist) {
        minDist = d;
        bestAssignment = k;
      }
    }
    clusterAssignments[m] = bestAssignment;
  }

  // Generate data file
  writeData("../data.dat", data, clusterCentroids, clusterAssignments, &M, &N,
            &K, &epsilon);

  // Clean up
  delete[] data;
  delete[] clusterCentroids;
  delete[] clusterAssignments;
  
  return 0;
}