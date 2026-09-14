struct P { int x; int y; }; int main(void){ struct P p; p.x=3; p.y=4; return p.x*p.y; }               // expect: 12
