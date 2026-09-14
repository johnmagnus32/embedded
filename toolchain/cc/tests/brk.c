int main(void){ int s; s=0; int i; for(i=0;i<20;i++){ if(i==3) continue; if(i==7) break; s+=i; } return s; } // expect: 18
