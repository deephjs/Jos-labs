

// Stripped-down primitive printf-style formatting routines,
// used in common by printf, sprintf, fprintf, etc.
// This code is also used by both the kernel and user programs.

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/stdarg.h>
#include <inc/error.h>



/*
 * Space or zero padding and a field width are supported for the numeric
 * formats only. 
 * 
 * The special format %e takes an integer error code
 * and prints a string describing the error.
 * The integer may be positive or negative,
 * so that -E_NO_MEM and E_NO_MEM are equivalent.
 */

static const char * const error_string[MAXERROR] =
{
	[E_UNSPECIFIED]	= "unspecified error",
	[E_BAD_ENV]	= "bad environment",
	[E_INVAL]	= "invalid parameter",
	[E_NO_MEM]	= "out of memory",
	[E_NO_FREE_ENV]	= "out of environments",
	[E_FAULT]	= "segmentation fault",
	[E_IPC_NOT_RECV]= "env is not recving",
	[E_EOF]		= "unexpected end of file",
};





/*
 * Print a number (base <= 16) in reverse order,
 * using specified putch function and associated pointer putdat.
 */

static int
printnumHelper(void (*putch)(int, void*, char*, int*), void *putdat,
	 unsigned long long num, unsigned base, int width, int padc,
	 char* nNum, int* nOverflow)
{
	if (num >= base) {
		width = printnumHelper(putch, putdat, num / base, base, width - 1, padc, nNum, nOverflow);
	} else if(padc != '-'){
		// padding at left
		while (--width > 0){
			putch(padc, putdat, nNum, nOverflow);
		}
	}

	// then print this (the least significant) digit
	putch("0123456789abcdef"[num % base], putdat, nNum, nOverflow);
	return width;
}



static void
printnum(void (*putch)(int, void*, char*, int*), void *putdat,
	 unsigned long long num, unsigned base, int width, int padc,
	 char* nNum, int* nOverflow)
{
	// if cprintf'parameter includes pattern of the form "%-", padding
	// space on the right side if neccesary.
	// you can add helper function if needed.
	// your code here:
	// first recursively print all preceding (more significant) digits
	if (num >= base) {
		width = printnumHelper(putch, putdat, num / base, base, width - 1, padc, nNum, nOverflow);
	}else if(padc != '-'){
		//padding at left, case for one character
		while(--width > 0){
			putch(padc, putdat, nNum, nOverflow);
		}
	}
	putch("0123456789abcdef"[num % base], putdat, nNum, nOverflow);

	//padding at right, padc is certainly '-', and it should be ' ' instead
	while (--width > 0){
		putch(' ', putdat, nNum, nOverflow);
	}
}

// Get an unsigned int of various possible sizes from a varargs list,
// depending on the lflag parameter.
static unsigned long long
getuint(va_list *ap, int lflag)
{
	if (lflag >= 2)
		return va_arg(*ap, unsigned long long);
	else if (lflag)
		return va_arg(*ap, unsigned long);
	else
		return va_arg(*ap, unsigned int);
}

// Same as getuint but signed - can't use getuint
// because of sign extension
static long long
getint(va_list *ap, int lflag)
{
	if (lflag >= 2)
		return va_arg(*ap, long long);
	else if (lflag)
		return va_arg(*ap, long);
	else
		return va_arg(*ap, int);
}


// Main function to format and print a string.
void printfmt(void (*putch)(int, void*, char*, int*), void *putdat, const char *fmt, ...);

