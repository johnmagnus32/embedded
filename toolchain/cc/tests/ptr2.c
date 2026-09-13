int main(void){ int x; int *p; int **pp; p=&x; pp=&p; **pp=7; return x; }         // expect: 7
