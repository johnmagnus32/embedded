int main(void){ char b[4]; b[0]=72; b[1]=105; b[2]=0; int i; i=0; int n; n=0; while(b[i]){ n=n+b[i]; i=i+1; } return n; } // expect: 177
