#include "csv.h"
#include <stdio.h>

int main(int argc, char **argv) {
	csv_parse parse;
	int err = Csv_Open("./Test.csv", &parse);
	printf("err:%d\n", err);
	if (err) return 0;

	csv_line firstline, templine;
	Csv_InitLine(&firstline);
	err = Csv_ParseOneLine(&parse, &firstline);
	printf("err:%d, col:%lu\n", err, parse.colnum);
	if (err == -2) {
		return 0;
	}

	int colindex;
	for (colindex=0; colindex<parse.colnum; colindex++) {
		csv_value *value = Csv_GetLineValue(&firstline, colindex);
		Csv_PrintValue(value);
		printf("\n");
	}

	Csv_InitLine(&templine);
	int intc;
	do {
		Csv_ClearLine(&templine);
		intc = Csv_ParseOneLine(&parse, &templine);
		if (intc == 0 || intc == -1) {
			for (colindex=0; colindex<templine.valuevec.n; colindex++) {
				csv_value *value = Csv_GetLineValue(&templine, colindex);
				Csv_PrintValue(value);
				printf(",");
			}
			printf("\n");
		}
	} while(intc == 0);
printf("curline:%lu\n", parse.loadf.curline);
	Csv_FreeLine(&firstline);
	Csv_FreeLine(&templine);
	Csv_Close(&parse);

	//memroy leak check
	printf("total mem:%lu\n", Csv_TotalMem());
	return 0;
}