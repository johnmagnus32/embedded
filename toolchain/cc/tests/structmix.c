struct S { char c; int n; }; int main(void){ struct S s; s.c=65; s.n=10; return s.c + s.n; }            // expect: 75
