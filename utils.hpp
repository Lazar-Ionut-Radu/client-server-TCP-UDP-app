#ifndef UTILS_HPP
#define UTILS_HPP

#include <vector>
#include <stdint.h>

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

std::vector <char *> tokenize(char *buff);

#endif // UTILS_HPP
