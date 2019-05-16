mcc: mcc.c
	gcc -O2 -o mcc.out mcc.c

debug: mcc.c
	gcc -g -Wall -Wextra -Wconversion -o mcc.out mcc.c

clean:
	rm -f mcc.out