void
vprintfmt(void (*putch)(int, void*, char*, int*), void *putdat, const char *fmt, va_list ap)
{
	register const char *p;
	register int ch, err;
	unsigned long long num;
	int base, lflag, width, precision, altflag, signflag;
	char padc;
	char nNum = (char)0;
	int nOverflow = 0;

	while (1) {
		while ((ch = *(unsigned char *) fmt++) != '%') {
			if (ch == '\0')
				return;
			putch(ch, putdat, &nNum, &nOverflow);
		}

		// Process a %-escape sequence
		padc = ' ';
		width = -1;
		precision = -1;
		lflag = 0;
		altflag = 0;
		signflag = 0;
	reswitch:
		switch (ch = *(unsigned char *) fmt++) {

		// flag to pad on the right
		case '-':
			padc = '-';
			goto reswitch;
			
		//flag to force showing sign
		case '+':
			signflag = 1;
			goto reswitch;

		// flag to pad with 0's instead of spaces
		case '0':
			padc = '0';
			goto reswitch;

		// width field
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			for (precision = 0; ; ++fmt) {
				precision = precision * 10 + ch - '0';
				ch = *fmt;
				if (ch < '0' || ch > '9')
					break;
			}
			goto process_precision;

		case '*':
			precision = va_arg(ap, int);
			goto process_precision;

		case '.':
			if (width < 0)
				width = 0;
			goto reswitch;

		case '#':
			altflag = 1;
			goto reswitch;

		process_precision:
			if (width < 0)
				width = precision, precision = -1;
			goto reswitch;

		// long flag (doubled for long long)
		case 'l':
			lflag++;
			goto reswitch;

		// character
		case 'c':
			putch(va_arg(ap, int), putdat, &nNum, &nOverflow);
			break;

		// error message
		case 'e':
			err = va_arg(ap, int);
			if (err < 0)
				err = -err;
			if (err >= MAXERROR || (p = error_string[err]) == NULL)
				printfmt(putch, putdat, "error %d", err);
			else
				printfmt(putch, putdat, "%s", p);
			break;

		// string
		case 's':
			if ((p = va_arg(ap, char *)) == NULL)
				p = "(null)";
			if (width > 0 && padc != '-')
				for (width -= strnlen(p, precision); width > 0; width--)
					putch(padc, putdat, &nNum, &nOverflow);
			for (; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0); width--)
				if (altflag && (ch < ' ' || ch > '~'))
					putch('?', putdat, &nNum, &nOverflow);
				else
					putch(ch, putdat, &nNum, &nOverflow);
			for (; width > 0; width--)
				putch(' ', putdat, &nNum, &nOverflow);
			break;

		// (signed) decimal
		case 'd':
			num = getint(&ap, lflag);
			if ((long long) num < 0) {
				putch('-', putdat, &nNum, &nOverflow);
				num = -(long long) num;
			}else if(signflag == 1){
				putch('+', putdat, &nNum, &nOverflow);
			}
			base = 10;
			goto number;

		// unsigned decimal
		case 'u':
			num = getuint(&ap, lflag);
			base = 10;
			goto number;

		// (unsigned) octal
		case 'o':
			// display a number in octal form and the form should begin with '0'
			num = getint(&ap, lflag);
                        if ((long long) num < 0) {
                                putch('-', putdat, &nNum, &nOverflow);
                                num = -(long long) num;
                        }else if(signflag == 1){
                                putch('+', putdat, &nNum, &nOverflow);
                        }	
			base = 8;
			putch('0', putdat, &nNum, &nOverflow);
			goto number;

		// pointer
		case 'p':
			putch('0', putdat, &nNum, &nOverflow);
			putch('x', putdat, &nNum, &nOverflow);
			num = (unsigned long long)
				(uintptr_t) va_arg(ap, void *);
			base = 16;
			goto number;

		// (unsigned) hexadecimal
		case 'x':
			num = getuint(&ap, lflag);
			base = 16;
		number:
			printnum(putch, putdat, num, base, width, padc, &nNum, &nOverflow);
			break;

        case 'n': {
            // You can consult the %n specifier specification of the C99 printf function
            // for your reference by typing "man 3 printf" on the console. 

            // 
            // Requirements:
            // Nothing printed. The argument must be a pointer to a signed char, 
            // where the number of characters written so far is stored.
            //

            // hint:  use the following strings to display the error messages 
            //        when the cprintf function ecounters the specific cases,
            //        for example, when the argument pointer is NULL
            //        or when the number of characters written so far 
            //        is beyond the range of the integers the signed char type 
            //        can represent.

            const char *null_error = "\nerror! writing through NULL pointer! (%n argument)\n";
            const char *overflow_error = "\nwarning! The value %n argument pointed to has been overflowed!\n";

            // Your code here
            char *p = va_arg(ap, char*);
	    if(p == NULL){
		int i = 0;
		while(1){
		    if(null_error[i] != '\0'){
			putch(null_error[i], putdat, &nNum, &nOverflow);
			i++;
		    }else{
			break;
		    }
		}
	    }else if(nOverflow == 1){
		int i = 0;
		*p = nNum;
                while(1){
                    if(overflow_error[i] != '\0'){
                        putch(overflow_error[i], putdat, &nNum, &nOverflow);
                        i++;
                    }else{
                        break;
                    }
                }
	    }else{
		*p = nNum;
	    }
            break;
        }

		// escaped '%' character
		case '%':
			putch(ch, putdat, &nNum, &nOverflow);
			break;
			
		// unrecognized escape sequence - just print it literally
		default:
			putch('%', putdat, &nNum, &nOverflow);
			for (fmt--; fmt[-1] != '%'; fmt--)
				/* do nothing */;
			break;
		}
	}
}

void
printfmt(void (*putch)(int, void*, char*, int*), void *putdat, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintfmt(putch, putdat, fmt, ap);
	va_end(ap);
}

struct sprintbuf {
	char *buf;
	char *ebuf;
	int cnt;
};

static void
sprintputch(int ch, struct sprintbuf *b)
{
	b->cnt++;
	if (b->buf < b->ebuf)
		*b->buf++ = ch;
}

int
vsnprintf(char *buf, int n, const char *fmt, va_list ap)
{
	struct sprintbuf b = {buf, buf+n-1, 0};

	if (buf == NULL || n < 1)
		return -E_INVAL;

	// print the string to the buffer
	vprintfmt((void*)sprintputch, &b, fmt, ap);

	// null terminate the buffer
	*b.buf = '\0';

	return b.cnt;
}

int
snprintf(char *buf, int n, const char *fmt, ...)
{
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = vsnprintf(buf, n, fmt, ap);
	va_end(ap);

	return rc;
}


