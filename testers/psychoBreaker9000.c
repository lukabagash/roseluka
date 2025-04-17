#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

void main() {
	char input[40];
	char reversed[40];
	int status;
	int i;

	print(WRITETERMINAL, "Reverse String Test starts\n");
	print(WRITETERMINAL, "Enter a string: ");

	status = SYSCALL(READTERMINAL, (int)&input[0], 0, 0);
	input[status] = EOS;

	print(WRITETERMINAL, "\nOriginal string: ");
	print(WRITETERMINAL, input);

	for (i = 0; i < status - 1; i++) {
		reversed[i] = input[status - 2 - i]; // -2 to skip newline and start from last char
	}
	reversed[status - 1] = EOS;

	print(WRITETERMINAL, "\nReversed string: ");
	print(WRITETERMINAL, reversed);
	print(WRITETERMINAL, "\n\nReverse concluded\n");

	SYSCALL(TERMINATE, 0, 0, 0);
}
