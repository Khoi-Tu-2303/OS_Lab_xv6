#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/riscv.h"
#include "user/user.h"

void
err(char *why)
{
  printf("pgtbltest: %s, pid=%d\n", why, getpid());
  exit(1);
}

// Test 1: Standard MIT pgaccess test
void
pgaccess_test(void)
{
  char *buf;
  unsigned int abits = 0;

  printf("Starting pgaccess_test...\n");
  buf = malloc(32 * PGSIZE);
  if (buf == 0)
    err("malloc failed");

  // Initial call to clear any existing access bits
  if (pgaccess(buf, 32, &abits) < 0)
    err("initial pgaccess failed");

  // Access pages 1, 2, and 30
  buf[PGSIZE * 1] += 1;
  buf[PGSIZE * 2] += 1;
  buf[PGSIZE * 30] += 1;

  if (pgaccess(buf, 32, &abits) < 0)
    err("pgaccess after access failed");

  unsigned int expected = (1U << 1) | (1U << 2) | (1U << 30);
  if (abits != expected) {
    printf("Expected bitmask: 0x%x, got: 0x%x\n", expected, abits);
    err("incorrect access bits set");
  }

  free(buf);
  printf("[PASS] pgaccess_test: OK (bits 1, 2, 30 detected)\n");
}

// Test 2: Verify that pgaccess clears the accessed bit
void
test_clear_bit(void)
{
  char *buf;
  unsigned int abits = 0;

  printf("Starting test_clear_bit...\n");
  buf = malloc(32 * PGSIZE);
  if (buf == 0)
    err("malloc failed");

  // Access page 3
  buf[PGSIZE * 3] += 1;

  // First call should see page 3
  if (pgaccess(buf, 32, &abits) < 0)
    err("pgaccess failed");
  if ((abits & (1U << 3)) == 0)
    err("page 3 access not detected");

  // Second call without touching memory must return 0
  if (pgaccess(buf, 32, &abits) < 0)
    err("second pgaccess failed");
  if (abits != 0) {
    printf("Expected 0 after clear, got: 0x%x\n", abits);
    err("accessed bit was not cleared by pgaccess");
  }

  free(buf);
  printf("[PASS] test_clear_bit: OK (bits cleared on check)\n");
}

// Test 3: Test read access vs write access
void
test_read_and_write(void)
{
  char *buf;
  unsigned int abits = 0;

  printf("Starting test_read_and_write...\n");
  buf = malloc(32 * PGSIZE);
  if (buf == 0)
    err("malloc failed");

  // Clear bits
  pgaccess(buf, 32, &abits);

  // Read from page 5
  volatile char c = buf[PGSIZE * 5];
  (void)c;

  // Write to page 12
  buf[PGSIZE * 12] = 42;

  if (pgaccess(buf, 32, &abits) < 0)
    err("pgaccess failed");

  unsigned int expected = (1U << 5) | (1U << 12);
  if (abits != expected) {
    printf("Expected bitmask: 0x%x, got: 0x%x\n", expected, abits);
    err("read/write access bits mismatch");
  }

  free(buf);
  printf("[PASS] test_read_and_write: OK (both read and write set PTE_A)\n");
}

// Test 4: Boundary conditions and error handling
void
test_error_handling(void)
{
  char *buf;
  unsigned int abits = 0;

  printf("Starting test_error_handling...\n");
  buf = malloc(32 * PGSIZE);

  // Negative len
  if (pgaccess(buf, -1, &abits) != -1)
    err("pgaccess accepted negative len");

  // len > 32
  if (pgaccess(buf, 33, &abits) != -1)
    err("pgaccess accepted len > 32");

  // Invalid user pointer for mask
  if (pgaccess(buf, 32, (void*)0) != -1)
    err("pgaccess accepted NULL mask");

  if (pgaccess(buf, 32, (void*)0xffffffffffffffffULL) != -1)
    err("pgaccess accepted out-of-bounds mask");

  free(buf);
  printf("[PASS] test_error_handling: OK (invalid args rejected)\n");
}

// Test 5: Variable length scanning (e.g., len = 8)
void
test_small_len(void)
{
  char *buf;
  unsigned int abits = 0;

  printf("Starting test_small_len...\n");
  buf = malloc(16 * PGSIZE);
  if (buf == 0)
    err("malloc failed");

  // Clear
  pgaccess(buf, 8, &abits);

  // Access page 0 and 7
  buf[PGSIZE * 0] += 1;
  buf[PGSIZE * 7] += 1;

  if (pgaccess(buf, 8, &abits) < 0)
    err("pgaccess with len=8 failed");

  unsigned int expected = (1U << 0) | (1U << 7);
  if (abits != expected) {
    printf("Expected: 0x%x, got: 0x%x\n", expected, abits);
    err("small len mismatch");
  }

  free(buf);
  printf("[PASS] test_small_len: OK (scanned len=8 correctly)\n");
}

int
main(int argc, char *argv[])
{
  printf("=== Starting pgtbltest (Exercise 3: pgaccess) ===\n");
  pgaccess_test();
  test_clear_bit();
  test_read_and_write();
  test_error_handling();
  test_small_len();
  printf("=== ALL PGTBLTEST TESTS PASSED! ===\n");
  exit(0);
}
