// T0 gate: prove vendored librope (the §5.11 text-buffer backing) compiles and
// links inside the termacs-core build, with an offset-based UTF-8 edit round-trip.
#include <rope.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

int main() {
    rope* r = rope_new();
    rope_insert(r, 0, (const uint8_t*)"Hello world");
    rope_insert(r, 5, (const uint8_t*)" cruel");      // "Hello cruel world"
    rope_del(r, 5, 6);                                  // back to "Hello world"
    assert(rope_char_count(r) == 11);
    uint8_t* s = rope_create_cstr(r);
    assert(std::strcmp((const char*)s, "Hello world") == 0);
    free(s);
    rope_free(r);

    // UTF-8: positions/counts are in codepoints, not bytes
    rope* u = rope_new_with_utf8((const uint8_t*)"héllo");
    assert(rope_char_count(u) == 5);   // 5 codepoints
    assert(rope_byte_count(u) == 6);   // é = 2 bytes
    rope_free(u);

    std::cout << "librope smoke OK\n";
    return 0;
}
