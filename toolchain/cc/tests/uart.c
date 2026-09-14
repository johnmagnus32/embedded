/* Milestone: a cc-compiled program doing real I/O — write a string to the qemu PL011 UART.
 * Checked by matching qemu's stdout (not the exit code), so it's run separately from the others. */
int main(void) {
  char *uart; uart = 0x09000000;             /* PL011 data register on qemu -M virt */
  char msg[15];
  msg[0]=99; msg[1]=99; msg[2]=32; msg[3]=85; msg[4]=65; msg[5]=82; msg[6]=84;   /* "cc UART" */
  msg[7]=32; msg[8]=119; msg[9]=111; msg[10]=114; msg[11]=107; msg[12]=115; msg[13]=10; msg[14]=0;  /* " works\n" */
  int i; i = 0;
  while (msg[i]) { *uart = msg[i]; i = i + 1; }
  return 0;
}
