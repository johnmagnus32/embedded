int sum(int *p, int n){ int i; int s; s=0; i=0; while(i<n){ s=s+p[i]; i=i+1; } return s; } int main(void){ int a[3]; a[0]=1; a[1]=2; a[2]=3; return sum(a, 3); } // expect: 6
