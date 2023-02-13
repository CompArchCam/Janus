#include <stdio.h>

#define N 10000
#define M 99999

int main(int a)
{
    int c=0;
    int d=10;
    for (int b=0; b<N; b++)
    {
        c=c+a;
        for (int j=0; j<M; j++)
        {
            d+=d/(c+1)*(j+M)-(M+j)*j;
        }
    }

    printf("Exit\n");
    return d;
}

