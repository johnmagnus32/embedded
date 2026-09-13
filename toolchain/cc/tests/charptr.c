int main(void){ char c; char *p; p=&c; *p=65; return *p; }                        // expect: 65
