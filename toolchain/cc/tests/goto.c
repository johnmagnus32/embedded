int main(void){ int i; int s; s=0; i=0; loop: if(i>=5) goto done; s+=i; i++; goto loop; done: return s; } // expect: 10
