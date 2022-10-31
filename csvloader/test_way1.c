#include "csv.h"
#include <stdio.h>

int main(int argc, char **argv) {
	csv_sheet sheet;
	int err = Csv_Load("./Test.csv", &sheet);
	printf("err:%d\n", err);
	if (err) return 0;

	size_t rowindex, colindex;
	for (rowindex = 0; rowindex < sheet.rownum; rowindex++) {
		for (colindex = 0; colindex < sheet.colnum; colindex++) {
			csv_value *value = Csv_GetValue(&sheet, rowindex, colindex);
			Csv_PrintValue(value);
			printf(",");
		}
		printf("\n");
	}

	Csv_Clear(&sheet);
	//memroy leak check
	printf("total mem:%lu\n", Csv_TotalMem());
	return 0;
}