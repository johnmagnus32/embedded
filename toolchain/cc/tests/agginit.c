struct P{int x;int y;int z;}; int main(void){ struct P p = {3,4,5}; int a[3]={10,20,12}; return p.x*p.y+p.z + a[0]+a[1]+a[2]; } // expect: 59
