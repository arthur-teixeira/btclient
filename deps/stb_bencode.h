#ifndef PARSER_H
#define PARSER_H

#include "stb_hashtable.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum BencodeKind {
  BYTESTRING,
  INTEGER,
  LIST,
  DICTIONARY,
  ERROR,
} BencodeKind;

typedef struct BencodeList {
  size_t len;
  size_t cap;
  struct BencodeType *values;
} BencodeList;

typedef struct BencodeString {
  size_t len;
  char *str;
} BencodeString;

typedef struct BencodeType {
  BencodeKind kind;
  unsigned char sha1_digest[20];
  union {
    BencodeString asString;
    long asInt;
    BencodeList asList;
    hash_table_t asDict;
  };
} BencodeType;

typedef enum {
  LIST_START,
  DICT_START,
  INT_START,
  INT,
  END,
  STRING_SIZE,
  STRING,
  COLON,
  END_OF_FILE,
  ILLEGAL,
} TokenType;

typedef struct {
  TokenType type;
  size_t pos;
  union {
    char *asString;
    long asInt;
  };
} Token;

typedef struct {
  FILE *input;
  char *buf;
  size_t bufsize;
  size_t pos;
  size_t read_pos;
  char ch;
  Token prevprev;
  Token prev;
} Lexer;

typedef struct {
  Lexer l;
  Token cur_token;
  Token peek_token;
  char *errors[500];
  size_t error_index;
} Parser;

void open_stream(Lexer *l, const char *filename);
Token next_token(Lexer *l);
BencodeType parse_item(Parser *p);
BencodeList parse(Parser *p);
void parser_next_token(Parser *p);
Parser new_parser(Lexer l);
bool expect_peek(Parser *p, TokenType expected);
void parse_error(Parser *p, char *error);
Lexer new_lexer(char *filename);

#endif // PARSER_H

#ifdef BENCODE_IMPLEMENTATION
#undef BENCODE_IMPLEMENTATION

#ifndef BENCODE_GET_SHA1
#define BENCODE_GET_SHA1(a, b, c) ""
#endif

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define HASH_TABLE_IMPLEMENTATION
#include "stb_hashtable.h"

#define da_init(da, size)                                                      \
  do {                                                                         \
    da->cap = 16;                                                              \
    da->values = calloc(da->cap, size);                                        \
    da->len = 0;                                                               \
  } while (0);

#define da_append(da, value)                                                   \
  do {                                                                         \
    if (da->len == da->cap) {                                                  \
      da->cap *= 2;                                                            \
      da->values = realloc(da->values, da->cap * sizeof(da->values[0]));       \
    }                                                                          \
    da->values[da->len++] = value;                                             \
  } while (0);

BencodeType parse_integer(Parser *p) {
  BencodeType b;
  if (!expect_peek(p, INT)) {
    parse_error(p, "Integer initializer is not followed by an integer value");
  }

  b.kind = INTEGER;
  b.asInt = p->cur_token.asInt;

  if (!expect_peek(p, END)) {
    parse_error(p, "Unterminated int value");
  }

  return b;
}

BencodeType parse_bytestring(Parser *p) {
  assert(p->cur_token.type == STRING_SIZE);
  BencodeType s = {0};

  BencodeString str = {
      .len = p->cur_token.asInt,
  };

  if (!expect_peek(p, COLON)) {
    return s;
  }

  if (!expect_peek(p, STRING)) {
    parse_error(p, "ERROR: A colon should be followed by a string\n");
    return s;
  }

  str.str = p->cur_token.asString;
  s.asString = str;

  return s;
}

BencodeType parse_list(Parser *p) {
  BencodeType l;
  l.kind = LIST;

  BencodeList *lp = &l.asList;
  da_init(lp, sizeof(BencodeType));

  parser_next_token(p);

  while (p->cur_token.type != END) {
    da_append(lp, parse_item(p));
    parser_next_token(p);
  }

  return l;
}

BencodeType parse_dict(Parser *p) {
  BencodeType d;
  d.kind = DICTIONARY;
  hash_table_init(&d.asDict);

  parser_next_token(p);
  while (p->cur_token.type != END) {
    BencodeType key = parse_item(p);
    if (key.kind != BYTESTRING) {
      parse_error(p, "Dictionary key is not a string\n");
      return d;
    }

#ifdef BENCODE_HASH_INFO_DICT
    bool parsing_info_dict = strcmp(key.asString.str, "info") == 0;
    size_t start_pos = 0;
    if (parsing_info_dict) {
      assert(p->peek_token.type == DICT_START);
      start_pos = p->peek_token.pos;
    }
#endif

    parser_next_token(p);
    BencodeType value = parse_item(p);
    BencodeType *heap_value = malloc(sizeof(BencodeType));
#ifdef BENCODE_HASH_INFO_DICT
    if (parsing_info_dict) {
      assert(p->cur_token.type == END);
      unsigned char *digest = BENCODE_GET_SHA1(p->l.buf, start_pos, p->cur_token.pos);
      memcpy(value.sha1_digest, digest, 20);
    }
#endif

    *heap_value = value;
    hash_table_insert(&d.asDict, key.asString.str, strlen(key.asString.str),
                      heap_value);

    parser_next_token(p);
  }

  return d;
}

