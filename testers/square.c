#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

void main() {
	int status;
	char buf[10];
	int num = 0;
	int result;
	int i = 0;

	print(WRITETERMINAL, "Square Calculator - Enter an integer (0-9): ");

	status = SYSCALL(READTERMINAL, (int)&buf[0], 0, 0);
	buf[status] = EOS;

	num = buf[0] - '0';
	result = num * num;

	print(WRITETERMINAL, "\nSquare of ");
	print(WRITETERMINAL, &buf[0]);
	print(WRITETERMINAL, " is: ");

	char resultStr[10];
	resultStr[0] = (result / 10) + '0';
	resultStr[1] = (result % 10) + '0';
	resultStr[2] = EOS;

	if (result >= 10)
		print(WRITETERMINAL, &resultStr[0]);
	else
		print(WRITETERMINAL, &resultStr[1]);

	print(WRITETERMINAL, "\n\nSquare Test Concluded\n");

	/* Terminate normally */	
	SYSCALL(TERMINATE, 0, 0, 0);
}
