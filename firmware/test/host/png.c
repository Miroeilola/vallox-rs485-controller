// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
#include "png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t s_crc_table[256];
static void crc_init(void)
{
    if (s_crc_table[1]) return;
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        s_crc_table[n] = c;
    }
}
static uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n)
{
    crc_init();
    crc ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) crc = s_crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
static uint32_t adler32(const uint8_t *p, size_t n)
{
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; i++) { a = (a + p[i]) % 65521u; b = (b + a) % 65521u; }
    return (b << 16) | a;
}
static void be32(uint8_t *o, uint32_t v) { o[0] = (uint8_t)(v >> 24); o[1] = (uint8_t)(v >> 16); o[2] = (uint8_t)(v >> 8); o[3] = (uint8_t)v; }
static uint32_t rd32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

static bool write_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
    uint8_t hdr[8]; be32(hdr, len); memcpy(hdr + 4, type, 4);
    uint32_t crc = crc32_update(0, hdr + 4, 4);
    if (len) crc = crc32_update(crc, data, len);
    uint8_t tail[4]; be32(tail, crc);
    return fwrite(hdr, 1, 8, f) == 8 && (len == 0 || fwrite(data, 1, len, f) == len) && fwrite(tail, 1, 4, f) == 4;
}

bool png_write_rgb565(const char *path, const uint16_t *fb, int w, int h)
{
    size_t row = (size_t)w * 3 + 1, raw_len = row * (size_t)h;
    uint8_t *raw = malloc(raw_len);
    if (!raw) return false;
    for (int y = 0; y < h; y++) {
        uint8_t *r = raw + (size_t)y * row;
        r[0] = 0;                                   // filter: none
        for (int x = 0; x < w; x++) {
            uint16_t p = fb[y * w + x];
            uint8_t R = (uint8_t)((p >> 11) & 31), G = (uint8_t)((p >> 5) & 63), B = (uint8_t)(p & 31);
            r[1 + x * 3] = (uint8_t)((R << 3) | (R >> 2));
            r[2 + x * 3] = (uint8_t)((G << 2) | (G >> 4));
            r[3 + x * 3] = (uint8_t)((B << 3) | (B >> 2));
        }
    }
    // zlib: header, stored blocks of ≤ 65535 bytes, adler32
    size_t nblocks = (raw_len + 65534) / 65535;
    size_t z_len = 2 + raw_len + nblocks * 5 + 4;
    uint8_t *z = malloc(z_len);
    if (!z) { free(raw); return false; }
    size_t o = 0; z[o++] = 0x78; z[o++] = 0x01;
    size_t done = 0;
    while (done < raw_len) {
        size_t n = raw_len - done; if (n > 65535) n = 65535;
        z[o++] = (done + n == raw_len) ? 1 : 0;    // BFINAL
        z[o++] = (uint8_t)(n & 0xFF); z[o++] = (uint8_t)(n >> 8);
        z[o++] = (uint8_t)(~n & 0xFF); z[o++] = (uint8_t)((~n >> 8) & 0xFF);
        memcpy(z + o, raw + done, n); o += n; done += n;
    }
    be32(z + o, adler32(raw, raw_len)); o += 4;
    FILE *f = fopen(path, "wb");
    bool ok = f != NULL;
    if (ok) {
        static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        uint8_t ihdr[13]; be32(ihdr, (uint32_t)w); be32(ihdr + 4, (uint32_t)h);
        ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;   // 8-bit RGB
        ok = fwrite(sig, 1, 8, f) == 8 && write_chunk(f, "IHDR", ihdr, 13) &&
             write_chunk(f, "IDAT", z, (uint32_t)o) && write_chunk(f, "IEND", NULL, 0);
        fclose(f);
    }
    free(z); free(raw);
    return ok;
}

bool png_read_rgb565(const char *path, uint16_t *fb, int w, int h)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 33) { fclose(f); return false; }
    uint8_t *buf = malloc((size_t)sz);
    bool ok = buf && fread(buf, 1, (size_t)sz, f) == (size_t)sz;
    fclose(f);
    if (!ok) { free(buf); return false; }
    size_t pos = 8, idat_cap = (size_t)sz, idat_len = 0;
    uint8_t *idat = malloc(idat_cap);
    ok = idat != NULL && memcmp(buf, "\x89PNG\r\n\x1a\n", 8) == 0;
    int pw = 0, ph = 0;
    while (ok && pos + 12 <= (size_t)sz) {
        uint32_t len = rd32(buf + pos); const uint8_t *type = buf + pos + 4; const uint8_t *data = buf + pos + 8;
        if (pos + 12 + len > (size_t)sz) { ok = false; break; }
        if (memcmp(type, "IHDR", 4) == 0) { pw = (int)rd32(data); ph = (int)rd32(data + 4); if (data[8] != 8 || data[9] != 2) ok = false; }
        else if (memcmp(type, "IDAT", 4) == 0) { memcpy(idat + idat_len, data, len); idat_len += len; }
        else if (memcmp(type, "IEND", 4) == 0) break;
        pos += 12 + len;
    }
    ok = ok && pw == w && ph == h && idat_len > 6;
    // inflate stored blocks only
    size_t row = (size_t)w * 3 + 1, raw_len = row * (size_t)h;
    uint8_t *raw = ok ? malloc(raw_len) : NULL;
    ok = ok && raw != NULL;
    size_t ip = 2, op = 0;
    while (ok && op < raw_len) {
        if (ip + 5 > idat_len) { ok = false; break; }
        uint8_t hdr = idat[ip++];
        if ((hdr & 0x06) != 0) { ok = false; break; }              // not a stored block
        size_t n = idat[ip] | ((size_t)idat[ip + 1] << 8); ip += 4;
        if (ip + n > idat_len || op + n > raw_len) { ok = false; break; }
        memcpy(raw + op, idat + ip, n); ip += n; op += n;
        if (hdr & 1) break;
    }
    ok = ok && op == raw_len;
    if (ok) {
        for (int y = 0; y < h; y++) {
            const uint8_t *r = raw + (size_t)y * row;
            if (r[0] != 0) { ok = false; break; }
            for (int x = 0; x < w; x++)
                fb[y * w + x] = (uint16_t)(((r[1 + x * 3] >> 3) << 11) | ((r[2 + x * 3] >> 2) << 5) | (r[3 + x * 3] >> 3));
        }
    }
    free(raw); free(idat); free(buf);
    return ok;
}
