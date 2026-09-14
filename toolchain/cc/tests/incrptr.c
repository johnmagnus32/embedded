int main(void){ int a[3]; a[0]=1;a[1]=2;a[2]=3; int *p; p=a; int s; s=0; while(p<a+3){ s=s+*p++; } return s; } // expect: 6
