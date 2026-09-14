// expect: 42
// Proof that -pie self-relocation works at a nonzero load bias.
//   arr : an initialized global array in .data              -> {10, 20, 12}
//   p   : a global pointer initialized to arr's ADDRESS      -> holds the LINK-TIME &arr + an R_ARM_RELATIVE
// If the crt did NOT apply the relocation, p still holds the link-time address (a low value that points
// at garbage once the image is loaded at 0x40200000) and the sum is wrong. It equals 42 only when the
// R_ARM_RELATIVE fixup added the load bias to p, making it point at the runtime &arr.
int arr[3] = { 10, 20, 12 };
int *p = &arr;   /* &arr == &arr[0] numerically; the value stored is a link-time address (an ABS32 -> R_ARM_RELATIVE) */
int main(void) { return p[0] + p[1] + p[2]; }
