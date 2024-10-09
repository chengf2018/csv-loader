#include "csv.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define CSVEOF -1
#define PARSEERR -2


static size_t totalmem = 0;


void* mymalloc(size_t size) {
	totalmem += size;
	return malloc(size); 
}


void myfree(void *mem, size_t size) {
	totalmem -= size;
	free(mem); 
}


void* myrealloc(void *mem, size_t old, size_t new) {
	totalmem += new;
	totalmem -= old;
	return realloc(mem, new);
}


static char *copystring(char *str) {
	size_t len = strlen(str);
	char *nstr = (char*)mymalloc(len+1);
	memcpy(nstr, str, len);
	nstr[len] = '\0';
	return nstr;
}


/*mbuffer function begin*/
#define char2int(c) ((int)(unsigned char)(c))
#define int2char(i) ((char)(i))
#define mbuffer_init(b) ((b)->buff = NULL, (b)->n = 0, (b)->size = 0)
#define mbuffer_reset(b) ((b)->n = 0)
#define mbuffer_size(b) ((b)->size)
#define mbuffer_len(b) ((b)->n)
#define mbuffer_resize(b, s) ((b)->buff = myrealloc((b)->buff, (b)->size, (s)), \
			(b)->size = s)
#define mbuffer_free(b) (((b)->buff) ? myfree((b)->buff, (b)->size) : 0)
void mbuffer_save(mbuffer *buffer, int c) {
	if (buffer->n + 1 > buffer->size) {
		size_t newsize = buffer->size ? (buffer->size * 2) : MIN_BUFFER_SIZE;
		mbuffer_resize(buffer, newsize);
	}
	buffer->buff[buffer->n++] = int2char(c);
}
/*mbuffer function end*/


static void reservevaluevec(csv_valuevec *valuevec) {
	if (!valuevec->values) {
		csv_value *values = mymalloc(sizeof(csv_value) * MIN_VALUEVEC_SIZE);
		memset(values, 0, sizeof(csv_value) * MIN_VALUEVEC_SIZE);

		valuevec->values = values;
		valuevec->n = 0;
		valuevec->size = MIN_VALUEVEC_SIZE;
	}

	if (valuevec->n + 1 > valuevec->size) {
		size_t oldsize = valuevec->size;
		valuevec->size *= 2;
		csv_value *values = myrealloc(valuevec->values,
									sizeof(csv_value) * oldsize,
									sizeof(csv_value) * valuevec->size);
		memset(values + oldsize, 0, sizeof(csv_value) * oldsize);
		valuevec->values = values;
	}
}

static inline void valuevec_init(csv_valuevec *valuevec) {
	valuevec->values = NULL;
	valuevec->n = valuevec->size = 0;
}

static void valuevec_push(csv_valuevec *valuevec, csv_value *value) {
	reservevaluevec(valuevec);
	valuevec->values[valuevec->n] = *value;
	valuevec->n ++;
}

static inline csv_value *valuevec_get(csv_valuevec *valuevec, size_t index) {
	return &valuevec->values[index];
}

static inline size_t valuevec_count(csv_valuevec *valuevec) {
	return valuevec->n;
}

static inline size_t valuevec_size(csv_valuevec *valuevec) {
	return valuevec->size;
}

static void valuevec_clear(csv_valuevec *valuevec) {
	int i;
	for (i=0; i < valuevec_count(valuevec); i++) {
		csv_value *value = valuevec_get(valuevec, i);
		if (value->type == TYPE_STRING) {
			myfree(value->stringvalue, strlen(value->stringvalue)+1);
		}
		value->type = TYPE_NIL;
	}
	valuevec->n = 0;
}

static void valuevec_free(csv_valuevec *valuevec) {
	int i;
	for (i=0; i < valuevec_count(valuevec); i++) {
		csv_value *value = valuevec_get(valuevec, i);
		if (value->type == TYPE_STRING) {
			myfree(value->stringvalue, strlen(value->stringvalue)+1);
		}
	}
	myfree(valuevec->values, sizeof(csv_value)*valuevec_size(valuevec));
	valuevec->values = NULL;
	valuevec->n = valuevec->size = 0;
}

