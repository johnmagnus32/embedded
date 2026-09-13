int main(void){ int x; int *p; p=&x; *p=42; return x; }                          // expect: 42
