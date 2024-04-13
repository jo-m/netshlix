#pragma once

#include <stdint.h>

// Wraps MakeTables from RFC2445 Appendix A.
// https://datatracker.ietf.org/doc/html/rfc2435
void rfc2435_make_tables(int q, uint8_t *lqt, uint8_t *cqt);

// Wraps MakeHeaders from RFC2445 Appendix B.
// https://datatracker.ietf.org/doc/html/rfc2435
int rfc2435_make_headers(uint8_t *p, int type, int w, int h, uint8_t *lqt, uint8_t *cqt,
                         uint16_t dri);
