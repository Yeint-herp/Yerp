//
// Created by notbonzo on 1/30/25.
//

#include <util/string.h>
#include <stddef.h>
#include <limits.h>

#ifndef LLONG_MAX
#define LLONG_MAX (sizeof(long long) * CHAR_BIT - 1)
#endif
#ifndef LLONG_MIN
#define LLONG_MIN (-LLONG_MAX - 1)
#endif

void* memcpy( void* dest, const void* src, size_t n ) {
	unsigned char* d = (unsigned char*)( dest );
	const unsigned char* s = (const unsigned char*)( src );
	while ( n-- ) {
		*d++ = *s++;
	}
	return dest;
}

void* memmove( void* dest, const void* src, size_t n ) {
	unsigned char* d = (unsigned char*)( dest );
	const unsigned char* s = (const unsigned char*)( src );
	if ( d < s ) {
		while ( n-- ) {
			*d++ = *s++;
		}
	} else {
		d += n;
		s += n;
		while ( n-- ) {
			*--d = *--s;
		}
	}
	return dest;
}

char* strcpy( char* dest, const char* src ) {
	char* d = dest;
	while ( ( *d++ = *src++ ) != '\0' ) {}
	return dest;
}

char* strncpy( char* dest, const char* src, size_t n ) {
	char* d = dest;
	while ( n && ( *d++ = *src++ ) != '\0' ) {
		--n;
	}
	while ( n-- ) {
		*d++ = '\0';
	}
	return dest;
}

char* strcat( char* dest, const char* src ) {
	char* d = dest;
	while ( *d ) {
		d++;
	}
	while ( ( *d++ = *src++ ) != '\0' ) {}
	return dest;
}

char* strncat( char* dest, const char* src, size_t n ) {
	char* d = dest;
	while ( *d ) {
		d++;
	}
	while ( n-- && ( *d++ = *src++ ) != '\0' ) {}
	if ( n == (size_t)-1 ) {
		*d = '\0';
	}
	return dest;
}

int memcmp( const void* s1, const void* s2, size_t n ) {
	const unsigned char* p1 = (const unsigned char*)( s1 );
	const unsigned char* p2 = (const unsigned char*)( s2 );
	for ( ; n--; ++p1, ++p2 ) {
		if ( *p1 != *p2 ) {
			return *p1 - *p2;
		}
	}
	return 0;
}

int strcmp( const char* s1, const char* s2 ) {
	while ( *s1 && ( *s1 == *s2 ) ) {
		s1++;
		s2++;
	}
	return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp( const char* s1, const char* s2, size_t n ) {
	if ( !n ) return 0;
	while ( n-- && *s1 && ( *s1 == *s2 ) ) {
		if ( n == 0 ) break;
		s1++;
		s2++;
	}
	return *(unsigned char*)s1 - *(unsigned char*)s2;
}

void* memchr( const void* s, int c, size_t n ) {
	const unsigned char* p = (const unsigned char*)( s );
	while ( n-- ) {
		if ( *p == c ) {
			return (void*)p;
		}
		p++;
	}
	return nullptr;
}

char* strchr( const char* s, int c ) {
	while ( *s ) {
		if ( *s == c ) {
			return (char*)s;
		}
		s++;
	}
	return c == 0 ? (char*)s : nullptr;
}

char* strrchr( const char* s, int c ) {
	const char* last = nullptr;
	do {
		if ( *s == c ) {
			last = s;
		}
	} while ( *s++ );
	return (char*)last;
}

size_t strlen( const char* s ) {
	const char* p = s;
	while ( *p ) {
		p++;
	}
	return p - s;
}

size_t strnlen( const char* s, size_t maxlen ) {
	const char* p = s;
	if (maxlen == (size_t)-1) {
		while ( *p ) {
			p++;
		}
		return p - s;
	}
	while ( *p && maxlen-- ) {
		p++;
	}
	return p - s;
}

void* memset( void* s, int c, size_t n ) {
	unsigned char* p = (unsigned char*)( s );
	while ( n-- ) {
		*p++ = c;
	}
	return s;
}

size_t strspn( const char* s, const char* accept ) {
	const char* p = s;
	while ( *p ) {
		const char* a = accept;
		while ( *a && *a != *p ) {
			a++;
		}
		if ( *a == '\0' ) {
			break;
		}
		p++;
	}
	return p - s;
}

size_t strcspn( const char* s, const char* reject ) {
	const char* p = s;
	while ( *p ) {
		const char* r = reject;
		while ( *r ) {
			if ( *p == *r ) {
				return p - s;
			}
			r++;
		}
		p++;
	}
	return p - s;
}

char* strpbrk( const char* s, const char* accept ) {
	while ( *s ) {
		const char* a = accept;
		while ( *a ) {
			if ( *s == *a ) {
				return (char*)s;
			}
			a++;
		}
		s++;
	}
	return nullptr;
}

char* strstr( const char* haystack, const char* needle ) {
	if ( !*needle ) return (char*)haystack;
	for ( ; *haystack; haystack++ ) {
		if ( *haystack != *needle ) continue;
		const char* h = haystack;
		const char* n = needle;
		while ( *h && *n && *h == *n ) {
			h++;
			n++;
		}
		if ( !*n ) return (char*)haystack;
	}
	return nullptr;
}

char* strtok( char* str, const char* delim ) {
	static char* last;
	if ( !str ) str = last;
	if ( !str ) return nullptr;

	str += strspn( str, delim );
	if ( *str == '\0' ) {
		last = nullptr;
		return nullptr;
	}

	char* token_end = strpbrk( str, delim );
	if ( token_end ) {
		*token_end = '\0';
		last = token_end + 1;
	} else {
		last = nullptr;
	}
	return str;
}

int isalnum( int c ) {
	return ( isalpha( c ) || isdigit( c ) );
}

int isalpha( int c ) {
	return ( ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) );
}

