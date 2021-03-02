
/**
 * `test.c' - b64
 *
 * copyright (c) 2014 joseph werle
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ok/ok.h>

#include "b64.h"

#define S(x) # x
#define t(m, a, b) ({                                                \
    char tmp[1024];                                                  \
    sprintf(tmp, "%s(%s) = %s", S(m), S(a), S(b));                   \
    char *r = (char *) m(a, strlen((char *) a));                     \
    assert(0 == strcmp(b, r));                                       \
    free(r);                                                         \
    ok(tmp);                                                         \
})

void* custom_malloc(size_t size){
  if (size == 0){
    /* On some systems malloc doesn't allow for size = 0 */
    return NULL;
  }
  return malloc(size);
}

void* custom_realloc(void* ptr, size_t size){
  return realloc(ptr, size);
}

int
main (void) {

  // encode
  {
    t(b64_encode, (const unsigned char *) "bradley", "YnJhZGxleQ==");
    t(b64_encode, (const unsigned char *) "kinkajou", "a2lua2Fqb3U=");
    t(b64_encode, (const unsigned char *) "vino", "dmlubw==");
    t(b64_encode,
        (const unsigned char *) "brian the monkey and bradley the kinkajou are friends",
        "YnJpYW4gdGhlIG1vbmtleSBhbmQgYnJhZGxleSB0aGUga2lua2Fqb3UgYXJlIGZyaWVuZHM=");
  }

  // decode
  {
    t(b64_decode, "Y2FzaWxsZXJv", "casillero");
    t(b64_decode, "aGF4", "hax");
    t(b64_decode, "bW9ua2V5cyBhbmQgZG9ncw==", "monkeys and dogs");
    t(b64_decode,
        "dGhlIGtpbmtham91IGFuZCBtb25rZXkgZm91Z2h0IG92ZXIgdGhlIGJhbmFuYQ==",
        "the kinkajou and monkey fought over the banana");
  }

  ok_done();
  return 0;
}
