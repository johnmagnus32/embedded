struct P{int x;char c;int z;}; struct P g = {7, 65, 9}; int *gp = &g; int main(void){ return g.x+g.c+g.z; } // expect: 81