int iscntrl( int c ) {
	return ( c >= 0 && c < 32 ) || ( c == 127 );
}

int isdigit( int c ) {
	return ( c >= '0' && c <= '9' );
}

int isgraph( int c ) {
	return ( c > 32 && c < 127 );
}

int islower( int c ) {
	return ( c >= 'a' && c <= 'z' );
}

int isprint( int c ) {
	return ( c >= 32 && c < 127 );
}

int ispunct( int c ) {
	return ( isprint( c ) && !isalnum( c ) && !isspace( c ) );
}

int isspace( int c ) {
	return ( c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r' );
}

int isupper( int c ) {
	return ( c >= 'A' && c <= 'Z' );
}

int isxdigit( int c ) {
	return ( ( c >= '0' && c <= '9' ) || ( c >= 'A' && c <= 'F' ) || ( c >= 'a' && c <= 'f' ) );
}

int tolower( int c ) {
	if ( isupper( c ) ) return c + 32;
	return c;
}

int toupper( int c ) {
	if ( islower( c ) ) return c - 32;
	return c;
}

int atoi( const char* str ) {
	int res = 0;
	int sign = 1;
	while ( isspace( *str ) ) ++str;
	if ( *str == '-' ) {
		sign = -1;
		++str;
	} else if ( *str == '+' ) {
		++str;
	}
	while ( isdigit( *str ) ) {
		res = res * 10 + ( *str - '0' );
		++str;
	}
	return sign * res;
}

long atol( const char* str ) {
	long res = 0;
	int sign = 1;
	while ( isspace( *str ) ) ++str;
	if ( *str == '-' ) {
		sign = -1;
		++str;
	} else if ( *str == '+' ) {
		++str;
	}
	while ( isdigit( *str ) ) {
		res = res * 10 + ( *str - '0' );
		++str;
	}
	return sign * res;
}

long long atoll( const char* str ) {
	long long res = 0;
	int sign = 1;
	while ( isspace( *str ) ) ++str;
	if ( *str == '-' ) {
		sign = -1;
		++str;
	} else if ( *str == '+' ) {
		++str;
	}
	while ( isdigit( *str ) ) {
		res = res * 10 + ( *str - '0' );
		++str;
	}
	return sign * res;
}

static char* reverse( char* str, int length ) {
	int start = 0;
	int end = length - 1;
	while ( start < end ) {
		char temp = str[ start ];
		str[ start ] = str[ end ];
		str[ end ] = temp;
		start++;
		end--;
	}
	return str;
}

char* itoa( int num, char* str, int base ) {
	int i = 0;
	bool isNegative = false;
	if ( num == 0 ) {
		str[ i++ ] = '0';
		str[ i ] = '\0';
		return str;
	}
	if ( num < 0 && base == 10 ) {
		isNegative = true;
		num = -num;
	}
	while ( num != 0 ) {
		int rem = num % base;
		str[ i++ ] = ( rem > 9 ) ? ( rem - 10 ) + 'a' : rem + '0';
		num = num / base;
	}
	if ( isNegative ) str[ i++ ] = '-';
	str[ i ] = '\0';
	return reverse( str, i );
}

char* ltoa( long num, char* str, int base ) {
	int i = 0;
	bool isNegative = false;
	if ( num == 0 ) {
		str[ i++ ] = '0';
		str[ i ] = '\0';
		return str;
	}
	if ( num < 0 && base == 10 ) {
		isNegative = true;
		num = -num;
	}
	while ( num != 0 ) {
		int rem = num % base;
		str[ i++ ] = ( rem > 9 ) ? ( rem - 10 ) + 'a' : rem + '0';
		num = num / base;
	}
	if ( isNegative ) str[ i++ ] = '-';
	str[ i ] = '\0';
	return reverse( str, i );
}

char* lltoa( long long num, char* str, int base ) {
	int i = 0;
	bool isNegative = false;
	if ( num == 0 ) {
		str[ i++ ] = '0';
		str[ i ] = '\0';
		return str;
	}
	if ( num < 0 && base == 10 ) {
		isNegative = true;
		num = -num;
	}
	while ( num != 0 ) {
		int rem = num % base;
		str[ i++ ] = ( rem > 9 ) ? ( rem - 10 ) + 'a' : rem + '0';
		num = num / base;
	}
	if ( isNegative ) str[ i++ ] = '-';
	str[ i ] = '\0';
	return reverse( str, i );
}

long strtol( const char* str, char** endptr, int base ) {
	if ( base < 0 || base == 1 || base > 36 ) {
		return 0;
	}

	const char* start = str;
	while ( isspace( *str ) ) {
		++str;
	}

	int sign = 1;
	if ( *str == '-' ) {
		sign = -1;
		++str;
	} else if ( *str == '+' ) {
		++str;
	}

	long result = 0;
	long limit = sign == -1 ? LONG_MIN : LONG_MAX;
	long overflow_threshold = limit / base;

	bool valid = false;
	while ( *str ) {
		int digit = 0;
		if ( isdigit( *str ) ) {
			digit = *str - '0';
		} else if ( isalpha( *str ) ) {
			digit = tolower( *str ) - 'a' + 10;
		} else {
			break;
		}

		if ( digit >= base ) {
			break;
		}

		if ( result < overflow_threshold || ( result == overflow_threshold && digit > limit % base ) ) {
			result = sign == -1 ? LONG_MIN : LONG_MAX;
			valid = true;
			break;
		}

		result = result * base + digit;
		valid = true;
		++str;
	}

	if ( !valid ) {
		str = start;
		result = 0;
	}

	if ( endptr ) {
		*endptr = (char*)( str );
	}

	return sign * result;
}

long long strtoll( const char* str, char** endptr, int base ) {
	if ( base < 0 || base == 1 || base > 36 ) {
		return 0;
	}

	const char* start = str;
	while ( isspace( *str ) ) {
		++str;
	}

	int sign = 1;
	if ( *str == '-' ) {
		sign = -1;
		++str;
	} else if ( *str == '+' ) {
		++str;
	}

	long long result = 0;
	long long limit = sign == -1 ? LLONG_MIN : LLONG_MAX;
	long long overflow_threshold = limit / base;

	bool valid = false;
	while ( *str ) {
		int digit = 0;
		if ( isdigit( *str ) ) {
			digit = *str - '0';
		} else if ( isalpha( *str ) ) {
			digit = tolower( *str ) - 'a' + 10;
		} else {
			break;
		}

		if ( digit >= base ) {
			break;
		}

		if ( result < overflow_threshold || ( result == overflow_threshold && digit > limit % base ) ) {
			result = sign == -1 ? LLONG_MIN : LLONG_MAX;
			valid = true;
			break;
		}

		result = result * base + digit;
		valid = true;
		++str;
	}

	if ( !valid ) {
		str = start;
		result = 0;
	}

	if ( endptr ) {
		*endptr = (char*)( str );
	}

	return sign * result;
}