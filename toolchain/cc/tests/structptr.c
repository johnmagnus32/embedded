struct P { int x; int y; }; int get(struct P *p){ return p->x + p->y; } int main(void){ struct P p; p.x=30; p.y=12; return get(&p); } // expect: 42
