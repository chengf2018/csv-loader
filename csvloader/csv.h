#include <stdio.h>

/* value type */
#define TYPE_NIL     0
#define TYPE_INT     1
#define TYPE_DOUBLE  2
#define TYPE_STRING  3

#define MIN_BUFFER_SIZE 32
#define MIN_VALUEVEC_SIZE 32

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct mbuffer {
	char *buff;
	size_t n;
	size_t size;
}mbuffer;

typedef struct csv_value {
	int type;
	union {
		long long intvalue;
		double doublevalue;
		char* stringvalue;
	};
}csv_value;

typedef struct csv_valuevec
{
	struct csv_value *values;
	size_t n;//count for values
	size_t size;//size for values
}csv_valuevec;

typedef struct csv_sheet {
	struct csv_context *context;//temp value
	struct csv_valuevec valuevec;
	size_t rownum;//count amount of effective line
	size_t colnum;//count amount of column for first line
}csv_sheet;

typedef struct csv_loadfile {
	FILE *fp;
	char readbuff[BUFSIZ];//BUFFSIZ:read file size every time,readbuff using for read file
	size_t n;
	const char* p;//current char point
	size_t curline;//current real line in the file
}csv_loadfile;

typedef struct csv_context {
	struct mbuffer *buffer;
	struct csv_loadfile *loadf;
}csv_context;

typedef struct csv_line {
	struct csv_valuevec valuevec;
}csv_line;

typedef struct csv_parse
{
	struct mbuffer buffer;
	struct csv_loadfile loadf;

	size_t rownum;//count amount of effective line
	size_t colnum;//count amount of column for first line
}csv_parse;

/*error code begin*/
#define Csv_Success 0
#define Csv_ParamError 1
#define Csv_OpenFileError 2
#define Csv_VoidFile 3

/*error code end*/

/*csv function begin*/

/* Load csv way 1 :Csv_Load -> Csv_GetValue -> Csv_Clear
Param [in] filename
Param [out] sheet
returns: errcode, 0 is success, other is error
*/
extern int Csv_Load(const char* filename, csv_sheet *sheet);
extern csv_value* Csv_GetValue(csv_sheet *sheet, size_t row, size_t col);
extern void Csv_Clear(csv_sheet* sheet);

/* Load csv way 2 :Csv_Open -> Csv_ParseOneLine -> Csv_Close
Param [in] filename
Param [out] parse
returns: errcode, 0 is success, other is error
*/
extern int Csv_Open(const char* filename, csv_parse *parse);
extern void Csv_Close(csv_parse *parse);
/*return -1 is EOF, -2 is parse error, return 0 is success*/
extern int Csv_ParseOneLine(csv_parse *parse, csv_line *line);

/* Auxiliary function for way 2 */
extern void Csv_InitLine(csv_line *line);
extern void Csv_ClearLine(csv_line *line);
extern void Csv_FreeLine(csv_line *line);
extern csv_value* Csv_GetLineValue(csv_line *line, size_t index);

extern size_t Csv_TotalMem();
extern void Csv_PrintValue(csv_value *value);
/*csv function end*/

#ifdef __cplusplus
}
#endif