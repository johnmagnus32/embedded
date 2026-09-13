int main(void){ int a; int b; int *p; a=10; b=20; p=&a; return *(p-1)+a; }         // expect: 30