static void freader(csv_loadfile *lf) {
	size_t rlen = fread(lf->readbuff, 1, sizeof(lf->readbuff), lf->fp);
	lf->n = rlen;
	lf->p = (rlen == 0) ? (NULL) : (lf->readbuff);
}


static int nextc(csv_loadfile *lf) {
	if (lf->n == 0) {
		freader(lf);
		if (lf->n == 0) {
			return CSVEOF;
		}
	}
	
	lf->n--;
	return char2int(*(lf->p++));
}


static int peekc(csv_loadfile *lf) {
	if (lf->n == 0) {
		freader(lf);
		if (lf->n == 0) {
			return CSVEOF;
		}
	}

	return char2int(*(lf->p));
}


static int loadfile_init(const char* filename, csv_loadfile *lf) {
	FILE *fp = fopen(filename, "r");
	if (!fp) return 0;

	lf->fp = fp;
	lf->n = 0;
	lf->p = NULL;
	lf->curline = 0;
	return 1;
}


static void loadfile_free(csv_loadfile *lf) {
	if (lf->fp) fclose(lf->fp);
}


static void context_init(csv_context *context, csv_loadfile *lf, mbuffer *buffer) {
	context->loadf = lf;
	context->buffer = buffer;
}


static int skipblank(csv_loadfile *lf) {
	int c = nextc(lf);
	while (c != CSVEOF && int2char(c) == ' ') c = nextc(lf);
	return c;
}


//skip utf8 bom
static int skipbom(csv_loadfile *lf) {
	const char *p = "\xEF\xBB\xBF";
	if (feof(lf->fp)) return -1;
	do {
		int c = fgetc(lf->fp);
		if (c == CSVEOF || c != *(unsigned char *)p++) {
			fseek(lf->fp, 0, SEEK_SET);
			return 0;
		}
	} while(*p != '\0');
	return 0;
}


static int skipline(csv_loadfile *lf) {
	int c = nextc(lf);
	while (c != CSVEOF && int2char(c) != '\n') c = nextc(lf);
	return c;
}


static int skipnoteline(csv_loadfile *lf) {
	int intc;
	do {
		intc = peekc(lf);
		if (intc <= 0) break;
		unsigned char c = int2char(intc);
		if (c == '#') {
			intc = skipline(lf);
			lf->curline++;
		} else {
			break;
		}
	} while(intc > 0);
	
	return intc;
}


static int judgefieldtype(char *str) {
	int point = 0, type = TYPE_INT;
	if (!str) return TYPE_NIL;
	if (*str == '-') str++;

	if (!(*str >= '0' && *str <= '9')) return TYPE_STRING;
	str++;
	while (*str) {
		if (*str >= '0' && *str <= '9') {
			str++;
		} else if (*str == '.') {
			if (point > 0) return TYPE_STRING;
			if (!(*(str+1) >= '0' && *(str+1) <= '9'))
				return TYPE_STRING;
			point++;
			type = TYPE_DOUBLE;
			str++;
			
		} else if (*str == 'e' || *str == 'E') {
			str++;
			if (*str != '+' && *str != '-') return TYPE_STRING;
			str++;
			while (*str && *str >= '0' && *str <= '9') str++;
			if (*str) return TYPE_STRING;
			return TYPE_DOUBLE;

		} else return TYPE_STRING;
	}

	return type;
}


static int presaveESC(mbuffer *buffer, csv_loadfile *lf) {
	int c = peekc(lf);
	switch (c) {
		case '\\':
			mbuffer_save(buffer, '\\');
			nextc(lf);
		break;
		case 'r':
			mbuffer_save(buffer, '\r');
			nextc(lf);
		break;
		case 'n':
			mbuffer_save(buffer, '\n');
			nextc(lf);
		break;
		case 't':
			mbuffer_save(buffer, '\t');
			nextc(lf);
		break;
		default:
			mbuffer_save(buffer, '\\');
			nextc(lf);
		break;
	}
	return c;
}


