/* Exercises inline asm: register-pinned vars + __asm__ svc (semihosting SYS_EXIT_EXTENDED). */
// expect: 77
int main(void){
  int block[2]; block[0] = 0x20026; block[1] = 77;   /* {ADP_Stopped_ApplicationExit, exit code} */
  register int r0 __asm__("r0") = 0x20;               /* SYS_EXIT_EXTENDED */
  register int r1 __asm__("r1") = (int)block;
  __asm__ volatile("svc 0x123456" : : "r"(r0), "r"(r1) : "memory");
  return 0;                                           /* unreached */
}
