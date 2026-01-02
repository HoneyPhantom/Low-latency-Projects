#include <iostream>
using namespace std;

const int BLOCK = 32;

void matrix_addition(int n, int a[][1000], int b[][1000], int c[][1000]) {
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) c[i][j] = a[i][j] + b[i][j];
}

void matrix_transpose(int n, int a[][1000], int b[][1000]) {
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) b[j][i] = a[i][j];
}

void matrix_block_transpose(int n, int a[][1000], int b[][1000]) {
    for(int ii = 0; ii < n; ii += BLOCK){
        for(int jj = 0; jj < n; jj += BLOCK){

            for(int i = ii; i < ii + BLOCK; ++i){
                for(int j = jj; j < jj + BLOCK; ++j) b[j][i] = a[i][j];
            }

        }
    }
}

void matrix_multiplication(int n, int a[][1000], int b[][1000], int c[][1000]){
    for(int i = 0; i < n; ++i){
        for(int j = 0; j < n; ++j){
            for(int k = 0; k < n; ++k) c[i][j] += a[i][k] * b[k][j];
        }
    }
}

void matrix_block_multiplication(int n, int a[][1000], int b[][1000], int c[][1000]){
    for(int ii = 0; ii < n; ii += BLOCK){
        for(int jj = 0; jj < n; jj += BLOCK){
            for(int kk = 0; kk < n; kk += BLOCK){

                for(int i = ii; i < ii + BLOCK; ++i){
                    for(int j = jj; j < jj + BLOCK; ++j){
                        for(int k = kk; k < kk + BLOCK; ++k) c[i][j] += a[i][k] * b[k][j];
                    }
                }
            
            }
        }
    }
}

int main(){
    const int N = 1000;

    static int a[N][N], b[N][N], c[N][N];

    matrix_addition(N , a , b , c);             //good
    matrix_transpose(N , a , b);                //bad
    matrix_block_transpose(N , a , b);          //good
    matrix_multiplication(N , a , b , c);       //bad
    matrix_block_multiplication(N , a , b , c); //good

    return 0;
}