static int presavefield(mbuffer *buffer, csv_loadfile *lf) {
	mbuffer_reset(buffer);

	int c = nextc(lf);
	if (c <= 0) return c;
	if (c == '\"') {
		c = nextc(lf);
		while (c > 0) {
			if (c == '\"') {
				if (peekc(lf) == '\"') {
					mbuffer_save(buffer, '\"');
					nextc(lf);
				} else {
					c = skipblank(lf);
					if (c == '\r') c = nextc(lf);
					if (c == ',' || c == '\n' || c == CSVEOF) {
						return c;
					}
					return PARSEERR;
				}
			} else if (c == '\\') {
				presaveESC(buffer, lf);
			} else {
				mbuffer_save(buffer, c);
			}
			c = nextc(lf);
		}
	} else {
		do {
			if (c == '\r') c = nextc(lf);
			if (c == ',' || c == '\n' || c == CSVEOF) {
				return c;
			}
			if (c == '\\') {
				presaveESC(buffer, lf);
			} else {
				mbuffer_save(buffer, c);
			}
			c = nextc(lf);
		} while (c > 0);
	}
	return c;
}


//parse a value, save to csv_value, return a next char
static int parsevalue(mbuffer *buffer, csv_loadfile *lf, csv_value *value) {
	int c = presavefield(buffer, lf);//presave field to buffer
	if (c == PARSEERR) {
		return PARSEERR;
	}
	memset(value, 0, sizeof(*value));

	size_t valuelen = mbuffer_len(buffer);
	if (valuelen > 0) {
		mbuffer_save(buffer, '\0');
		//judge value type
		int type = judgefieldtype(buffer->buff);
		value->type = type;
		if (type == TYPE_STRING) {
			value->stringvalue = copystring(buffer->buff);
		} else if (type == TYPE_INT) {
			value->intvalue = atoll(buffer->buff);
		} else if (type == TYPE_DOUBLE) {
			value->doublevalue = atof(buffer->buff);
		}
	} else {
		value->type = TYPE_NIL;
	}
	return c;
}


//return < 1 is error
static int parseline(csv_sheet *sheet, mbuffer *buffer, csv_loadfile *lf) {
	int intc = peekc(lf);
	if (intc < 1) {
		return intc;
	}
	if (intc == '\r') {
		nextc(lf);
		intc = peekc(lf);
	}
	if (intc == '\n') {
		nextc(lf);
		return intc;
	}
	
	int colnum = 0;
	do {
		csv_value value;
		intc = parsevalue(buffer, lf, &value);
		if (intc == PARSEERR) break;

		valuevec_push(&sheet->valuevec, &value);
		colnum ++;

		//skip excess field
		if (sheet->rownum > 0 && colnum == sheet->colnum) {
			if (intc != '\n' && intc != CSVEOF) {
				intc = skipline(lf);
			}
			break;
		}
	} while (intc == ',');

	if (intc != PARSEERR && colnum > 0) {
		if (sheet->rownum == 0) { //set first colnum of line
			sheet->colnum = colnum;
		} else {
			while (colnum < sheet->colnum) {
				csv_value value = {0};
				value.type = TYPE_NIL;
				valuevec_push(&sheet->valuevec, &value);
				colnum++;
			}
		}
		sheet->rownum++;
	}

	return intc;
}


static int sheetparse(csv_sheet *sheet) {
	csv_context *context = sheet->context;
	csv_loadfile *lf = context->loadf;
	mbuffer *buffer = context->buffer;

	//skip bom
	if (skipbom(lf) < 0) {
		return Csv_VoidFile;
	}

	int intc;
	do {
		intc = skipnoteline(lf);
		if (intc < 1) break;
		intc = parseline(sheet, buffer, lf);
		if (intc < 1) break;

		lf->curline++;
	} while(intc == '\n');

	return intc;
}


int Csv_Load(const char* filename,  csv_sheet *sheet) {
	if (!sheet || !filename) return Csv_ParamError;
	csv_loadfile lf;
	csv_context context;
	mbuffer buffer;
	int succ = loadfile_init(filename, &lf);
	if (!succ) return Csv_OpenFileError;

	mbuffer_init(&buffer);
	context_init(&context, &lf, &buffer);
	valuevec_init(&sheet->valuevec);

	sheet->context = &context;
	sheet->rownum = sheet->colnum = 0;

	int intc = sheetparse(sheet);
	if (intc != CSVEOF) {
		printf("parse error, current line:%lu, content:%s\n", lf.curline, lf.p);
		loadfile_free(&lf);
		mbuffer_free(&buffer);
		Csv_Clear(sheet);
		return PARSEERR;
	}

	loadfile_free(&lf);
	mbuffer_free(&buffer);
	sheet->context = NULL;
	return Csv_Success;
}