BencodeType parse_item(Parser *p) {
  switch (p->cur_token.type) {
  case INT_START:
    return parse_integer(p);
  case LIST_START:
    return parse_list(p);
    break;
  case DICT_START:
    return parse_dict(p);
    break;
  case STRING_SIZE:
    return parse_bytestring(p);
  default:
    parse_error(p, "unexpected token");
    break;
  }

  BencodeType e;
  e.kind = ERROR;

  return e;
}

BencodeList parse(Parser *p) {
  BencodeList l = {0};
  BencodeList *lp = &l;

  da_init(lp, sizeof(BencodeType));

  while (p->cur_token.type != END_OF_FILE) {
    da_append(lp, parse_item(p));
    parser_next_token(p);
  }

  return l;
}

void open_stream(Lexer *l, const char *filename) {
  FILE *f = fopen(filename, "r");

  if (!f) {
    perror("ERROR: could not open file");
    exit(EXIT_FAILURE);
  }

  fread(l->buf, l->bufsize, sizeof(char), f);
  l->input = f;
}

Lexer new_lexer(char *filename) {
  Lexer l;
  l.pos = 0;
  l.read_pos = 0;
  l.bufsize = 10000;
  l.buf = calloc(l.bufsize, sizeof(char));
  open_stream(&l, filename);
  return l;
}

void free_lexer(Lexer *l) { free(l->buf); }

void read_char(Lexer *l) {
  if (l->read_pos >= l->bufsize) {
    if (l->input && !feof(l->input)) {
      memset(l->buf, 0, l->bufsize);
      fread(l->buf, l->bufsize, sizeof(char), l->input);
      l->read_pos = 0;
      l->pos = 0;

      l->ch = l->buf[l->read_pos];
    } else {
      l->ch = 0;
    }
  } else {
    l->ch = l->buf[l->read_pos];
  }

  l->pos = l->read_pos;
  l->read_pos++;
}

char peek_char(Lexer *l) {
  if (l->read_pos >= l->bufsize) {
    if (l->input && !feof(l->input)) {
      char c = fgetc(l->input);
      ungetc(c, l->input);
      return c;
    }

    return '\0';
  }

  return l->buf[l->read_pos];
}

Token next_token(Lexer *l) {
  Token t = {0};

  read_char(l);

  switch (l->ch) {
  case ':':
    t.type = COLON;
    break;
  case 'd':
    if (l->prev.type != COLON) {
      t.type = DICT_START;
      t.pos = l->pos;
      break;
    }
  case 'l':
    if (l->prev.type != COLON) {
      t.type = LIST_START;
      break;
    }
  case 'i':
    if (l->prev.type != COLON) {
      t.type = INT_START;
      break;
    }
  case 'e':
    if (l->prev.type != COLON) {
      t.type = END;
      t.pos = l->pos;
      break;
    }
  default:
    if (isdigit(l->ch) || l->ch == '-') {
      char buf[500];
      memset(buf, 0, sizeof(buf));
      size_t i = 0;
      while (true) {
        buf[i] = l->ch;
        if (!isdigit(buf[i]) && l->ch != '-') {
          t.type = ILLEGAL;
          return t;
        }

        if (peek_char(l) == 'e' || peek_char(l) == ':') {
          break;
        }

        i++;
        read_char(l);
      }

      if (l->prev.type == INT_START) {
        t.type = INT;
      } else if (peek_char(l) == ':') {
        t.type = STRING_SIZE;
      } else {
        t.type = ILLEGAL;
      }

      t.asInt = strtol(buf, NULL, 10);
    } else if (l->prevprev.type == STRING_SIZE) {
      t.type = STRING;
      size_t n = l->prevprev.asInt;
      t.asString = calloc(n, sizeof(char));

      size_t i = 0;
      for (; i < n - 1; i++, read_char(l)) {
        t.asString[i] = l->ch;
      }

      t.asString[i] = l->ch;
    } else if (!l->input && (strcmp("\n", &l->ch) || strcmp("\0", &l->ch))) {
      t.type = END_OF_FILE;
    } else if (l->input && feof(l->input)) {
      t.type = END_OF_FILE;
    }
  }

  l->prevprev = l->prev;
  l->prev = t;

  return t;
}

void parser_next_token(Parser *p) {
  p->cur_token = p->peek_token;
  p->peek_token = next_token(&p->l);
}

bool expect_peek(Parser *p, TokenType expected) {
  if (p->peek_token.type != expected) {
    char err[100];
    sprintf(err, "ERROR: expected token %i", expected);
    parse_error(p, err);
    return false;
  }

  parser_next_token(p);
  return true;
}

void parse_error(Parser *p, char *error) {
  p->errors[p->error_index++] = strdup(error);
}

Parser new_parser(Lexer l) {
  Parser p = {
      .l = l,
  };
  p.cur_token = next_token(&p.l);
  p.peek_token = next_token(&p.l);

  return p;
}

#endif // BENCODE_IMPLEMENTATION
