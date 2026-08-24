/* ============================================================================
 * fibonacci.c — Compute Fibonacci(10) on bare-metal RISC-V
 * ============================================================================
 * This is a real C program that compiles to RV32I machine code and runs
 * on our Verilog processor. No standard library, no printf, no OS.
 *
 * Expected result: main() returns 55 (Fibonacci of 10)
 * The testbench checks register a0 (x10) for this value after ECALL.
 * ============================================================================ */

/* Compute the n-th Fibonacci number iteratively */
int fibonacci(int n) {
    if (n <= 1) return n;

    int prev = 0;
    int curr = 1;

    for (int i = 2; i <= n; i++) {
        int next = prev + curr;
        prev = curr;
        curr = next;
    }

    return curr;
}

/* Entry point — called by crt0.s after setup */
int main(void) {
    int result = fibonacci(10);

    /* Store result to a known memory address for verification */
    /* The testbench can also just check a0 (x10) at halt */
    volatile int *output = (volatile int *)0x00010100;
    *output = result;

    return result;  /* a0 = 55 */
}