csv_value* Csv_GetValue(csv_sheet *sheet, size_t row, size_t col) {
	if (!sheet || sheet->rownum <= row || sheet->colnum <= col)
		return NULL;
	return valuevec_get(&sheet->valuevec, row * sheet->colnum + col);
}


void Csv_Clear(csv_sheet *sheet) {
	if (!sheet) return;
	valuevec_free(&sheet->valuevec);
}

static int parseoneline(csv_parse *parse, csv_line *outline) {
	csv_loadfile *lf = &parse->loadf;
	mbuffer *buffer = &parse->buffer;

	int intc = peekc(lf);
	if (intc < 1) {
		return intc;
	}
	if (intc == '\r') {
		nextc(lf);
		intc = peekc(lf);
	}
	if (intc == '\n') {
		nextc(lf);
		lf->curline++;
		return intc;
	}
	
	int colnum = 0;
	do {
		csv_value value;
		intc = parsevalue(buffer, lf, &value);
		if (intc == PARSEERR) break;

		valuevec_push(&outline->valuevec, &value);
		colnum ++;

		//skip excess field
		if (parse->rownum > 0 && colnum == parse->colnum) {
			if (intc != '\n' && intc != CSVEOF) {
				intc = skipline(lf);
			}
			break;
		}
	} while (intc == ',');

	if (intc != PARSEERR && colnum > 0) {
		if (parse->rownum == 0) { //set first line colnum
			parse->colnum = colnum;
		} else {
			while (colnum < parse->colnum) {
				csv_value value = {0};
				value.type = TYPE_NIL;
				valuevec_push(&outline->valuevec, &value);
				colnum++;
			}
		}
		parse->rownum++;
	}
	lf->curline++;
	return intc;
}

int Csv_Open(const char* filename, csv_parse *parse) {
	if (!filename || !parse) return Csv_ParamError;
	int succ = loadfile_init(filename, &parse->loadf);
	if (!succ) return Csv_OpenFileError;

	//skip bom
	if (skipbom(&parse->loadf) < 0) {
		loadfile_free(&parse->loadf);
		return Csv_VoidFile;
	}
	parse->rownum = parse->colnum = 0;
	mbuffer_init(&parse->buffer);
	return Csv_Success;
}

void Csv_Close(csv_parse *parse) {
	if (!parse) return;
	loadfile_free(&parse->loadf);
	mbuffer_free(&parse->buffer);
}

int Csv_ParseOneLine(csv_parse *parse, csv_line *line) {
	if (!parse || !line) return Csv_ParamError;
	int intc;
	intc = skipnoteline(&parse->loadf);
	if (intc < 1) {
		return intc;
	}
	intc = parseoneline(parse, line);
	if (intc == '\n') {
		return Csv_Success;
	}

	return intc;
}

void Csv_InitLine(csv_line *line) {
	valuevec_init(&line->valuevec);
}

void Csv_ClearLine(csv_line *line) {
	if (!line) return;
	valuevec_clear(&line->valuevec);
}

void Csv_FreeLine(csv_line *line) {
	if (!line) return;
	valuevec_free(&line->valuevec);
}

csv_value* Csv_GetLineValue(csv_line *line, size_t index) {
	if (!line || valuevec_count(&line->valuevec) <= index) return NULL;
	return valuevec_get(&line->valuevec, index);
}

size_t Csv_TotalMem() {
	return totalmem;
}


void Csv_PrintValue(csv_value *value) {
	if (!value) return;
	switch(value->type) {
		case TYPE_NIL:
			printf("nil");
		break;
		case TYPE_INT:
			printf("%lld", value->intvalue);
		break;
		case TYPE_DOUBLE:
			printf("%lf", value->doublevalue);
		break;
		case TYPE_STRING:
			printf("%s", value->stringvalue);
		break;
	}
}