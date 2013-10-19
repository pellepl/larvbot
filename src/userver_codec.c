/*
 * userver_codec.c
 *
 *  Created on: Oct 19, 2013
 *      Author: petera
 */


static char from_hex(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'a' && c <= 'z') {
    return c - 'a' + 10;
  } else if (c >= 'A' && c <= 'Z') {
    return c - 'A' + 10;
  } else {
    return -1;
  }
}

static char to_nibble(char n) {
  static const char const __hex[] = "0123456789abcdef";
  return __hex[n & 0xf];
}

char *urlnencode(char *dst, char *str, int num) {
  char *pstr = str;
  char *pdst = dst;
  while (*pstr && pdst < dst+num) {
    if ((*pstr >= '0' && *pstr <= '9') ||
        (*pstr >= 'A' && *pstr <= 'Z') ||
        (*pstr >= 'a' && *pstr <= 'z') ||
        *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') {
      *pdst++ = *pstr;
    } else if (*pstr == ' ') {
      *pdst++ = '+';
    } else {
      if (pdst + 3 >= dst+num) {
        break;
      }
      *pdst++ = '%';
      *pdst++ = to_nibble(*pstr >> 4);
      *pdst++ = to_nibble(*pstr & 15);
    }
    pstr++;
  }
  *pdst = '\0';
  return dst;
}

char *urlndecode(char *dst, char *str, int num) {
  char *pstr = str;
  char *pdst = dst;
  while (*pstr && pdst < dst+num) {
    if (*pstr == '%') {
      if (pstr[1] && pstr[2]) {
        *pdst++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
        pstr += 2;
      }
    } else if (*pstr == '+') {
      *pdst++ = ' ';
    } else {
      *pdst++ = *pstr;
    }
    pstr++;
  }
  *pdst = '\0';
  return dst;
}
